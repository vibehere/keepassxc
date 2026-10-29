/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_PLUGINREGISTRATIONMANAGER_H
#define KEEPASSXC_PLUGINREGISTRATIONMANAGER_H

#include "WebAuthnPluginApi.h"
#include "passkeys/PasskeyEngine.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

class PluginRegistrationManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginRegistrationManager(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    static PluginRegistrationManager* instance();

    bool loadApis();
    bool registerAuthenticator();
    bool unregisterAuthenticator();
    bool isRegistered() const;
    bool isRunningPackaged() const;

    bool publishCredentials(const QList<PasskeyCredentialMetadata>& metas);
    bool removeAllCredentials();
    bool performUserVerification(HWND hwnd, const wchar_t* message, BOOL* verified) const;

    QByteArray operationSigningPubKey() const;
    HRESULT lastError() const;
    QString lastErrorString() const;

private:
    Q_DISABLE_COPY(PluginRegistrationManager)

    QByteArray buildAuthenticatorGetInfoCbor() const;
    void writeDiag(const char* api) const;

    bool m_loaded = false;
    bool m_registered = false;
    HRESULT m_lastError = S_OK;
    HMODULE m_webauthn = nullptr;
    QByteArray m_opSignPubKey;
    QByteArray m_authenticatorInfo;

    PFN_WebAuthNPluginAddAuthenticator m_add = nullptr;
    PFN_WebAuthNPluginAddAuthenticator2 m_add2 = nullptr;
    PFN_WebAuthNPluginRemoveAuthenticator m_remove = nullptr;
    PFN_WebAuthNPluginGetAuthenticatorState m_getState = nullptr;
    PFN_WebAuthNPluginAuthenticatorAddCredentials m_addCreds = nullptr;
    PFN_WebAuthNPluginAuthenticatorRemoveAllCredentials m_removeAllCreds = nullptr;
    PFN_WebAuthNPluginPerformUserVerification m_uv = nullptr;
    PFN_WebAuthNPluginFreeAddAuthenticatorResponse m_freeResponse = nullptr;
};

#endif
