/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "CtapCodec.h"
#include "passkeys/PasskeyEncoding.h"

#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QJsonArray>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <webauthn.h>
#include <webauthnplugin.h>
#endif

namespace
{
#ifdef Q_OS_WIN
using PFN_WebAuthNEncodeGetAssertionResponse = HRESULT(WINAPI*)(PCWEBAUTHN_CTAPCBOR_GET_ASSERTION_RESPONSE,
                                                                DWORD*,
                                                                BYTE**);
using PFN_WebAuthNEncodeMakeCredentialResponse = HRESULT(WINAPI*)(PCWEBAUTHN_CREDENTIAL_ATTESTATION, DWORD*, BYTE**);
#endif

QByteArray ecdsaP1363ToDer(const QByteArray& p1363)
{
    if (p1363.size() != 64) {
        return p1363;
    }

    auto encodeInt = [](QByteArray half) {
        int i = 0;
        while (i < half.size() - 1 && static_cast<unsigned char>(half.at(i)) == 0) {
            ++i;
        }
        half = half.mid(i);
        if (static_cast<unsigned char>(half.at(0)) & 0x80) {
            half.prepend('\0');
        }
        QByteArray out;
        out.append(char(0x02));
        out.append(char(half.size()));
        out.append(half);
        return out;
    };

    const QByteArray body = encodeInt(p1363.left(32)) + encodeInt(p1363.mid(32));
    QByteArray der;
    der.append(char(0x30));
    der.append(char(body.size()));
    der.append(body);
    return der;
}

QByteArray cborBytes(const QCborValue& value)
{
    if (value.isByteArray()) {
        return value.toByteArray();
    }
    if (value.isString()) {
        return passkeyEncoding()->getArrayFromBase64(value.toString());
    }
    return {};
}

QString cborText(const QCborValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isByteArray()) {
        return QString::fromUtf8(value.toByteArray());
    }
    return {};
}

QCborMap asMap(const QCborValue& value)
{
    return value.isMap() ? value.toMap() : QCborMap();
}

QByteArray extractAuthDataFromAttestationObject(const QByteArray& attestationObject)
{
    const QCborMap att = asMap(QCborValue::fromCbor(attestationObject));
    QByteArray authData = cborBytes(att.value(QStringLiteral("authData")));
    if (authData.isEmpty()) {
        authData = cborBytes(att.value(2));
    }
    return authData;
}

#ifdef Q_OS_WIN
template <typename Fn>
Fn loadWebAuthnFn(const char* name)
{
    HMODULE dll = GetModuleHandleW(L"webauthn.dll");
    if (!dll) {
        dll = LoadLibraryW(L"webauthn.dll");
    }
    if (!dll) {
        return nullptr;
    }
    return reinterpret_cast<Fn>(GetProcAddress(dll, name));
}
#endif
} 

