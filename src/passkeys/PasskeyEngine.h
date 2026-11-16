/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSXC_PASSKEYENGINE_H
#define KEEPASSXC_PASSKEYENGINE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QString>

class Database;
class Entry;
class Group;

struct PasskeyCredentialMetadata
{
    QString credentialId;
    QString rpId;
    QString username;
    QString userHandle;
    QString entryUuid;
};

struct PasskeyCeremonyResult
{
    int errorCode = 0;
    QJsonObject response;
    Entry* entry = nullptr;
};

class PasskeyEngine : public QObject
{
    Q_OBJECT

public:
    explicit PasskeyEngine(QObject* parent = nullptr);
    ~PasskeyEngine() override = default;
    static PasskeyEngine* instance();

    PasskeyCeremonyResult prepareRegistration(const QJsonObject& publicKeyOptions, const QString& origin) const;
    PasskeyCeremonyResult completeRegistration(const QJsonObject& credentialCreationOptions) const;

    PasskeyCeremonyResult prepareAssertion(const QJsonObject& publicKeyOptions, const QString& origin) const;
    PasskeyCeremonyResult completeAssertion(const QJsonObject& assertionOptions,
                                            Entry* entry) const;

    void storePasskeyOnEntry(Entry* entry,
                             const QString& rpId,
                             const QString& rpName,
                             const QString& username,
                             const QString& credentialId,
                             const QString& userHandle,
                             const QString& privateKeyPem) const;

    Entry* createPasskeyEntry(Group* group,
                              const QString& url,
                              const QString& rpId,
                              const QString& rpName,
                              const QString& username,
                              const QString& credentialId,
                              const QString& userHandle,
                              const QString& privateKeyPem) const;

    QList<Entry*> findPasskeysForRp(const QSharedPointer<Database>& db, const QString& rpId) const;
    QList<Entry*> findPasskeysWithUserHandle(const QSharedPointer<Database>& db,
                                             const QString& rpId,
                                             const QString& userHandle) const;
    QList<Entry*> findAllowedAssertionEntries(const QSharedPointer<Database>& db,
                                              const QJsonObject& assertionOptions,
                                              const QString& rpId) const;
    bool isCredentialExcluded(const QSharedPointer<Database>& db,
                              const QJsonArray& excludeCredentials,
                              const QString& rpId) const;

    QList<PasskeyCredentialMetadata> listAllPasskeyMetadata(const QSharedPointer<Database>& db) const;

    static QJsonObject errorObject(int errorCode);

private:
    Q_DISABLE_COPY(PasskeyEngine)
};

static inline PasskeyEngine* passkeyEngine()
{
    return PasskeyEngine::instance();
}

#endif // KEEPASSXC_PASSKEYENGINE_H
