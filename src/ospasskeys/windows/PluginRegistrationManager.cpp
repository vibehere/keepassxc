/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "PluginRegistrationManager.h"
#include "PluginAuthenticatorIds.h"
#include "passkeys/PasskeyEncoding.h"

#include <QDateTime>
#include <QFile>
#include <QGlobalStatic>
#include <QStandardPaths>
#include <QTextStream>
#include <QVector>

#include <appmodel.h>
#include <string>

Q_GLOBAL_STATIC(PluginRegistrationManager, s_pluginRegistrationManager);

static const wchar_t kLogoSvg[] =
    L"PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAxNiAxNiI+"
    L"PHJlY3Qgd2lkdGg9IjE2IiBoZWlnaHQ9IjE2IiBmaWxsPSIjMTk3NkQyIi8+PC9zdmc+";

PluginRegistrationManager* PluginRegistrationManager::instance()
{
    return s_pluginRegistrationManager;
}

bool PluginRegistrationManager::isRunningPackaged() const
{
    UINT32 length = 0;
    const LONG rc = GetCurrentPackageFullName(&length, nullptr);
    return rc == ERROR_INSUFFICIENT_BUFFER;
}

HRESULT PluginRegistrationManager::lastError() const
{
    return m_lastError;
}

QString PluginRegistrationManager::lastErrorString() const
{
    if (SUCCEEDED(m_lastError)) {
        return {};
    }
    QString name;
    switch (static_cast<quint32>(m_lastError)) {
    case 0x80090027:
        name = QStringLiteral("NTE_INVALID_PARAMETER");
        break;
    case 0x8009000F:
        name = QStringLiteral("NTE_EXISTS / not packaged");
        break;
    case 0x80090011:
        name = QStringLiteral("NTE_NOT_FOUND");
        break;
    case 0x80004001:
        name = QStringLiteral("E_NOTIMPL");
        break;
    default:
        break;
    }
    if (name.isEmpty()) {
        return QStringLiteral("HRESULT 0x%1").arg(static_cast<quint32>(m_lastError), 8, 16, QLatin1Char('0'));
    }
    return QStringLiteral("HRESULT 0x%1 (%2)")
        .arg(static_cast<quint32>(m_lastError), 8, 16, QLatin1Char('0'))
        .arg(name);
}

QByteArray PluginRegistrationManager::buildAuthenticatorGetInfoCbor() const
{
    
    
    
    
    static const char kPrefixHex[] =
        "A60182684649444F5F325F30684649444F5F325F310282637072666B686D61632D7365637265740350";
    static const char kSuffixHex[] =
        "04A362726BF5627570F5627576F5098168696E7465726E616C0A81A263616C672664747970656A7075626C69632D6B6579";

    QByteArray hex = QByteArray(kPrefixHex) + QByteArray(KEEPASSXC_PASSKEY_AAGUID).toUpper() + QByteArray(kSuffixHex);
    return QByteArray::fromHex(hex);
}

void PluginRegistrationManager::writeDiag(const char* api) const
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + QStringLiteral("/ospasskeys-last-error.txt");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }
    QTextStream out(&f);
    out << "time=" << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
    out << "packaged=" << (isRunningPackaged() ? "1" : "0") << '\n';
    out << "api=" << api << '\n';
    out << "hr=0x" << QString::number(static_cast<quint32>(m_lastError), 16) << '\n';
    out << "ok=" << (SUCCEEDED(m_lastError) ? "1" : "0") << '\n';
    out << "infoBytes=" << m_authenticatorInfo.size() << '\n';
}