bool CtapCodec::decodeMakeCredential(const QByteArray& ctapCbor,
                                      QJsonObject* publicKeyOptions,
                                      QString* origin,
                                      QString* error)
{
    if (!publicKeyOptions || ctapCbor.isEmpty()) {
        if (error) {
            *error = QStringLiteral("empty request");
        }
        return false;
    }

    QCborValue root = QCborValue::fromCbor(ctapCbor);
    QCborMap map;
    if (root.isArray() && root.toArray().size() >= 2) {
        map = asMap(root.toArray().at(1));
    } else if (root.isMap()) {
        map = root.toMap();
    } else {
        if (error) {
            *error = QStringLiteral("unsupported CTAP request shape");
        }
        return false;
    }

    const auto clientDataHash = cborBytes(map.value(1));
    const auto rp = asMap(map.value(2));
    const auto user = asMap(map.value(3));
    const auto pubKeyCredParams = map.value(4).toArray();

    const QString rpId = cborText(rp.value(QStringLiteral("id")));
    const QString rpName = cborText(rp.value(QStringLiteral("name")));
    const QString userName = cborText(user.value(QStringLiteral("name")));
    const QByteArray userId = cborBytes(user.value(QStringLiteral("id")));

    if (rpId.isEmpty() || clientDataHash.isEmpty()) {
        if (error) {
            *error = QStringLiteral("missing rpId or clientDataHash");
        }
        return false;
    }

    QJsonArray algParams;
    for (const auto& p : pubKeyCredParams) {
        const auto pm = asMap(p);
        qint64 alg = pm.value(QStringLiteral("alg")).toInteger(0);
        if (alg == 0) {
            alg = pm.value(2).toInteger(-7);
        }
        if (alg == 0) {
            alg = -7;
        }
        algParams.append(QJsonObject{{"type", "public-key"}, {"alg", alg}});
    }
    if (algParams.isEmpty()) {
        algParams.append(QJsonObject{{"type", "public-key"}, {"alg", -7}});
    }

    QJsonArray excludeCredentials;
    const auto excludeList = map.value(5).toArray();
    for (const auto& cred : excludeList) {
        const auto cm = asMap(cred);
        auto id = cborBytes(cm.value(QStringLiteral("id")));
        if (id.isEmpty()) {
            id = cborBytes(cm.value(2));
        }
        if (!id.isEmpty()) {
            excludeCredentials.append(
                QJsonObject{{"type", "public-key"}, {"id", passkeyEncoding()->getBase64FromArray(id)}});
        }
    }

    *publicKeyOptions = QJsonObject{
        {"challenge", passkeyEncoding()->getBase64FromArray(clientDataHash)},
        {"_osClientDataHash", passkeyEncoding()->getBase64FromArray(clientDataHash)},
        {"rp", QJsonObject{{"id", rpId}, {"name", rpName.isEmpty() ? rpId : rpName}}},
        {"user",
         QJsonObject{{"id", passkeyEncoding()->getBase64FromArray(userId)},
                     {"name", userName},
                     {"displayName", cborText(user.value(QStringLiteral("displayName")))}}},
        {"pubKeyCredParams", algParams},
        {"excludeCredentials", excludeCredentials},
        {"authenticatorSelection", QJsonObject{{"userVerification", "preferred"}, {"residentKey", "preferred"}}},
        {"attestation", "none"},
    };

    if (origin) {
        *origin = QStringLiteral("https://%1").arg(rpId);
    }
    return true;
}

bool CtapCodec::decodeGetAssertion(const QByteArray& ctapCbor,
                                   QJsonObject* publicKeyOptions,
                                   QString* origin,
                                   QString* error)
{
    if (!publicKeyOptions || ctapCbor.isEmpty()) {
        if (error) {
            *error = QStringLiteral("empty request");
        }
        return false;
    }

    QCborValue root = QCborValue::fromCbor(ctapCbor);
    QCborMap map;
    if (root.isArray() && root.toArray().size() >= 2) {
        map = asMap(root.toArray().at(1));
    } else if (root.isMap()) {
        map = root.toMap();
    } else {
        if (error) {
            *error = QStringLiteral("unsupported CTAP request shape");
        }
        return false;
    }

    const QString rpId = cborText(map.value(1));
    const auto clientDataHash = cborBytes(map.value(2));
    const auto allowList = map.value(3).toArray();

    if (rpId.isEmpty() || clientDataHash.isEmpty()) {
        if (error) {
            *error = QStringLiteral("missing rpId or clientDataHash");
        }
        return false;
    }

    QJsonArray allowCredentials;
    for (const auto& cred : allowList) {
        const auto cm = asMap(cred);
        
        auto id = cborBytes(cm.value(QStringLiteral("id")));
        if (id.isEmpty()) {
            id = cborBytes(cm.value(2));
        }
        if (!id.isEmpty()) {
            allowCredentials.append(QJsonObject{{"type", "public-key"},
                                                {"id", passkeyEncoding()->getBase64FromArray(id)},
                                                {"transports",
                                                 QJsonArray{QStringLiteral("internal"),
                                                            QStringLiteral("usb"),
                                                            QStringLiteral("hybrid")}}});
        }
    }

    *publicKeyOptions = QJsonObject{
        {"challenge", passkeyEncoding()->getBase64FromArray(clientDataHash)},
        {"_osClientDataHash", passkeyEncoding()->getBase64FromArray(clientDataHash)},
        {"rpId", rpId},
        {"allowCredentials", allowCredentials},
        {"userVerification", "preferred"},
    };

    if (origin) {
        *origin = QStringLiteral("https://%1").arg(rpId);
    }
    return true;
}

bool CtapCodec::getAssertionHasAllowList(const QByteArray& ctapCbor)
{
    return getAssertionAllowListSize(ctapCbor) > 0;
}

