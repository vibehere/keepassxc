/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_OSPASSKEYCEREMONY_H
#define KEEPASSXC_OSPASSKEYCEREMONY_H

#include <QByteArray>
#include <QSharedPointer>

class Database;
class DatabaseTabWidget;

namespace OsPasskeyCeremony
{
QSharedPointer<Database> unlockedDatabase(DatabaseTabWidget* tabWidget);

QByteArray handleRegister(DatabaseTabWidget* tabWidget, const QByteArray& ctapRequest, int* errorCode);
QByteArray handleAssert(DatabaseTabWidget* tabWidget, const QByteArray& ctapRequest, int* errorCode);
}

#endif
