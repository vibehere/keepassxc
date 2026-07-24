/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "OsPasskeyService.h"
#include "UhidCtapDevice.h"

#include "ospasskeys/CtapCodec.h"
#include "ospasskeys/OsPasskeyCeremony.h"
#include "core/Config.h"
#include "passkeys/PasskeyErrors.h"

#include <QApplication>
#include <QGlobalStatic>
#include <QMetaObject>
#include <QThread>

Q_GLOBAL_STATIC(OsPasskeyService, s_osPasskeyService);

namespace
{
constexpr quint8 kCtapMakeCredential = 0x01;
constexpr quint8 kCtapGetAssertion = 0x02;
constexpr quint8 kCtapGetInfo = 0x04;
constexpr quint8 kCtapOk = 0x00;
constexpr quint8 kCtapErrOperationDenied = 0x27;
constexpr quint8 kCtapErrNoCredentials = 0x2e;
constexpr quint8 kCtapErrOther = 0x7f;

QByteArray statusOnly(quint8 status)
{
    return QByteArray(1, char(status));
}

QByteArray wrapOk(const QByteArray& cbor)
{
    QByteArray out;
    out.append(char(kCtapOk));
    out.append(cbor);
    return out;
}

quint8 mapPasskeyError(int code)
{
    switch (code) {
    case PASSKEY_SUCCESS:
        return kCtapOk;
    case ERROR_PASSKEYS_REQUEST_CANCELED:
    case ERROR_KEEPASS_DATABASE_NOT_OPENED:
        return kCtapErrOperationDenied;
    case ERROR_KEEPASS_NO_LOGINS_FOUND:
        return kCtapErrNoCredentials;
    default:
        return kCtapErrOther;
    }
}
} 

OsPasskeyService::OsPasskeyService(QObject* parent)
    : QObject(parent)
{
}

OsPasskeyService* OsPasskeyService::instance()
{
    return s_osPasskeyService;
}

void OsPasskeyService::setDatabaseTabWidget(DatabaseTabWidget* tabWidget)
{
    m_tabWidget = tabWidget;
}

void OsPasskeyService::initialize()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    if (config()->get(Config::OsPasskeys_Enabled).toBool()) {
        startDevice();
    }
}

void OsPasskeyService::shutdown()
{
    stopDevice();
    m_initialized = false;
}

bool OsPasskeyService::isEnabled() const
{
    return config()->get(Config::OsPasskeys_Enabled).toBool();
}

bool OsPasskeyService::isDeviceActive() const
{
    return m_device && m_device->isRunning();
}

QString OsPasskeyService::lastError() const
{
    return m_lastError;
}

bool OsPasskeyService::setEnabled(bool enabled)
{
    config()->set(Config::OsPasskeys_Enabled, enabled);
    bool ok = true;
    if (enabled) {
        ok = startDevice();
    } else {
        stopDevice();
    }
    emit providerStateChanged(enabled && ok);
    return ok;
}

bool OsPasskeyService::startDevice()
{
    stopDevice();
    m_device = std::make_unique<UhidCtapDevice>();
    const bool ok = m_device->start([this](quint8 cmd, const QByteArray& cbor) { return handleCtap(cmd, cbor); });
    if (!ok) {
        m_lastError = m_device->lastError();
        m_device.reset();
        return false;
    }
    m_lastError.clear();
    return true;
}

void OsPasskeyService::stopDevice()
{
    if (m_device) {
        m_device->stop();
        m_device.reset();
    }
}

QByteArray OsPasskeyService::handleCtap(quint8 cmd, const QByteArray& cbor)
{
    if (!isEnabled()) {
        return statusOnly(kCtapErrOperationDenied);
    }

    if (cmd == kCtapGetInfo) {
        return wrapOk(CtapCodec::authenticatorGetInfoCbor());
    }

    if (cmd != kCtapMakeCredential && cmd != kCtapGetAssertion) {
        return statusOnly(kCtapErrOther);
    }

    auto run = [this, cmd, cbor]() -> QByteArray {
        int errorCode = 0;
        QByteArray encoded;
        if (cmd == kCtapMakeCredential) {
            encoded = OsPasskeyCeremony::handleRegister(m_tabWidget, cbor, &errorCode);
        } else {
            encoded = OsPasskeyCeremony::handleAssert(m_tabWidget, cbor, &errorCode);
        }
        if (errorCode != PASSKEY_SUCCESS || encoded.isEmpty()) {
            return statusOnly(mapPasskeyError(errorCode));
        }
        return wrapOk(encoded);
    };

    if (QThread::currentThread() == qApp->thread()) {
        return run();
    }

    QByteArray out;
    QMetaObject::invokeMethod(
        this,
        [run, &out]() { out = run(); },
        Qt::BlockingQueuedConnection);
    return out;
}
