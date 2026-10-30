/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  Neutral identity for the OS CTAP virtual authenticator.
 *  Do not use product branding here — browsers surface HID name / AAGUID / provider name.
 */

#ifndef KEEPASSXC_OSPASSKEYIDS_H
#define KEEPASSXC_OSPASSKEYIDS_H

#include <QByteArray>
#include <QString>

namespace OsPasskeyIds
{
// Unregistered, stable AAGUID (not the public KeePassXC MDS id).
inline QString aaguidHex()
{
    return QStringLiteral("c4e8a91f3b7d42e6a0f15d8c9e2b4a70");
}

inline QByteArray aaguidBytes()
{
    return QByteArray::fromHex(aaguidHex().toLatin1());
}

// Shown by Chrome/Firefox as the security-key label on Linux.
inline const char* hidProductName()
{
    return "Security Key";
}

inline const char* hidSerial()
{
    return "fido2-sk";
}

#if defined(_WIN32)
// Shown in the Windows WebAuthn / passkey account picker.
inline const wchar_t* windowsProviderName()
{
    return L"Passkeys";
}
#endif
}

#endif
