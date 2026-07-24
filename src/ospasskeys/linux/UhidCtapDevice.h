/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  Virtual FIDO2 HID authenticator via Linux /dev/uhid.
 */

#ifndef KEEPASSXC_UHIDCTAPDEVICE_H
#define KEEPASSXC_UHIDCTAPDEVICE_H

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>
#include <atomic>
#include <functional>

class UhidCtapDevice : public QObject
{
    Q_OBJECT

public:
    using CborHandler = std::function<QByteArray(quint8 ctapCmd, const QByteArray& cborPayload)>;

    explicit UhidCtapDevice(QObject* parent = nullptr);
    ~UhidCtapDevice() override;

    bool start(CborHandler handler);
    void stop();
    bool isRunning() const;
    QString lastError() const;

private:
    struct ChannelMsg
    {
        quint8 cmd = 0;
        int expected = 0;
        int seq = 0;
        QByteArray buf;
    };

    struct PendingCbor
    {
        quint32 cid = 0;
        quint8 ctapCmd = 0;
        QByteArray cbor;
    };

    void readerLoop();
    void handleHidPacket(const QByteArray& packet);
    void dispatchCompleteMessage(quint32 cid, quint8 cmd, const QByteArray& payload);
    void enqueueCbor(quint32 cid, quint8 ctapCmd, const QByteArray& cbor);
    void scheduleProcessQueue();
    void processQueue();
    void sendHidResponse(quint32 cid, quint8 cmd, const QByteArray& payload);
    void writeUhidOutput(const QByteArray& report);

    int m_fd = -1;
    std::atomic<bool> m_stop{false};
    QThread* m_thread = nullptr;
    CborHandler m_handler;
    QString m_lastError;
    QMutex m_writeMutex;
    QMutex m_channelMutex;

    quint32 m_broadcastCid = 0xffffffff;
    QHash<quint32, ChannelMsg> m_channels;

    QMutex m_queueMutex;
    QQueue<PendingCbor> m_cborQueue;
    bool m_processScheduled = false;
    bool m_processing = false;
};

#endif
