/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "OsPasskeyBootstrap.h"
#include "OsPasskeyService.h"
#include "PluginAuthenticatorImpl.h"

#include <objbase.h>

DWORD OsPasskeyBootstrap::s_classObjectCookie = 0;

void OsPasskeyBootstrap::start(DatabaseTabWidget* tabWidget)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RegisterPluginAuthenticatorClassObject(&s_classObjectCookie);

    osPasskeyService()->setDatabaseTabWidget(tabWidget);
    osPasskeyService()->initialize();
}

void OsPasskeyBootstrap::stop()
{
    osPasskeyService()->shutdown();
    if (s_classObjectCookie != 0) {
        RevokePluginAuthenticatorClassObject(s_classObjectCookie);
        s_classObjectCookie = 0;
    }
}
