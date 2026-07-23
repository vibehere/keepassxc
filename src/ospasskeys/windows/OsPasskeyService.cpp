/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "OsPasskeyService.h"

#include "PluginRegistrationManager.h"
#include "config-keepassx.h"
#include "core/Config.h"
#include "core/Database.h"
#include "gui/DatabaseTabWidget.h"
#include "gui/DatabaseWidget.h"
#include "ospasskeys/OsPasskeyCeremony.h"
#include "passkeys/PasskeyEngine.h"
#include "passkeys/PasskeyErrors.h"

#include <QApplication>
#include <QGlobalStatic>
#include <QMetaObject>
#include <QThread>

Q_GLOBAL_STATIC(OsPasskeyService, s_osPasskeyService);

OsPasskeyService* OsPasskeyService::instance()
{
    return s_osPasskeyService;
}

void OsPasskeyService::setDatabaseTabWidget(DatabaseTabWidget* tabWidget)
{
    m_tabWidget = tabWidget;
    if (!m_tabWidget) {
        return;
    }

    connect(m_tabWidget, &DatabaseTabWidget::databaseUnlocked, this, &OsPasskeyService::handleDatabaseUnlocked);
    connect(m_tabWidget, &DatabaseTabWidget::databaseLocked, this, &OsPasskeyService::handleDatabaseLocked);
}

void OsPasskeyService::initialize()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    if (config()->get(Config::OsPasskeys_Enabled).toBool()) {
        PluginRegistrationManager::instance()->registerAuthenticator();
        syncCredentialMetadata();
    }
}

void OsPasskeyService::shutdown()
{
    if (config()->get(Config::OsPasskeys_Enabled).toBool()) {
        PluginRegistrationManager::instance()->unregisterAuthenticator();
    }
    m_initialized = false;
}

bool OsPasskeyService::isEnabled() const
{
    return config()->get(Config::OsPasskeys_Enabled).toBool();
}

bool OsPasskeyService::setEnabled(bool enabled)
{
    config()->set(Config::OsPasskeys_Enabled, enabled);
    bool ok = false;
    if (enabled) {
        ok = PluginRegistrationManager::instance()->registerAuthenticator();
        if (ok) {
            syncCredentialMetadata();
        }
    } else {
        ok = PluginRegistrationManager::instance()->unregisterAuthenticator();
    }
    emit providerStateChanged(enabled && ok);
    return ok;
}

bool OsPasskeyService::isDatabaseUnlocked() const
{
    return !unlockedDatabase().isNull();
}

QSharedPointer<Database> OsPasskeyService::unlockedDatabase() const
{
    return OsPasskeyCeremony::unlockedDatabase(m_tabWidget);
}

void OsPasskeyService::cancelCurrentOperation(const QUuid& transactionId)
{
    QMutexLocker lock(&m_cancelMutex);
    if (m_activeTransaction == transactionId || transactionId.isNull()) {
        m_cancelRequested = true;
    }
}

HRESULT OsPasskeyService::makeCredential(HWND hwnd, const QByteArray& ctapRequest, QByteArray* ctapResponse)
{
    return runCeremonyOnUiThread(true, hwnd, ctapRequest, ctapResponse);
}

HRESULT OsPasskeyService::getAssertion(HWND hwnd, const QByteArray& ctapRequest, QByteArray* ctapResponse)
{
    return runCeremonyOnUiThread(false, hwnd, ctapRequest, ctapResponse);
}

HRESULT OsPasskeyService::runCeremonyOnUiThread(bool registerMode,
                                                HWND hwnd,
                                                const QByteArray& request,
                                                QByteArray* response)
{
    Q_UNUSED(hwnd)
    if (!isEnabled()) {
        return E_FAIL;
    }
    if (!response) {
        return E_POINTER;
    }

    auto run = [this, registerMode, request](int* errorCode) -> QByteArray {
        return registerMode ? OsPasskeyCeremony::handleRegister(m_tabWidget, request, errorCode)
                            : OsPasskeyCeremony::handleAssert(m_tabWidget, request, errorCode);
    };

    if (QThread::currentThread() == qApp->thread()) {
        int errorCode = 0;
        *response = run(&errorCode);
        if (errorCode == PASSKEY_SUCCESS) {
            syncCredentialMetadata();
        }
        return errorCode == PASSKEY_SUCCESS ? S_OK : E_ABORT;
    }

    HRESULT hr = E_FAIL;
    QByteArray out;
    QMetaObject::invokeMethod(
        this,
        [this, run, &out, &hr]() {
            int errorCode = 0;
            out = run(&errorCode);
            if (errorCode == PASSKEY_SUCCESS) {
                syncCredentialMetadata();
            }
            hr = (errorCode == PASSKEY_SUCCESS) ? S_OK : E_ABORT;
        },
        Qt::BlockingQueuedConnection);

    *response = out;
    return hr;
}

bool OsPasskeyService::performUserVerification(HWND hwnd) const
{
    auto* reg = PluginRegistrationManager::instance();
    BOOL verified = FALSE;
    if (reg->performUserVerification(hwnd, L"Verify to use KeePassXC passkey", &verified)) {
        return verified == TRUE;
    }
    return true;
}

void OsPasskeyService::syncCredentialMetadata()
{
    if (!isEnabled()) {
        return;
    }

    auto db = unlockedDatabase();
    if (!db) {
        PluginRegistrationManager::instance()->removeAllCredentials();
        return;
    }

    const auto metas = passkeyEngine()->listAllPasskeyMetadata(db);
    PluginRegistrationManager::instance()->publishCredentials(metas);
}

void OsPasskeyService::handleDatabaseUnlocked()
{
    syncCredentialMetadata();
}

void OsPasskeyService::handleDatabaseLocked()
{
    if (isEnabled()) {
        PluginRegistrationManager::instance()->removeAllCredentials();
    }
}