int CtapCodec::getAssertionAllowListSize(const QByteArray& ctapCbor)
{
    if (ctapCbor.isEmpty()) {
        return 0;
    }
    QCborValue root = QCborValue::fromCbor(ctapCbor);
    QCborMap map;
    if (root.isArray() && root.toArray().size() >= 2) {
        map = asMap(root.toArray().at(1));
    } else if (root.isMap()) {
        map = root.toMap();
    } else {
        return 0;
    }
    return map.value(3).toArray().size();
}

bool CtapCodec::getAssertionAllowListUsesStringKeys(const QByteArray& ctapCbor)
{
    if (ctapCbor.isEmpty()) {
        return true; 
    }
    QCborValue root = QCborValue::fromCbor(ctapCbor);
    QCborMap map;
    if (root.isArray() && root.toArray().size() >= 2) {
        map = asMap(root.toArray().at(1));
    } else if (root.isMap()) {
        map = root.toMap();
    } else {
        return true;
    }
    const auto allowList = map.value(3).toArray();
    if (allowList.isEmpty()) {
        return true;
    }
    const auto cm = asMap(allowList.at(0));
    
    return cm.contains(QStringLiteral("id")) || cm.contains(QStringLiteral("type"));
}

QString CtapCodec::getAssertionRpId(const QByteArray& ctapCbor)
{
    if (ctapCbor.isEmpty()) {
        return {};
    }
    QCborValue root = QCborValue::fromCbor(ctapCbor);
    QCborMap map;
    if (root.isArray() && root.toArray().size() >= 2) {
        map = asMap(root.toArray().at(1));
    } else if (root.isMap()) {
        map = root.toMap();
    } else {
        return {};
    }
    return cborText(map.value(1));
}

QByteArray CtapCodec::encodeMakeCredentialResponse(const QJsonObject& webauthnResponse)
{
    const auto response = webauthnResponse.value(QStringLiteral("response")).toObject();
    const auto attestationObject =
        passkeyEncoding()->getArrayFromBase64(response.value(QStringLiteral("attestationObject")).toString());
    const QByteArray authData = extractAuthDataFromAttestationObject(attestationObject);
    if (authData.isEmpty()) {
        return {};
    }

#ifdef Q_OS_WIN
    if (auto encode = loadWebAuthnFn<PFN_WebAuthNEncodeMakeCredentialResponse>("WebAuthNEncodeMakeCredentialResponse")) {
        WEBAUTHN_CREDENTIAL_ATTESTATION attestation{};
        attestation.dwVersion = WEBAUTHN_CREDENTIAL_ATTESTATION_CURRENT_VERSION;
        attestation.pwszFormatType = WEBAUTHN_ATTESTATION_TYPE_NONE;
        attestation.cbAuthenticatorData = static_cast<DWORD>(authData.size());
        attestation.pbAuthenticatorData = reinterpret_cast<PBYTE>(const_cast<char*>(authData.constData()));

        DWORD cb = 0;
        BYTE* pb = nullptr;
        if (SUCCEEDED(encode(&attestation, &cb, &pb)) && pb && cb > 0) {
            QByteArray out(reinterpret_cast<const char*>(pb), static_cast<int>(cb));
            CoTaskMemFree(pb);
            return out;
        }
    }
#endif

    QCborMap out;
    out.insert(1, QStringLiteral("none"));
    out.insert(2, authData);
    out.insert(3, QCborMap());
    return QCborValue(out).toCbor(QCborValue::SortKeysInMaps);
}

