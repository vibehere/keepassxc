/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "PluginAuthenticatorImpl.h"
#include "OsPasskeyService.h"
#include "PluginAuthenticatorIds.h"

#include <QByteArray>
#include <QUuid>
#include <cstring>
#include <new>
#include <objbase.h>

namespace
{
QUuid guidToUuid(const GUID& g)
{
    return QUuid(g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5],
                 g.Data4[6], g.Data4[7]);
}
} 

PluginAuthenticatorImpl::PluginAuthenticatorImpl() = default;

PluginAuthenticatorImpl::~PluginAuthenticatorImpl() = default;

HRESULT STDMETHODCALLTYPE PluginAuthenticatorImpl::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == __uuidof(IPluginAuthenticator)) {
        *ppvObject = static_cast<IPluginAuthenticator*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE PluginAuthenticatorImpl::AddRef()
{
    return ++m_ref;
}

ULONG STDMETHODCALLTYPE PluginAuthenticatorImpl::Release()
{
    const ULONG ref = --m_ref;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

HRESULT PluginAuthenticatorImpl::fillResponse(const QByteArray& data, WEBAUTHN_PLUGIN_OPERATION_RESPONSE* response)
{
    if (!response) {
        return E_POINTER;
    }
    if (data.isEmpty()) {
        return E_FAIL;
    }

    BYTE* buffer = static_cast<BYTE*>(CoTaskMemAlloc(data.size()));
    if (!buffer) {
        return E_OUTOFMEMORY;
    }
    memcpy(buffer, data.constData(), static_cast<size_t>(data.size()));
    response->pbEncodedResponse = buffer;
    response->cbEncodedResponse = static_cast<DWORD>(data.size());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PluginAuthenticatorImpl::MakeCredential(const WEBAUTHN_PLUGIN_OPERATION_REQUEST* request,
                                                                  WEBAUTHN_PLUGIN_OPERATION_RESPONSE* response)
{
    if (!request || !response) {
        return E_POINTER;
    }
    *response = {};

    QByteArray ctapRequest(reinterpret_cast<const char*>(request->pbEncodedRequest),
                           static_cast<int>(request->cbEncodedRequest));
    QByteArray ctapResponse;
    const HRESULT hr = osPasskeyService()->makeCredential(request->hWnd, ctapRequest, &ctapResponse);
    if (FAILED(hr)) {
        return hr;
    }
    return fillResponse(ctapResponse, response);
}

HRESULT STDMETHODCALLTYPE PluginAuthenticatorImpl::GetAssertion(const WEBAUTHN_PLUGIN_OPERATION_REQUEST* request,
                                                                WEBAUTHN_PLUGIN_OPERATION_RESPONSE* response)
{
    if (!request || !response) {
        return E_POINTER;
    }

    QByteArray ctapRequest(reinterpret_cast<const char*>(request->pbEncodedRequest),
                           static_cast<int>(request->cbEncodedRequest));
    QByteArray ctapResponse;
    const HRESULT hr = osPasskeyService()->getAssertion(request->hWnd, ctapRequest, &ctapResponse);
    if (FAILED(hr)) {
        return hr;
    }
    return fillResponse(ctapResponse, response);
}

HRESULT STDMETHODCALLTYPE
PluginAuthenticatorImpl::CancelOperation(const WEBAUTHN_PLUGIN_CANCEL_OPERATION_REQUEST* request)
{
    if (!request) {
        return E_POINTER;
    }
    osPasskeyService()->cancelCurrentOperation(guidToUuid(request->transactionId));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PluginAuthenticatorImpl::GetLockStatus(PLUGIN_LOCK_STATUS* lockStatus)
{
    if (!lockStatus) {
        return E_POINTER;
    }
    *lockStatus = osPasskeyService()->isDatabaseUnlocked() ? PluginUnlocked : PluginLocked;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PluginAuthenticatorClassFactory::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppvObject = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE PluginAuthenticatorClassFactory::AddRef()
{
    return ++m_ref;
}

ULONG STDMETHODCALLTYPE PluginAuthenticatorClassFactory::Release()
{
    const ULONG ref = --m_ref;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

HRESULT STDMETHODCALLTYPE PluginAuthenticatorClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject)
{
    if (pUnkOuter) {
        return CLASS_E_NOAGGREGATION;
    }
    auto* impl = new (std::nothrow) PluginAuthenticatorImpl();
    if (!impl) {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = impl->QueryInterface(riid, ppvObject);
    impl->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE PluginAuthenticatorClassFactory::LockServer(BOOL fLock)
{
    Q_UNUSED(fLock)
    return S_OK;
}

HRESULT RegisterPluginAuthenticatorClassObject(DWORD* cookie)
{
    if (!cookie) {
        return E_POINTER;
    }
    auto* factory = new (std::nothrow) PluginAuthenticatorClassFactory();
    if (!factory) {
        return E_OUTOFMEMORY;
    }

    const HRESULT hr = CoRegisterClassObject(CLSID_KeePassXCPluginAuthenticator,
                                             factory,
                                             CLSCTX_LOCAL_SERVER,
                                             REGCLS_MULTIPLEUSE,
                                             cookie);
    factory->Release();
    return hr;
}

HRESULT RevokePluginAuthenticatorClassObject(DWORD cookie)
{
    return CoRevokeClassObject(cookie);
}
