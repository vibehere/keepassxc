/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "OsPasskeyCeremony.h"
#include "CtapCodec.h"

#include "core/Database.h"
#include "core/Group.h"
#include "gui/DatabaseTabWidget.h"
#include "gui/DatabaseWidget.h"
#include "gui/MainWindow.h"
#include "passkeys/BrowserPasskeysConfirmationDialog.h"
#include "passkeys/PasskeyEncoding.h"
#include "passkeys/PasskeyEngine.h"
#include "passkeys/PasskeyErrors.h"
#include "passkeys/PasskeyUtils.h"
#include "core/EntryAttributes.h"

#include <QCborMap>
#include <QCborValue>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QStandardPaths>
#include <QTextStream>
#include <QWidget>
#include <QtEndian>

namespace
{
void ceremonyLog(const QString& line)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/ospasskeys-ceremony.log"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QFile f2(QStringLiteral("/tmp/ospasskeys-ceremony.log"));
        if (!f2.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return;
        }
        QTextStream out(&f2);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << ' ' << line << '\n';
        return;
    }
    QTextStream out(&f);
    out << QDateTime::currentDateTime().toString(Qt::ISODate) << ' ' << line << '\n';
}

bool isBrowserProbeRpId(const QString& rpId)
{
    const QString id = rpId.trimmed().toLower();
    return id == QLatin1String(".dummy") || id == QLatin1String("dummy") || id.endsWith(QLatin1String(".dummy"));
}

bool isFirefoxBlinkRpId(const QString& rpId)
{
    return rpId.trimmed().toLower() == QLatin1String("make.me.blink");
}
} 

QSharedPointer<Database> OsPasskeyCeremony::unlockedDatabase(DatabaseTabWidget* tabWidget)
{
    if (!tabWidget) {
        return {};
    }
    auto* dbWidget = tabWidget->currentDatabaseWidget();
    if (!dbWidget || dbWidget->isLocked()) {
        for (int i = 0; i < tabWidget->count(); ++i) {
            auto* w = qobject_cast<DatabaseWidget*>(tabWidget->widget(i));
            if (w && !w->isLocked()) {
                return w->database();
            }
        }
        return {};
    }
    return dbWidget->database();
}