QByteArray CtapCodec::encodeGetAssertionResponse(const QJsonObject& webauthnResponse,
                                                 int allowListSize,
                                                 bool stringCredKeys)
{
    const auto response = webauthnResponse.value(QStringLiteral("response")).toObject();
    QByteArray id = passkeyEncoding()->getArrayFromBase64(webauthnResponse.value(QStringLiteral("id")).toString());
    QByteArray authData =
        passkeyEncoding()->getArrayFromBase64(response.value(QStringLiteral("authenticatorData")).toString());
    QByteArray signature =
        passkeyEncoding()->getArrayFromBase64(response.value(QStringLiteral("signature")).toString());
    if (signature.size() == 64) {
        signature = ecdsaP1363ToDer(signature);
    }
    QByteArray userHandle =
        passkeyEncoding()->getArrayFromBase64(response.value(QStringLiteral("userHandle")).toString());

    if (authData.isEmpty() || signature.isEmpty() || id.isEmpty()) {
        return {};
    }

#ifdef Q_OS_WIN
    if (auto encode = loadWebAuthnFn<PFN_WebAuthNEncodeGetAssertionResponse>("WebAuthNEncodeGetAssertionResponse")) {
        WEBAUTHN_ASSERTION assertion{};
        assertion.dwVersion = WEBAUTHN_ASSERTION_CURRENT_VERSION;
        assertion.cbAuthenticatorData = static_cast<DWORD>(authData.size());
        assertion.pbAuthenticatorData = reinterpret_cast<PBYTE>(authData.data());
        assertion.cbSignature = static_cast<DWORD>(signature.size());
        assertion.pbSignature = reinterpret_cast<PBYTE>(signature.data());
        assertion.Credential.dwVersion = WEBAUTHN_CREDENTIAL_CURRENT_VERSION;
        assertion.Credential.cbId = static_cast<DWORD>(id.size());
        assertion.Credential.pbId = reinterpret_cast<PBYTE>(id.data());
        assertion.Credential.pwszCredentialType = WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY;
        assertion.cbUserId = static_cast<DWORD>(userHandle.size());
        assertion.pbUserId = userHandle.isEmpty() ? nullptr : reinterpret_cast<PBYTE>(userHandle.data());

        WEBAUTHN_USER_ENTITY_INFORMATION userInfo{};
        userInfo.dwVersion = WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION;
        userInfo.cbId = assertion.cbUserId;
        userInfo.pbId = assertion.pbUserId;

        WEBAUTHN_CTAPCBOR_GET_ASSERTION_RESPONSE ctap{};
        ctap.WebAuthNAssertion = assertion;
        ctap.pUserInformation = userHandle.isEmpty() ? nullptr : &userInfo;
        ctap.dwNumberOfCredentials = 1;

        DWORD cb = 0;
        BYTE* pb = nullptr;
        if (SUCCEEDED(encode(&ctap, &cb, &pb)) && pb && cb > 0) {
            QByteArray out(reinterpret_cast<const char*>(pb), static_cast<int>(cb));
            CoTaskMemFree(pb);
            return out;
        }
    }
#endif

    
    
    
    
    QCborMap out;
    const bool omitCredential = (allowListSize == 1);
    if (!omitCredential) {
        QCborMap credential;
        if (stringCredKeys) {
            credential.insert(QStringLiteral("id"), id);
            credential.insert(QStringLiteral("type"), QStringLiteral("public-key"));
        } else {
            credential.insert(1, QStringLiteral("public-key"));
            credential.insert(2, id);
        }
        out.insert(1, credential);
    }
    out.insert(2, authData);
    out.insert(3, signature);
    if (!omitCredential && !userHandle.isEmpty()) {
        QCborMap user;
        if (stringCredKeys) {
            user.insert(QStringLiteral("id"), userHandle);
        } else {
            user.insert(1, userHandle);
        }
        out.insert(4, user);
    }
    out.insert(5, 1);
    return QCborValue(out).toCbor(QCborValue::SortKeysInMaps);
}

QByteArray CtapCodec::authenticatorGetInfoCbor()
{
    
    
    
    
    QCborMap info;
    info.insert(1, QCborArray({QStringLiteral("U2F_V2"), QStringLiteral("FIDO_2_0")}));
    info.insert(2, QCborArray()); 
    info.insert(3, QByteArray::fromHex("FDB141B25D84443E8A354698C205A502"));
    QCborMap options;
    options.insert(QStringLiteral("rk"), true);
    options.insert(QStringLiteral("up"), true);
    options.insert(QStringLiteral("uv"), true);
    options.insert(QStringLiteral("plat"), false);
    info.insert(4, options);
    info.insert(5, 1200); 
    info.insert(9, QCborArray({QStringLiteral("usb")}));
    QCborMap alg;
    alg.insert(QStringLiteral("alg"), -7);
    alg.insert(QStringLiteral("type"), QStringLiteral("public-key"));
    info.insert(10, QCborArray({alg}));
    return QCborValue(info).toCbor(QCborValue::SortKeysInMaps);
}
