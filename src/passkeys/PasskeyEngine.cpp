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

#include "PasskeyEngine.h"

#include "BrowserPasskeys.h"
#include "BrowserPasskeysClient.h"
#include "PasskeyEncoding.h"
#include "PasskeyErrors.h"
#include "PasskeyUtils.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/EntryAttributes.h"
#include "core/Group.h"

#include <QGlobalStatic>
#include <QSet>
#include <QUuid>
#include <algorithm>

Q_GLOBAL_STATIC(PasskeyEngine, s_passkeyEngine);

PasskeyEngine::PasskeyEngine(QObject* parent)
    : QObject(parent)
{
}

PasskeyEngine* PasskeyEngine::instance()
{
    return s_passkeyEngine;
}

QJsonObject PasskeyEngine::errorObject(int errorCode)
{
    return QJsonObject({{"errorCode", errorCode}});
}

PasskeyCeremonyResult PasskeyEngine::prepareRegistration(const QJsonObject& publicKeyOptions,
                                                         const QString& origin) const
{
    PasskeyCeremonyResult result;
    QJsonObject credentialCreationOptions;
    const auto pkOptionsResult =
        browserPasskeysClient()->getCredentialCreationOptions(publicKeyOptions, origin, &credentialCreationOptions);
    if (pkOptionsResult > 0 || credentialCreationOptions.isEmpty()) {
        result.errorCode = pkOptionsResult > 0 ? pkOptionsResult : ERROR_PASSKEYS_UNKNOWN_ERROR;
        return result;
    }

    result.response = credentialCreationOptions;
    return result;
}

PasskeyCeremonyResult PasskeyEngine::completeRegistration(const QJsonObject& credentialCreationOptions) const
{
    PasskeyCeremonyResult result;
    const auto publicKeyCredentials = browserPasskeys()->buildRegisterPublicKeyCredential(credentialCreationOptions);
    if (publicKeyCredentials.credentialId.isEmpty() || publicKeyCredentials.key.isEmpty()
        || publicKeyCredentials.response.isEmpty()) {
        result.errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
        return result;
    }

    result.response = publicKeyCredentials.response;
    result.response["credentialId"] = publicKeyCredentials.credentialId;
    result.response["_privateKeyPem"] = QString::fromUtf8(publicKeyCredentials.key);
    return result;
}

PasskeyCeremonyResult PasskeyEngine::prepareAssertion(const QJsonObject& publicKeyOptions, const QString& origin) const
{
    PasskeyCeremonyResult result;
    QJsonObject assertionOptions;
    const auto assertionResult = browserPasskeysClient()->getAssertionOptions(publicKeyOptions, origin, &assertionOptions);
    if (assertionResult > 0 || assertionOptions.isEmpty()) {
        result.errorCode = assertionResult > 0 ? assertionResult : ERROR_PASSKEYS_UNKNOWN_ERROR;
        return result;
    }

    result.response = assertionOptions;
    return result;
}

PasskeyCeremonyResult PasskeyEngine::completeAssertion(const QJsonObject& assertionOptions, Entry* entry) const
{
    PasskeyCeremonyResult result;
    if (!entry || !entry->hasPasskey()) {
        result.errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
        return result;
    }

    const auto privateKeyPem = entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM);
    const auto credentialId = passkeyUtils()->getCredentialIdFromEntry(entry);
    const auto userHandle = entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_USER_HANDLE);

    const auto beFlag = entry->attributes()->hasKey(EntryAttributes::KPEX_PASSKEY_FLAG_BE)
                            ? entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_FLAG_BE) == "1"
                                  || entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_FLAG_BE) == "true"
                            : DEFAULT_BE_FLAG;
    const auto bsFlag = entry->attributes()->hasKey(EntryAttributes::KPEX_PASSKEY_FLAG_BS)
                            ? entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_FLAG_BS) == "1"
                                  || entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_FLAG_BS) == "true"
                            : DEFAULT_BS_FLAG;

    auto publicKeyCredential = browserPasskeys()->buildGetPublicKeyCredential(
        assertionOptions, credentialId, userHandle, privateKeyPem, beFlag, bsFlag);
    if (publicKeyCredential.isEmpty()) {
        result.errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
        return result;
    }

    result.response = publicKeyCredential;
    result.entry = entry;
    return result;
}

void PasskeyEngine::storePasskeyOnEntry(Entry* entry,
                                        const QString& rpId,
                                        const QString& rpName,
                                        const QString& username,
                                        const QString& credentialId,
                                        const QString& userHandle,
                                        const QString& privateKeyPem) const
{
    Q_UNUSED(rpName)
    Q_ASSERT(entry);
    if (!entry) {
        return;
    }

    entry->beginUpdate();
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_USERNAME, username);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_CREDENTIAL_ID, credentialId, true);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM, privateKeyPem, true);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY, rpId);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_USER_HANDLE, userHandle, true);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_FLAG_BE, "1");
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_FLAG_BS, "1");
    entry->addTag(tr("Passkey"));
    entry->endUpdate();
}

