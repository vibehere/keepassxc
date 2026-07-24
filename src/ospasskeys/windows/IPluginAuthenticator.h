/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  Minimal IPluginAuthenticator interface (from microsoft/webauthn pluginauthenticator.h).
 */

#pragma once

#include <windows.h>
#include <unknwn.h>

typedef enum _WEBAUTHN_PLUGIN_REQUEST_TYPE
{
    WEBAUTHN_PLUGIN_REQUEST_TYPE_CTAP2_CBOR = 0x1
} WEBAUTHN_PLUGIN_REQUEST_TYPE;

typedef struct _WEBAUTHN_PLUGIN_OPERATION_REQUEST
{
    HWND hWnd;
    GUID transactionId;
    DWORD cbRequestSignature;
    BYTE* pbRequestSignature;
    WEBAUTHN_PLUGIN_REQUEST_TYPE requestType;
    DWORD cbEncodedRequest;
    BYTE* pbEncodedRequest;
} WEBAUTHN_PLUGIN_OPERATION_REQUEST;

typedef struct _WEBAUTHN_PLUGIN_OPERATION_RESPONSE
{
    DWORD cbEncodedResponse;
    BYTE* pbEncodedResponse;
} WEBAUTHN_PLUGIN_OPERATION_RESPONSE;

typedef struct _WEBAUTHN_PLUGIN_CANCEL_OPERATION_REQUEST
{
    GUID transactionId;
    DWORD cbRequestSignature;
    BYTE* pbRequestSignature;
} WEBAUTHN_PLUGIN_CANCEL_OPERATION_REQUEST;

typedef enum _PLUGIN_LOCK_STATUS
{
    PluginLocked = 0,
    PluginUnlocked = 1
} PLUGIN_LOCK_STATUS;

MIDL_INTERFACE("d26bcf6f-b54c-43ff-9f06-d5bf148625f7")
IPluginAuthenticator : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE MakeCredential(const WEBAUTHN_PLUGIN_OPERATION_REQUEST* request,
                                                     WEBAUTHN_PLUGIN_OPERATION_RESPONSE* response) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAssertion(const WEBAUTHN_PLUGIN_OPERATION_REQUEST* request,
                                                   WEBAUTHN_PLUGIN_OPERATION_RESPONSE* response) = 0;
    virtual HRESULT STDMETHODCALLTYPE CancelOperation(const WEBAUTHN_PLUGIN_CANCEL_OPERATION_REQUEST* request) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLockStatus(PLUGIN_LOCK_STATUS* lockStatus) = 0;
};