QByteArray OsPasskeyCeremony::handleRegister(DatabaseTabWidget* tabWidget, const QByteArray& ctapRequest, int* errorCode)
{
    *errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
    ceremonyLog(QStringLiteral("register begin req=%1").arg(ctapRequest.size()));

    auto db = unlockedDatabase(tabWidget);
    if (!db) {
        ceremonyLog(QStringLiteral("register fail: database locked"));
        *errorCode = ERROR_KEEPASS_DATABASE_NOT_OPENED;
        return {};
    }

    QJsonObject publicKeyOptions;
    QString origin;
    QString decodeError;
    if (!CtapCodec::decodeMakeCredential(ctapRequest, &publicKeyOptions, &origin, &decodeError)) {
        ceremonyLog(QStringLiteral("register fail: decode %1").arg(decodeError));
        *errorCode = ERROR_PASSKEYS_EMPTY_PUBLIC_KEY;
        return {};
    }

    const QString probeRp = publicKeyOptions.value(QStringLiteral("rp")).toObject().value(QStringLiteral("id")).toString();
    const bool isProbe = isBrowserProbeRpId(probeRp) || isFirefoxBlinkRpId(probeRp);

    
    
    QString originForPrepare = origin;
    if (isProbe) {
        QJsonObject rp = publicKeyOptions.value(QStringLiteral("rp")).toObject();
        rp.insert(QStringLiteral("id"), QStringLiteral("localhost"));
        rp.insert(QStringLiteral("name"), QStringLiteral("probe"));
        publicKeyOptions.insert(QStringLiteral("rp"), rp);
        if (!publicKeyOptions.contains(QStringLiteral("pubKeyCredParams"))
            || publicKeyOptions.value(QStringLiteral("pubKeyCredParams")).toArray().isEmpty()) {
            publicKeyOptions.insert(QStringLiteral("pubKeyCredParams"),
                                    QJsonArray{QJsonObject{{"type", "public-key"}, {"alg", -7}}});
        }
        originForPrepare = QStringLiteral("https://localhost");
    }

    auto prepared = passkeyEngine()->prepareRegistration(publicKeyOptions, originForPrepare);
    if (prepared.errorCode != PASSKEY_SUCCESS) {
        ceremonyLog(QStringLiteral("register fail: prepare %1 rp=%2")
                        .arg(prepared.errorCode)
                        .arg(probeRp));
        *errorCode = prepared.errorCode;
        return {};
    }

    const auto& creationOptions = prepared.response;
    const auto excludeCredentials = creationOptions["excludeCredentials"].toArray();
    const auto rpId = creationOptions["rp"].toObject()["id"].toString();
    const auto username = creationOptions["user"].toObject()["name"].toString();
    const auto userId = creationOptions["user"].toObject()["id"].toString();

    
    if (isProbe) {
        auto completed = passkeyEngine()->completeRegistration(creationOptions);
        if (completed.errorCode != PASSKEY_SUCCESS) {
            ceremonyLog(QStringLiteral("register probe fail: complete %1").arg(completed.errorCode));
            *errorCode = completed.errorCode;
            return {};
        }
        completed.response.remove(QStringLiteral("_privateKeyPem"));
        completed.response.remove(QStringLiteral("credentialId"));
        const QByteArray encoded = CtapCodec::encodeMakeCredentialResponse(completed.response);
        if (encoded.isEmpty()) {
            ceremonyLog(QStringLiteral("register probe fail: empty encoded response"));
            *errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
            return {};
        }
        ceremonyLog(QStringLiteral("register probe ok rp=%1 encoded=%2").arg(probeRp).arg(encoded.size()));
        *errorCode = PASSKEY_SUCCESS;
        return encoded;
    }

    if (auto* mw = getMainWindow()) {
        
        if (!mw->isVisible()) {
            mw->show();
        }
    }

    if (!excludeCredentials.isEmpty() && passkeyEngine()->isCredentialExcluded(db, excludeCredentials, rpId)) {
        ceremonyLog(QStringLiteral("register fail: excluded"));
        *errorCode = ERROR_PASSKEYS_CREDENTIAL_IS_EXCLUDED;
        return {};
    }

    auto* parent = getMainWindow() ? static_cast<QWidget*>(getMainWindow()) : nullptr;
    BrowserPasskeysConfirmationDialog confirmDialog(parent);
    const auto existing = passkeyEngine()->findPasskeysWithUserHandle(db, rpId, userId);
    confirmDialog.registerCredential(username, rpId, existing, publicKeyOptions["timeout"].toInt(300000));
    if (confirmDialog.exec() != QDialog::Accepted) {
        ceremonyLog(QStringLiteral("register fail: user cancelled confirm"));
        *errorCode = ERROR_PASSKEYS_REQUEST_CANCELED;
        return {};
    }

    auto completed = passkeyEngine()->completeRegistration(creationOptions);
    if (completed.errorCode != PASSKEY_SUCCESS) {
        ceremonyLog(QStringLiteral("register fail: complete %1").arg(completed.errorCode));
        *errorCode = completed.errorCode;
        return {};
    }

    const auto credentialId = completed.response["credentialId"].toString();
    const auto privateKey = completed.response["_privateKeyPem"].toString();
    completed.response.remove("_privateKeyPem");
    completed.response.remove("credentialId");

    const auto rpName = publicKeyOptions["rp"].toObject()["name"].toString();
    if (confirmDialog.isPasskeyUpdated() && confirmDialog.getSelectedEntry()) {
        passkeyEngine()->storePasskeyOnEntry(
            confirmDialog.getSelectedEntry(), rpId, rpName, username, credentialId, userId, privateKey);
    } else {
        passkeyEngine()->createPasskeyEntry(
            db->rootGroup(), origin, rpId, rpName, username, credentialId, userId, privateKey);
    }

    const QByteArray encoded = CtapCodec::encodeMakeCredentialResponse(completed.response);
    if (encoded.isEmpty()) {
        ceremonyLog(QStringLiteral("register fail: empty encoded response"));
        *errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
        return {};
    }

    const auto attObj = passkeyEncoding()->getArrayFromBase64(
        completed.response.value(QStringLiteral("response")).toObject().value(QStringLiteral("attestationObject")).toString());
    const QCborMap attMap = QCborValue::fromCbor(attObj).toMap();
    QByteArray authData = attMap.value(QStringLiteral("authData")).toByteArray();
    if (authData.isEmpty()) {
        authData = attMap.value(2).toByteArray();
    }
    QString idFromAuth;
    if (authData.size() >= 55) {
        const auto credLen = qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(authData.constData() + 53));
        if (authData.size() >= 55 + credLen) {
            idFromAuth = passkeyEncoding()->getBase64FromArray(authData.mid(55, credLen));
        }
    }
    ceremonyLog(QStringLiteral("register ok encoded=%1 authData=%2 cose=%3 id=%4 authId=%5")
                    .arg(encoded.size())
                    .arg(authData.size())
                    .arg(authData.size() >= 55 + 32 ? authData.size() - 87 : -1)
                    .arg(credentialId.left(20))
                    .arg(idFromAuth.left(20)));
    if (idFromAuth.isEmpty() || idFromAuth != credentialId) {
        ceremonyLog(QStringLiteral("register fail: authData id mismatch"));
        *errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
        return {};
    }
    
    const int coseLen = authData.size() >= 87 ? authData.size() - 87 : -1;
    if (coseLen >= 0 && coseLen < 60) {
        ceremonyLog(QStringLiteral("register fail: cose key too small (%1) — expected ES256").arg(coseLen));
        *errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
        return {};
    }
    *errorCode = PASSKEY_SUCCESS;
    return encoded;
}

