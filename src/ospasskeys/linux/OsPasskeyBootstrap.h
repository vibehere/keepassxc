/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_OSPASSKEYBOOTSTRAP_H
#define KEEPASSXC_OSPASSKEYBOOTSTRAP_H

class DatabaseTabWidget;

class OsPasskeyBootstrap
{
public:
    static void start(DatabaseTabWidget* tabWidget);
    static void stop();
};

#endif
