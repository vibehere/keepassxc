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

#ifndef KEEPASSXC_PASSKEYERRORS_H
#define KEEPASSXC_PASSKEYERRORS_H

enum PasskeyErrorCode
{
    PASSKEY_SUCCESS = 0,
    ERROR_KEEPASS_DATABASE_NOT_OPENED = 1,
    ERROR_KEEPASS_NO_LOGINS_FOUND = 15,
    ERROR_PASSKEYS_ATTESTATION_NOT_SUPPORTED = 20,
    ERROR_PASSKEYS_CREDENTIAL_IS_EXCLUDED = 21,
    ERROR_PASSKEYS_REQUEST_CANCELED = 22,
    ERROR_PASSKEYS_INVALID_USER_VERIFICATION = 23,
    ERROR_PASSKEYS_EMPTY_PUBLIC_KEY = 24,
    ERROR_PASSKEYS_INVALID_URL_PROVIDED = 25,
    ERROR_PASSKEYS_ORIGIN_NOT_ALLOWED = 26,
    ERROR_PASSKEYS_DOMAIN_IS_NOT_VALID = 27,
    ERROR_PASSKEYS_DOMAIN_RPID_MISMATCH = 28,
    ERROR_PASSKEYS_NO_SUPPORTED_ALGORITHMS = 29,
    ERROR_PASSKEYS_WAIT_FOR_LIFETIMER = 30,
    ERROR_PASSKEYS_UNKNOWN_ERROR = 31,
    ERROR_PASSKEYS_INVALID_CHALLENGE = 32,
    ERROR_PASSKEYS_INVALID_USER_ID = 33,
};

#endif // KEEPASSXC_PASSKEYERRORS_H