QByteArray OsPasskeyCeremony::handleAssert(DatabaseTabWidget* tabWidget, const QByteArray& ctapRequest, int* errorCode)
{
    *errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
    ceremonyLog(QStringLiteral("assert begin req=%1").arg(ctapRequest.size()));

    auto db = unlockedDatabase(tabWidget);
    if (!db) {
        ceremonyLog(QStringLiteral("assert fail: database locked"));
        *errorCode = ERROR_KEEPASS_DATABASE_NOT_OPENED;
        return {};
    }

    QJsonObject publicKeyOptions;
    QString origin;
    QString decodeError;
    if (!CtapCodec::decodeGetAssertion(ctapRequest, &publicKeyOptions, &origin, &decodeError)) {
        ceremonyLog(QStringLiteral("assert fail: decode %1").arg(decodeError));
        *errorCode = ERROR_PASSKEYS_EMPTY_PUBLIC_KEY;
        return {};
    }

    const QString probeRp = publicKeyOptions.value(QStringLiteral("rpId")).toString();
    if (isBrowserProbeRpId(probeRp) || isFirefoxBlinkRpId(probeRp)) {
        
        ceremonyLog(QStringLiteral("assert probe no-cred rp=%1").arg(probeRp));
        *errorCode = ERROR_KEEPASS_NO_LOGINS_FOUND;
        return {};
    }

    if (auto* mw = getMainWindow()) {
        if (!mw->isVisible()) {
            mw->show();
        }
    }

    auto prepared = passkeyEngine()->prepareAssertion(publicKeyOptions, origin);
    if (prepared.errorCode != PASSKEY_SUCCESS) {
        ceremonyLog(QStringLiteral("assert fail: prepare %1").arg(prepared.errorCode));
        *errorCode = prepared.errorCode;
        return {};
    }

    const auto& assertionOptions = prepared.response;
    const auto rpId = assertionOptions["rpId"].toString();
    const auto allowArr = assertionOptions.value(QStringLiteral("allowCredentials")).toArray();
    const auto entries = passkeyEngine()->findAllowedAssertionEntries(db, assertionOptions, rpId);
    if (entries.isEmpty()) {
        const int rpN = passkeyEngine()->findPasskeysForRp(db, rpId).size();
        QString allowPreview;
        for (int i = 0; i < qMin(2, allowArr.size()); ++i) {
            if (!allowPreview.isEmpty()) {
                allowPreview += QLatin1Char(',');
            }
            allowPreview += allowArr.at(i).toObject().value(QStringLiteral("id")).toString().left(16);
        }
        QString storedPreview;
        for (Entry* e : passkeyEngine()->findPasskeysForRp(db, rpId)) {
            if (!storedPreview.isEmpty()) {
                storedPreview += QLatin1Char(',');
            }
            storedPreview += passkeyUtils()->getCredentialIdFromEntry(e).left(16);
        }
        ceremonyLog(QStringLiteral("assert fail: no matching entries for rp=%1 allow=%2 stored=%3 allowIds=[%4] storedIds=[%5]")
                        .arg(rpId)
                        .arg(allowArr.size())
                        .arg(rpN)
                        .arg(allowPreview)
                        .arg(storedPreview));
        *errorCode = ERROR_KEEPASS_NO_LOGINS_FOUND;
        return {};
    }

    
    
    auto* parent = getMainWindow() ? static_cast<QWidget*>(getMainWindow()) : nullptr;
    if (auto* mw = getMainWindow()) {
        mw->show();
        mw->raise();
        mw->activateWindow();
    }
    BrowserPasskeysConfirmationDialog confirmDialog(parent);
    confirmDialog.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    confirmDialog.authenticateCredential(entries, rpId, publicKeyOptions["timeout"].toInt(300000));
    if (confirmDialog.exec() != QDialog::Accepted) {
        ceremonyLog(QStringLiteral("assert fail: user cancelled"));
        *errorCode = ERROR_PASSKEYS_REQUEST_CANCELED;
        return {};
    }
    Entry* selected = confirmDialog.getSelectedEntry();

    if (!selected) {
        *errorCode = ERROR_PASSKEYS_REQUEST_CANCELED;
        return {};
    }

    auto completed = passkeyEngine()->completeAssertion(assertionOptions, selected);
    if (completed.errorCode != PASSKEY_SUCCESS) {
        ceremonyLog(QStringLiteral("assert fail: complete %1").arg(completed.errorCode));
        *errorCode = completed.errorCode;
        return {};
    }

    const QByteArray encoded = CtapCodec::encodeGetAssertionResponse(
        completed.response,
        CtapCodec::getAssertionAllowListSize(ctapRequest),
        CtapCodec::getAssertionAllowListUsesStringKeys(ctapRequest));
    if (encoded.isEmpty()) {
        ceremonyLog(QStringLiteral("assert fail: empty encoded response"));
        *errorCode = ERROR_PASSKEYS_UNKNOWN_ERROR;
        return {};
    }

    ceremonyLog(QStringLiteral("assert ok encoded=%1 rp=%2 id=%3 allow=%4")
                    .arg(encoded.size())
                    .arg(rpId)
                    .arg(passkeyUtils()->getCredentialIdFromEntry(selected).left(16))
                    .arg(allowArr.size()));
    *errorCode = PASSKEY_SUCCESS;
    return encoded;
}