bool PluginRegistrationManager::loadApis()
{
    if (m_loaded) {
        return m_add != nullptr || m_add2 != nullptr;
    }
    m_loaded = true;

    m_webauthn = LoadLibraryW(L"webauthn.dll");
    if (!m_webauthn) {
        m_lastError = HRESULT_FROM_WIN32(GetLastError());
        return false;
    }

    m_add = reinterpret_cast<PFN_WebAuthNPluginAddAuthenticator>(
        GetProcAddress(m_webauthn, "WebAuthNPluginAddAuthenticator"));
    m_add2 = reinterpret_cast<PFN_WebAuthNPluginAddAuthenticator2>(
        GetProcAddress(m_webauthn, "WebAuthNPluginAddAuthenticator2"));
    m_remove = reinterpret_cast<PFN_WebAuthNPluginRemoveAuthenticator>(
        GetProcAddress(m_webauthn, "WebAuthNPluginRemoveAuthenticator"));
    m_getState = reinterpret_cast<PFN_WebAuthNPluginGetAuthenticatorState>(
        GetProcAddress(m_webauthn, "WebAuthNPluginGetAuthenticatorState"));
    m_addCreds = reinterpret_cast<PFN_WebAuthNPluginAuthenticatorAddCredentials>(
        GetProcAddress(m_webauthn, "WebAuthNPluginAuthenticatorAddCredentials"));
    m_removeAllCreds = reinterpret_cast<PFN_WebAuthNPluginAuthenticatorRemoveAllCredentials>(
        GetProcAddress(m_webauthn, "WebAuthNPluginAuthenticatorRemoveAllCredentials"));
    m_uv = reinterpret_cast<PFN_WebAuthNPluginPerformUserVerification>(
        GetProcAddress(m_webauthn, "WebAuthNPluginPerformUserVerification"));
    m_freeResponse = reinterpret_cast<PFN_WebAuthNPluginFreeAddAuthenticatorResponse>(
        GetProcAddress(m_webauthn, "WebAuthNPluginFreeAddAuthenticatorResponse"));

    if (!m_add && !m_add2) {
        m_lastError = E_NOTIMPL;
        return false;
    }
    if (!m_remove) {
        m_lastError = E_NOTIMPL;
        return false;
    }
    return true;
}

bool PluginRegistrationManager::registerAuthenticator()
{
    if (!loadApis()) {
        return false;
    }

    if (!isRunningPackaged()) {
        m_lastError = static_cast<HRESULT>(0x8009000F);
        writeDiag("unpackaged");
        return false;
    }

    m_authenticatorInfo = buildAuthenticatorGetInfoCbor();

    CLSID clsid = CLSID_KeePassXCPluginAuthenticator;
    WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_RESPONSE* response = nullptr;

    
    if (m_add) {
        WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_OPTIONS options{};
        options.pwszAuthenticatorName = KEEPASSXC_PASSKEY_PROVIDER_NAME;
        options.pClsid = &clsid;
        options.pwszPluginRpId = L"keepassxc.org";
        options.pwszLightThemeLogoSvg = kLogoSvg;
        options.pwszDarkThemeLogoSvg = kLogoSvg;
        options.cbAuthenticatorInfo = static_cast<DWORD>(m_authenticatorInfo.size());
        options.pbAuthenticatorInfo = reinterpret_cast<const BYTE*>(m_authenticatorInfo.constData());
        options.cSupportedRpIds = 0;
        options.ppwszSupportedRpIds = nullptr;

        m_lastError = m_add(&options, &response);
        writeDiag("WebAuthNPluginAddAuthenticator");
    } else {
        m_lastError = E_NOTIMPL;
    }

    if (FAILED(m_lastError) && m_add2) {
        response = nullptr;
        WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_OPTIONS_2 options{};
        options.pwszAuthenticatorName = KEEPASSXC_PASSKEY_PROVIDER_NAME;
        options.pClsid = &clsid;
        options.pwszPluginRpId = L"keepassxc.org";
        options.pwszLightThemeLogoSvg = kLogoSvg;
        options.pwszDarkThemeLogoSvg = kLogoSvg;
        options.cbAuthenticatorInfo = static_cast<DWORD>(m_authenticatorInfo.size());
        options.pbAuthenticatorInfo = reinterpret_cast<const BYTE*>(m_authenticatorInfo.constData());
        options.cSupportedRpIds = 0;
        options.ppwszSupportedRpIds = nullptr;
        options.pwszUserVerificationKeyName = nullptr;

        m_lastError = m_add2(&options, &response);
        writeDiag("WebAuthNPluginAddAuthenticator2");
    }

    if (FAILED(m_lastError)) {
        return false;
    }

    if (response && response->pbOpSignPubKey && response->cbOpSignPubKey > 0) {
        m_opSignPubKey = QByteArray(reinterpret_cast<const char*>(response->pbOpSignPubKey),
                                    static_cast<int>(response->cbOpSignPubKey));
    }
    if (m_freeResponse && response) {
        m_freeResponse(response);
    }

    if (m_getState) {
        AUTHENTICATOR_STATE state = AuthenticatorState_Disabled;
        const HRESULT stateHr = m_getState(clsid, &state);
        if (FAILED(stateHr)) {
            m_lastError = stateHr;
            m_registered = false;
            writeDiag("GetAuthenticatorState");
            return false;
        }
    }

    m_registered = true;
    m_lastError = S_OK;
    writeDiag("ok");
    return true;
}

