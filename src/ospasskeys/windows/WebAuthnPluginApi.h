/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  Minimal WebAuthn plugin API declarations (dynamically loaded from webauthn.dll).
 */

#pragma once

#include <windows.h>
#include <guiddef.h>

typedef enum _PLUGIN_AUTHENTICATOR_STATE
{
    AuthenticatorState_Disabled = 0,
    AuthenticatorState_Enabled
} AUTHENTICATOR_STATE;

typedef struct _WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_OPTIONS
{
    LPCWSTR pwszAuthenticatorName;
    const CLSID* pClsid;
    LPCWSTR pwszPluginRpId;
    LPCWSTR pwszLightThemeLogoSvg;
    LPCWSTR pwszDarkThemeLogoSvg;
    DWORD cbAuthenticatorInfo;
    const BYTE* pbAuthenticatorInfo;
    DWORD cSupportedRpIds;
    const LPCWSTR* ppwszSupportedRpIds;
} WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_OPTIONS;

typedef struct _WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_OPTIONS_2
{
    LPCWSTR pwszAuthenticatorName;
    const CLSID* pClsid;
    LPCWSTR pwszPluginRpId;
    LPCWSTR pwszLightThemeLogoSvg;
    LPCWSTR pwszDarkThemeLogoSvg;
    DWORD cbAuthenticatorInfo;
    const BYTE* pbAuthenticatorInfo;
    DWORD cSupportedRpIds;
    const LPCWSTR* ppwszSupportedRpIds;
    LPCWSTR pwszUserVerificationKeyName;
} WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_OPTIONS_2;

typedef struct _WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_RESPONSE
{
    DWORD cbOpSignPubKey;
    PBYTE pbOpSignPubKey;
} WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_RESPONSE;

typedef struct _WEBAUTHN_PLUGIN_CREDENTIAL_DETAILS
{
    DWORD cbCredentialId;
    const BYTE* pbCredentialId;
    LPCWSTR pwszRpId;
    LPCWSTR pwszRpName;
    DWORD cbUserId;
    const BYTE* pbUserId;
    LPCWSTR pwszUserName;
    LPCWSTR pwszUserDisplayName;
} WEBAUTHN_PLUGIN_CREDENTIAL_DETAILS;

typedef HRESULT(WINAPI* PFN_WebAuthNPluginAddAuthenticator)(
    const WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_OPTIONS* options,
    WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_RESPONSE** response);
typedef HRESULT(WINAPI* PFN_WebAuthNPluginAddAuthenticator2)(
    const WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_OPTIONS_2* options,
    WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_RESPONSE** response);
typedef HRESULT(WINAPI* PFN_WebAuthNPluginRemoveAuthenticator)(REFCLSID rclsid);
typedef HRESULT(WINAPI* PFN_WebAuthNPluginGetAuthenticatorState)(REFCLSID rclsid, AUTHENTICATOR_STATE* state);
typedef HRESULT(WINAPI* PFN_WebAuthNPluginAuthenticatorAddCredentials)(REFCLSID rclsid,
                                                                       DWORD cCredentialDetails,
                                                                       const WEBAUTHN_PLUGIN_CREDENTIAL_DETAILS* pCredentialDetails);
typedef HRESULT(WINAPI* PFN_WebAuthNPluginAuthenticatorRemoveAllCredentials)(REFCLSID rclsid);
typedef HRESULT(WINAPI* PFN_WebAuthNPluginPerformUserVerification)(HWND hWnd, LPCWSTR pwszMessage, BOOL* pbVerified);
typedef void(WINAPI* PFN_WebAuthNPluginFreeAddAuthenticatorResponse)(
    WEBAUTHN_PLUGIN_ADD_AUTHENTICATOR_RESPONSE* response);
