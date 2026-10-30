/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_OSPASSKEYCEREMONY_H
#define KEEPASSXC_OSPASSKEYCEREMONY_H

#include <QByteArray>
#include <QList>
#include <QSharedPointer>
#include <functional>

class Database;
class DatabaseTabWidget;

namespace OsPasskeyCeremony
{
/** Current tab only, and only if unlocked. Never silently picks another vault. */
QSharedPointer<Database> unlockedDatabase(DatabaseTabWidget* tabWidget);

/** Every unlocked open database (order matches tab order). */
QList<QSharedPointer<Database>> unlockedDatabases(DatabaseTabWidget* tabWidget);

using UserVerifyFn = std::function<bool()>;

QByteArray handleRegister(DatabaseTabWidget* tabWidget,
                          const QByteArray& ctapRequest,
                          int* errorCode,
                          const UserVerifyFn& verifyUser = {});
QByteArray handleAssert(DatabaseTabWidget* tabWidget,
                        const QByteArray& ctapRequest,
                        int* errorCode,
                        const UserVerifyFn& verifyUser = {});
}

#endif