bool PluginRegistrationManager::unregisterAuthenticator()
{
    if (!m_remove) {
        loadApis();
    }
    if (!m_remove) {
        return false;
    }

    m_lastError = m_remove(CLSID_KeePassXCPluginAuthenticator);
    m_registered = false;
    m_opSignPubKey.clear();
    removeAllCredentials();
    return SUCCEEDED(m_lastError);
}

bool PluginRegistrationManager::isRegistered() const
{
    return m_registered;
}

bool PluginRegistrationManager::publishCredentials(const QList<PasskeyCredentialMetadata>& metas)
{
    if (!m_addCreds) {
        return false;
    }

    removeAllCredentials();
    if (metas.isEmpty()) {
        return true;
    }

    QList<QByteArray> idStorage;
    QList<QByteArray> userStorage;
    QList<std::wstring> rpStorage;
    QList<std::wstring> userNameStorage;
    QVector<WEBAUTHN_PLUGIN_CREDENTIAL_DETAILS> details;
    details.reserve(metas.size());

    for (const auto& meta : metas) {
        idStorage.append(passkeyEncoding()->getArrayFromBase64(meta.credentialId));
        userStorage.append(passkeyEncoding()->getArrayFromBase64(meta.userHandle));
        rpStorage.push_back(meta.rpId.toStdWString());
        userNameStorage.push_back(meta.username.toStdWString());

        WEBAUTHN_PLUGIN_CREDENTIAL_DETAILS d = {};
        d.cbCredentialId = static_cast<DWORD>(idStorage.last().size());
        d.pbCredentialId = reinterpret_cast<const BYTE*>(idStorage.last().constData());
        d.pwszRpId = rpStorage.last().c_str();
        d.pwszRpName = rpStorage.last().c_str();
        d.cbUserId = static_cast<DWORD>(userStorage.last().size());
        d.pbUserId = reinterpret_cast<const BYTE*>(userStorage.last().constData());
        d.pwszUserName = userNameStorage.last().c_str();
        d.pwszUserDisplayName = userNameStorage.last().c_str();
        details.append(d);
    }

    m_lastError =
        m_addCreds(CLSID_KeePassXCPluginAuthenticator, static_cast<DWORD>(details.size()), details.constData());
    return SUCCEEDED(m_lastError);
}

bool PluginRegistrationManager::removeAllCredentials()
{
    if (!m_removeAllCreds) {
        return false;
    }
    return SUCCEEDED(m_removeAllCreds(CLSID_KeePassXCPluginAuthenticator));
}

bool PluginRegistrationManager::performUserVerification(HWND hwnd, const wchar_t* message, BOOL* verified) const
{
    if (!m_uv || !verified) {
        return false;
    }
    return SUCCEEDED(m_uv(hwnd, message, verified));
}

QByteArray PluginRegistrationManager::operationSigningPubKey() const
{
    return m_opSignPubKey;
}
