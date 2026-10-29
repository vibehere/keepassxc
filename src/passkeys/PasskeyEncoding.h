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

#ifndef KEEPASSXC_PASSKEYENCODING_H
#define KEEPASSXC_PASSKEYENCODING_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class PasskeyEncoding
{
public:
    static PasskeyEncoding* instance();

    QString getRandomBytesAsBase64(int bytes) const;
    QString getBase64FromArray(const char* arr, int len) const;
    QString getBase64FromArray(const QByteArray& byteArray) const;
    QString getBase64FromJson(const QJsonObject& jsonObject) const;
    QByteArray getArrayFromHexString(const QString& hexString) const;
    QByteArray getArrayFromBase64(const QString& base64str) const;
    QByteArray getSha256Hash(const QString& str) const;
    QString getSha256HashAsBase64(const QString& str) const;
    QByteArray getQByteArray(const uchar* array, const uint len) const;
    QString getErrorMessage(int errorCode) const;
};

static inline PasskeyEncoding* passkeyEncoding()
{
    return PasskeyEncoding::instance();
}

#endif // KEEPASSXC_PASSKEYENCODING_H
