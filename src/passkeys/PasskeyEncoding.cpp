/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PasskeyEncoding.h"
#include "PasskeyErrors.h"
#include "crypto/Random.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QGlobalStatic>

Q_GLOBAL_STATIC(PasskeyEncoding, s_passkeyEncoding);

PasskeyEncoding* PasskeyEncoding::instance()
{
    return s_passkeyEncoding;
}

QString PasskeyEncoding::getRandomBytesAsBase64(int bytes) const
{
    if (bytes == 0) {
        return {};
    }

    return getBase64FromArray(randomGen()->randomArray(bytes));
}

QString PasskeyEncoding::getBase64FromArray(const char* arr, int len) const
{
    if (len < 1) {
        return {};
    }

    auto data = QByteArray::fromRawData(arr, len);
    return getBase64FromArray(data);
}

QString PasskeyEncoding::getBase64FromArray(const QByteArray& byteArray) const
{
    if (byteArray.length() < 1) {
        return {};
    }

    return byteArray.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString PasskeyEncoding::getBase64FromJson(const QJsonObject& jsonObject) const
{
    if (jsonObject.isEmpty()) {
        return {};
    }

    const auto dataArray = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
    return getBase64FromArray(dataArray);
}

QByteArray PasskeyEncoding::getArrayFromHexString(const QString& hexString) const
{
    return QByteArray::fromHex(hexString.toUtf8());
}

QByteArray PasskeyEncoding::getArrayFromBase64(const QString& base64str) const
{
    return QByteArray::fromBase64(base64str.toUtf8(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray PasskeyEncoding::getSha256Hash(const QString& str) const
{
    return QCryptographicHash::hash(str.toUtf8(), QCryptographicHash::Sha256);
}

QString PasskeyEncoding::getSha256HashAsBase64(const QString& str) const
{
    return getBase64FromArray(QCryptographicHash::hash(str.toUtf8(), QCryptographicHash::Sha256));
}

QByteArray PasskeyEncoding::getQByteArray(const uchar* array, const uint len) const
{
    QByteArray qba;
    qba.reserve(len);
    for (uint i = 0; i < len; ++i) {
        qba.append(static_cast<char>(array[i]));
    }
    return qba;
}

QString PasskeyEncoding::getErrorMessage(int errorCode) const
{
    switch (errorCode) {
    case ERROR_KEEPASS_DATABASE_NOT_OPENED:
        return QObject::tr("Database not opened.");
    case ERROR_KEEPASS_NO_LOGINS_FOUND:
        return QObject::tr("No logins found.");
    case ERROR_PASSKEYS_ATTESTATION_NOT_SUPPORTED:
        return QObject::tr("Attestation not supported.");
    case ERROR_PASSKEYS_CREDENTIAL_IS_EXCLUDED:
        return QObject::tr("Credential is excluded.");
    case ERROR_PASSKEYS_REQUEST_CANCELED:
        return QObject::tr("Passkey request canceled.");
    case ERROR_PASSKEYS_INVALID_USER_VERIFICATION:
        return QObject::tr("Invalid user verification.");
    case ERROR_PASSKEYS_EMPTY_PUBLIC_KEY:
        return QObject::tr("Empty public key.");
    case ERROR_PASSKEYS_INVALID_URL_PROVIDED:
        return QObject::tr("Invalid URL provided.");
    case ERROR_PASSKEYS_ORIGIN_NOT_ALLOWED:
        return QObject::tr("Origin not allowed.");
    case ERROR_PASSKEYS_DOMAIN_IS_NOT_VALID:
        return QObject::tr("Domain is not valid.");
    case ERROR_PASSKEYS_DOMAIN_RPID_MISMATCH:
        return QObject::tr("Domain / RP ID mismatch.");
    case ERROR_PASSKEYS_NO_SUPPORTED_ALGORITHMS:
        return QObject::tr("No supported algorithms.");
    case ERROR_PASSKEYS_WAIT_FOR_LIFETIMER:
        return QObject::tr("Wait for life timer.");
    case ERROR_PASSKEYS_INVALID_CHALLENGE:
        return QObject::tr("Invalid challenge.");
    case ERROR_PASSKEYS_INVALID_USER_ID:
        return QObject::tr("Invalid user ID.");
    case ERROR_PASSKEYS_UNKNOWN_ERROR:
    default:
        return QObject::tr("Unknown passkey error.");
    }
}
