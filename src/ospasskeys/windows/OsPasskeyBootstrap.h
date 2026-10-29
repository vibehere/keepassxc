/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_OSPASSKEYBOOTSTRAP_H
#define KEEPASSXC_OSPASSKEYBOOTSTRAP_H

#include <windows.h>

#include <QObject>

class DatabaseTabWidget;

class OsPasskeyBootstrap : public QObject
{
    Q_OBJECT

public:
    static void start(DatabaseTabWidget* tabWidget);
    static void stop();

private:
    static DWORD s_classObjectCookie;
};

#endif
