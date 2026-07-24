/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "OsPasskeyBootstrap.h"
#include "OsPasskeyService.h"

void OsPasskeyBootstrap::start(DatabaseTabWidget* tabWidget)
{
    osPasskeyService()->setDatabaseTabWidget(tabWidget);
    osPasskeyService()->initialize();
}

void OsPasskeyBootstrap::stop()
{
    osPasskeyService()->shutdown();
}
