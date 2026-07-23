/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "UhidCtapDevice.h"

#include "ospasskeys/CtapCodec.h"

#include <QMetaObject>
#include <QMutex>
#include <QRandomGenerator>
#include <QThread>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uhid.h>
#include <poll.h>
#include <unistd.h>

namespace
{
constexpr int kReportSize = 64;
constexpr quint8 kTypeInit = 0x80;
constexpr quint8 kCmdPing = 0x01;
constexpr quint8 kCmdMsg = 0x03;
constexpr quint8 kCmdLock = 0x04;
constexpr quint8 kCmdInit = 0x06;
constexpr quint8 kCmdWink = 0x08;
constexpr quint8 kCmdCbor = 0x10;
constexpr quint8 kCmdCancel = 0x11;
constexpr quint8 kCmdError = 0x3f;
constexpr quint8 kErrInvalidCmd = 0x01;
constexpr quint8 kErrInvalidLen = 0x03;
constexpr quint8 kErrOther = 0x7f;
constexpr quint8 kCtapGetAssertion = 0x02;
constexpr quint8 kCtapErrNoCredentials = 0x2e;

const unsigned char kFidoReportDesc[] = {
    0x06, 0xd0, 0xf1, 0x09, 0x01, 0xa1, 0x01, 0x09, 0x20, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08,
    0x95, 0x40, 0x81, 0x02, 0x09, 0x21, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x40, 0x91,
    0x02, 0xc0};

quint32 readCid(const uchar* p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

void writeCid(uchar* p, quint32 cid)
{
    p[0] = uchar((cid >> 24) & 0xff);
    p[1] = uchar((cid >> 16) & 0xff);
    p[2] = uchar((cid >> 8) & 0xff);
    p[3] = uchar(cid & 0xff);
}
} 

UhidCtapDevice::UhidCtapDevice(QObject* parent)
    : QObject(parent)
{
}

UhidCtapDevice::~UhidCtapDevice()
{
    stop();
}

bool UhidCtapDevice::isRunning() const
{
    return m_fd >= 0 && m_thread && m_thread->isRunning();
}

QString UhidCtapDevice::lastError() const
{
    return m_lastError;
}

bool UhidCtapDevice::start(CborHandler handler)
{
    stop();
    m_handler = std::move(handler);
    m_lastError.clear();

    m_fd = ::open("/dev/uhid", O_RDWR | O_CLOEXEC);
    if (m_fd < 0) {
        m_lastError = QStringLiteral("Cannot open /dev/uhid (%1). Need uhid kernel module and write access "
                                     "(real Linux/VM; WSL often lacks this).")
                          .arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    struct uhid_event ev {};
    ev.type = UHID_CREATE2;
    auto& c = ev.u.create2;
    std::snprintf(reinterpret_cast<char*>(c.name), sizeof(c.name), "KeePassXC Passkeys");
    c.phys[0] = '\0';
    std::snprintf(reinterpret_cast<char*>(c.uniq), sizeof(c.uniq), "keepassxc-os-passkeys");
    c.rd_size = sizeof(kFidoReportDesc);
    std::memcpy(c.rd_data, kFidoReportDesc, sizeof(kFidoReportDesc));
    c.bus = BUS_USB;
    c.vendor = 0x1209;
    c.product = 0x5078;
    c.version = 0x0100;
    c.country = 0;

    if (::write(m_fd, &ev, sizeof(ev)) < 0) {
        m_lastError = QStringLiteral("UHID_CREATE2 failed: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    m_stop = false;
    m_thread = QThread::create([this]() { readerLoop(); });
    m_thread->setObjectName(QStringLiteral("UhidCtapReader"));
    m_thread->start();
    return true;
}

void UhidCtapDevice::stop()
{
    m_stop = true;
    if (m_thread) {
        m_thread->wait(3000);
        delete m_thread;
        m_thread = nullptr;
    }
    if (m_fd >= 0) {
        struct uhid_event ev {};
        ev.type = UHID_DESTROY;
        (void)::write(m_fd, &ev, sizeof(ev));
        ::close(m_fd);
        m_fd = -1;
    }
    {
        QMutexLocker lock(&m_channelMutex);
        m_channels.clear();
    }
    {
        QMutexLocker lock(&m_queueMutex);
        m_cborQueue.clear();
        m_processScheduled = false;
        m_processing = false;
    }
    m_handler = {};
}

void UhidCtapDevice::readerLoop()
{
    while (!m_stop && m_fd >= 0) {
        pollfd pfd{m_fd, POLLIN, 0};
        const int pr = ::poll(&pfd, 1, 250);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (pr == 0) {
            continue;
        }

        struct uhid_event ev {};
        const ssize_t n = ::read(m_fd, &ev, sizeof(ev));
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            continue;
        }

        if (ev.type == UHID_OUTPUT) {
            const auto size = qMin<int>(ev.u.output.size, UHID_DATA_MAX);
            QByteArray report(reinterpret_cast<const char*>(ev.u.output.data), size);
            if (report.size() == kReportSize + 1 && report.at(0) == char(0)) {
                report = report.mid(1);
            } else if (report.size() > kReportSize && report.at(0) == char(0)) {
                report = report.mid(1, kReportSize);
            } else if (report.size() > kReportSize) {
                report = report.left(kReportSize);
            } else if (report.size() < kReportSize && !report.isEmpty()) {
                report.append(QByteArray(kReportSize - report.size(), '\0'));
            }
            if (report.size() == kReportSize) {
                handleHidPacket(report);
            }
        }
    }
}

void UhidCtapDevice::writeUhidOutput(const QByteArray& report)
{
    if (m_fd < 0 || report.size() != kReportSize) {
        return;
    }
    QMutexLocker lock(&m_writeMutex);
    struct uhid_event ev {};
    ev.type = UHID_INPUT2;
    ev.u.input2.size = kReportSize;
    std::memcpy(ev.u.input2.data, report.constData(), kReportSize);
    (void)::write(m_fd, &ev, sizeof(ev));
}

void UhidCtapDevice::sendHidResponse(quint32 cid, quint8 cmd, const QByteArray& payload)
{
    
    QMutexLocker lock(&m_writeMutex);
    const int total = payload.size();
    QByteArray packet(kReportSize, '\0');
    auto* p = reinterpret_cast<uchar*>(packet.data());
    writeCid(p, cid);
    p[4] = cmd | kTypeInit;
    p[5] = uchar((total >> 8) & 0xff);
    p[6] = uchar(total & 0xff);

    const int firstLen = qMin(total, kReportSize - 7);
    if (firstLen > 0) {
        std::memcpy(p + 7, payload.constData(), firstLen);
    }
    {
        struct uhid_event ev {};
        ev.type = UHID_INPUT2;
        ev.u.input2.size = kReportSize;
        std::memcpy(ev.u.input2.data, packet.constData(), kReportSize);
        (void)::write(m_fd, &ev, sizeof(ev));
    }

    int offset = firstLen;
    quint8 seq = 0;
    while (offset < total) {
        
        QThread::usleep(8000);
        packet.fill('\0');
        p = reinterpret_cast<uchar*>(packet.data());
        writeCid(p, cid);
        p[4] = seq++;
        const int chunk = qMin(total - offset, kReportSize - 5);
        std::memcpy(p + 5, payload.constData() + offset, chunk);
        offset += chunk;
        struct uhid_event ev {};
        ev.type = UHID_INPUT2;
        ev.u.input2.size = kReportSize;
        std::memcpy(ev.u.input2.data, packet.constData(), kReportSize);
        (void)::write(m_fd, &ev, sizeof(ev));
    }
}

void UhidCtapDevice::handleHidPacket(const QByteArray& packet)
{
    if (packet.size() < 7) {
        return;
    }
    const auto* p = reinterpret_cast<const uchar*>(packet.constData());
    const quint32 cid = readCid(p);
    const quint8 cmdOrSeq = p[4];

    QByteArray completePayload;
    quint8 completeCmd = 0;
    bool complete = false;

    {
        QMutexLocker lock(&m_channelMutex);
        if (cmdOrSeq & kTypeInit) {
            const quint8 cmd = cmdOrSeq & ~kTypeInit;
            const int bcnt = (int(p[5]) << 8) | int(p[6]);
            ChannelMsg& msg = m_channels[cid];
            msg.cmd = cmd;
            msg.expected = bcnt;
            msg.seq = 0;
            msg.buf = QByteArray(reinterpret_cast<const char*>(p + 7), qMin(bcnt, kReportSize - 7));
            if (msg.buf.size() >= msg.expected) {
                msg.buf.truncate(msg.expected);
                completePayload = msg.buf;
                completeCmd = msg.cmd;
                m_channels.remove(cid);
                complete = true;
            }
        } else {
            auto it = m_channels.find(cid);
            if (it == m_channels.end() || it->expected <= 0) {
                return;
            }
            ChannelMsg& msg = it.value();
            if (cmdOrSeq != msg.seq) {
                m_channels.remove(cid);
                lock.unlock();
                sendHidResponse(cid, kCmdError, QByteArray(1, char(kErrInvalidLen)));
                return;
            }
            ++msg.seq;
            msg.buf.append(reinterpret_cast<const char*>(p + 5), kReportSize - 5);
            if (msg.buf.size() < msg.expected) {
                return;
            }
            msg.buf.truncate(msg.expected);
            completePayload = msg.buf;
            completeCmd = msg.cmd;
            m_channels.remove(cid);
            complete = true;
        }
    }

    if (complete) {
        dispatchCompleteMessage(cid, completeCmd, completePayload);
    }
}

void UhidCtapDevice::enqueueCbor(quint32 cid, quint8 ctapCmd, const QByteArray& cbor)
{
    {
        QMutexLocker lock(&m_queueMutex);
        m_cborQueue.enqueue(PendingCbor{cid, ctapCmd, cbor});
    }
    scheduleProcessQueue();
}

void UhidCtapDevice::scheduleProcessQueue()
{
    QMutexLocker lock(&m_queueMutex);
    if (m_processScheduled) {
        return;
    }
    m_processScheduled = true;
    lock.unlock();
    QMetaObject::invokeMethod(
        this,
        [this]() {
            {
                QMutexLocker ql(&m_queueMutex);
                m_processScheduled = false;
            }
            processQueue();
        },
        Qt::QueuedConnection);
}

void UhidCtapDevice::processQueue()
{
    for (;;) {
        QList<PendingCbor> batch;
        {
            QMutexLocker lock(&m_queueMutex);
            if (m_processing || m_cborQueue.isEmpty()) {
                return;
            }
            m_processing = true;
            batch.append(m_cborQueue.dequeue());
            if (batch.first().ctapCmd == kCtapGetAssertion) {
                
                lock.unlock();
                QThread::msleep(40);
                lock.relock();
                QQueue<PendingCbor> rest;
                while (!m_cborQueue.isEmpty()) {
                    const auto next = m_cborQueue.dequeue();
                    if (next.ctapCmd == kCtapGetAssertion) {
                        batch.append(next);
                    } else {
                        rest.enqueue(next);
                    }
                }
                m_cborQueue = rest;
            }
        }

        struct BatchResult
        {
            quint32 cid = 0;
            QByteArray ctapResp;
        };
        QList<BatchResult> results;
        results.reserve(batch.size());

        
        
        
        QList<bool> hasAllowList;
        hasAllowList.reserve(batch.size());
        if (batch.first().ctapCmd == kCtapGetAssertion) {
            for (const auto& req : batch) {
                hasAllowList.append(CtapCodec::getAssertionHasAllowList(req.cbor));
            }
        }

        for (int i = 0; i < batch.size(); ++i) {
            const auto& req = batch.at(i);
            BatchResult r;
            r.cid = req.cid;
            const bool suppressDiscoverable =
                req.ctapCmd == kCtapGetAssertion && i < hasAllowList.size() && !hasAllowList.at(i);
            if (suppressDiscoverable) {
                r.ctapResp = QByteArray(1, char(kCtapErrNoCredentials));
            } else if (m_handler) {
                r.ctapResp = m_handler(req.ctapCmd, req.cbor);
            }
            results.append(r);
        }

        
        
        auto isCtapOk = [](const QByteArray& resp) {
            return !resp.isEmpty() && static_cast<quint8>(resp.at(0)) == 0x00 && resp.size() > 1;
        };
        for (const auto& r : results) {
            if (!isCtapOk(r.ctapResp)) {
                continue;
            }
            sendHidResponse(r.cid, kCmdCbor, r.ctapResp);
        }
        for (const auto& r : results) {
            if (isCtapOk(r.ctapResp)) {
                continue;
            }
            if (r.ctapResp.isEmpty()) {
                sendHidResponse(r.cid, kCmdError, QByteArray(1, char(kErrOther)));
            } else {
                sendHidResponse(r.cid, kCmdCbor, r.ctapResp);
            }
        }

        {
            QMutexLocker lock(&m_queueMutex);
            m_processing = false;
            if (m_cborQueue.isEmpty()) {
                return;
            }
        }
    }
}

void UhidCtapDevice::dispatchCompleteMessage(quint32 cid, quint8 cmd, const QByteArray& payload)
{
    if (cmd == kCmdInit) {
        if (payload.size() < 8) {
            sendHidResponse(cid, kCmdError, QByteArray(1, char(kErrInvalidLen)));
            return;
        }
        quint32 newCid = cid;
        if (cid == m_broadcastCid) {
            do {
                newCid = QRandomGenerator::global()->generate();
            } while (newCid == 0 || newCid == m_broadcastCid);
        }
        QByteArray resp;
        resp.append(payload.left(8));
        uchar cidBytes[4];
        writeCid(cidBytes, newCid);
        resp.append(reinterpret_cast<const char*>(cidBytes), 4);
        resp.append(char(2));
        resp.append(char(1));
        resp.append(char(0));
        resp.append(char(0));
        resp.append(char(0x04));
        sendHidResponse(cid, kCmdInit, resp);
        return;
    }

    if (cmd == kCmdPing) {
        sendHidResponse(cid, kCmdPing, payload);
        return;
    }

    if (cmd == kCmdWink || cmd == kCmdLock) {
        sendHidResponse(cid, cmd, {});
        return;
    }

    if (cmd == kCmdCancel) {
        
        QMutexLocker lock(&m_queueMutex);
        QQueue<PendingCbor> filtered;
        while (!m_cborQueue.isEmpty()) {
            auto item = m_cborQueue.dequeue();
            if (item.cid != cid) {
                filtered.enqueue(item);
            }
        }
        m_cborQueue = filtered;
        lock.unlock();
        sendHidResponse(cid, cmd, {});
        return;
    }

    if (cmd == kCmdCbor || cmd == kCmdMsg) {
        if (!m_handler) {
            sendHidResponse(cid, kCmdError, QByteArray(1, char(kErrOther)));
            return;
        }
        if (payload.isEmpty()) {
            sendHidResponse(cid, kCmdError, QByteArray(1, char(kErrInvalidLen)));
            return;
        }
        enqueueCbor(cid, static_cast<quint8>(payload.at(0)), payload.mid(1));
        return;
    }

    Q_UNUSED(kErrInvalidCmd)
    sendHidResponse(cid, kCmdError, QByteArray(1, char(kErrInvalidCmd)));
}
