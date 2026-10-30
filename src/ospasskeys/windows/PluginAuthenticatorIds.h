/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#ifndef KEEPASSXC_PLUGINAUTHENTICATORIDS_H
#define KEEPASSXC_PLUGINAUTHENTICATORIDS_H

#include <guiddef.h>

static const CLSID CLSID_KeePassXCPluginAuthenticator = {
    0xa7c3e8f1, 0x2b4d, 0x4e9a, {0x9c, 0x1f, 0x8d, 0x6e, 0x5a, 0x4b, 0x3c, 0x2d}};

// Neutral identity (must match OsPasskeyIds / authenticatorGetInfo).
static const char* const KEEPASSXC_PASSKEY_AAGUID = "c4e8a91f3b7d42e6a0f15d8c9e2b4a70";

static const wchar_t* const KEEPASSXC_PASSKEY_PROVIDER_NAME = L"Passkeys";

#endif