Entry* PasskeyEngine::createPasskeyEntry(Group* group,
                                         const QString& url,
                                         const QString& rpId,
                                         const QString& rpName,
                                         const QString& username,
                                         const QString& credentialId,
                                         const QString& userHandle,
                                         const QString& privateKeyPem) const
{
    if (!group) {
        return nullptr;
    }

    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setGroup(group);
    entry->setTitle(tr("%1 (Passkey)").arg(rpName));
    entry->setUsername(username);
    entry->setUrl(url);
    storePasskeyOnEntry(entry, rpId, rpName, username, credentialId, userHandle, privateKeyPem);
    entry->removeHistoryItems(entry->historyItems());
    return entry;
}

QList<Entry*> PasskeyEngine::findPasskeysForRp(const QSharedPointer<Database>& db, const QString& rpId) const
{
    QList<Entry*> entries;
    if (!db || !db->rootGroup()) {
        return entries;
    }

    for (Entry* entry : db->rootGroup()->entriesRecursive()) {
        if (entry->isRecycled()) {
            continue;
        }
        if (entry->hasPasskey() && entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY) == rpId) {
            entries << entry;
        }
    }
    return entries;
}

QList<Entry*> PasskeyEngine::findPasskeysWithUserHandle(const QSharedPointer<Database>& db,
                                                        const QString& rpId,
                                                        const QString& userHandle) const
{
    QList<Entry*> entries;
    for (Entry* entry : findPasskeysForRp(db, rpId)) {
        if (entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_USER_HANDLE) == userHandle) {
            entries << entry;
        }
    }
    return entries;
}

QList<Entry*> PasskeyEngine::findAllowedAssertionEntries(const QSharedPointer<Database>& db,
                                                         const QJsonObject& assertionOptions,
                                                         const QString& rpId) const
{
        QList<Entry*> entries;
    const auto allowedCredentials = passkeyUtils()->getAllowedCredentialsFromAssertionOptions(assertionOptions);
    if (!assertionOptions["allowCredentials"].toArray().isEmpty() && allowedCredentials.isEmpty()) {
        return {};
    }

    QSet<QByteArray> allowedRaw;
    for (const auto& id : allowedCredentials) {
        const auto raw = passkeyEncoding()->getArrayFromBase64(id);
        if (!raw.isEmpty()) {
            allowedRaw.insert(raw);
        }
                allowedRaw.insert(id.toUtf8());
    }

    for (Entry* entry : findPasskeysForRp(db, rpId)) {
        const auto entryId = passkeyUtils()->getCredentialIdFromEntry(entry);
        const auto entryRaw = passkeyEncoding()->getArrayFromBase64(entryId);

        const bool allowListHit = !allowedCredentials.isEmpty()
                                  && (allowedCredentials.contains(entryId) || allowedRaw.contains(entryRaw)
                                      || allowedRaw.contains(entryId.toUtf8())
                                      || (!entryRaw.isEmpty()
                                          && allowedCredentials.contains(passkeyEncoding()->getBase64FromArray(entryRaw))));
        const bool discoverableHit =
            allowedCredentials.isEmpty() && entry->attributes()->hasKey(EntryAttributes::KPEX_PASSKEY_USER_HANDLE);
        if (allowListHit || discoverableHit) {
            entries << entry;
        }
    }
    return entries;
}

bool PasskeyEngine::isCredentialExcluded(const QSharedPointer<Database>& db,
                                         const QJsonArray& excludeCredentials,
                                         const QString& rpId) const
{
    QStringList allIds;
    for (const auto& cred : excludeCredentials) {
        allIds << cred.toObject().value("id").toString();
    }

    const auto passkeyEntries = findPasskeysForRp(db, rpId);
    return std::any_of(passkeyEntries.begin(), passkeyEntries.end(), [&](const auto& entry) {
        return allIds.contains(passkeyUtils()->getCredentialIdFromEntry(entry));
    });
}

QList<PasskeyCredentialMetadata> PasskeyEngine::listAllPasskeyMetadata(const QSharedPointer<Database>& db) const
{
    QList<PasskeyCredentialMetadata> result;
    if (!db || !db->rootGroup()) {
        return result;
    }

    for (Entry* entry : db->rootGroup()->entriesRecursive()) {
        if (entry->isRecycled() || !entry->hasPasskey()) {
            continue;
        }
        PasskeyCredentialMetadata meta;
        meta.credentialId = passkeyUtils()->getCredentialIdFromEntry(entry);
        meta.rpId = entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY);
        meta.username = passkeyUtils()->getUsernameFromEntry(entry);
        meta.userHandle = entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_USER_HANDLE);
        meta.entryUuid = entry->uuid().toString(QUuid::WithoutBraces);
        result << meta;
    }
    return result;
}
