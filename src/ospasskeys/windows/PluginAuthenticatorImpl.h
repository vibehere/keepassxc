/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_PLUGINAUTHENTICATORIMPL_H
#define KEEPASSXC_PLUGINAUTHENTICATORIMPL_H

#include "IPluginAuthenticator.h"

#include <QByteArray>
#include <atomic>

class PluginAuthenticatorImpl : public IPluginAuthenticator
{
public:
    PluginAuthenticatorImpl();
    virtual ~PluginAuthenticatorImpl();

    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    
    HRESULT STDMETHODCALLTYPE MakeCredential(const WEBAUTHN_PLUGIN_OPERATION_REQUEST* request,
                                             WEBAUTHN_PLUGIN_OPERATION_RESPONSE* response) override;
    HRESULT STDMETHODCALLTYPE GetAssertion(const WEBAUTHN_PLUGIN_OPERATION_REQUEST* request,
                                           WEBAUTHN_PLUGIN_OPERATION_RESPONSE* response) override;
    HRESULT STDMETHODCALLTYPE CancelOperation(const WEBAUTHN_PLUGIN_CANCEL_OPERATION_REQUEST* request) override;
    HRESULT STDMETHODCALLTYPE GetLockStatus(PLUGIN_LOCK_STATUS* lockStatus) override;

private:
    HRESULT fillResponse(const QByteArray& data, WEBAUTHN_PLUGIN_OPERATION_RESPONSE* response);

    std::atomic<ULONG> m_ref{1};
};

class PluginAuthenticatorClassFactory : public IClassFactory
{
public:
    PluginAuthenticatorClassFactory() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override;
    HRESULT STDMETHODCALLTYPE LockServer(BOOL fLock) override;

private:
    std::atomic<ULONG> m_ref{1};
};

HRESULT RegisterPluginAuthenticatorClassObject(DWORD* cookie);
HRESULT RevokePluginAuthenticatorClassObject(DWORD cookie);

#endif
