/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_OSPASSKEYSERVICE_H
#define KEEPASSXC_OSPASSKEYSERVICE_H

#include <windows.h>

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QUuid>

class Database;
class DatabaseTabWidget;

class OsPasskeyService : public QObject
{
    Q_OBJECT

public:
    explicit OsPasskeyService(QObject* parent = nullptr)
        : QObject(parent)
    {
    }
    static OsPasskeyService* instance();

    void setDatabaseTabWidget(DatabaseTabWidget* tabWidget);
    void initialize();
    void shutdown();

    bool isEnabled() const;
    bool setEnabled(bool enabled);

    bool isDatabaseUnlocked() const;
    QSharedPointer<Database> unlockedDatabase() const;

    
    HRESULT makeCredential(HWND hwnd, const QByteArray& ctapRequest, QByteArray* ctapResponse);
    HRESULT getAssertion(HWND hwnd, const QByteArray& ctapRequest, QByteArray* ctapResponse);
    void cancelCurrentOperation(const QUuid& transactionId);

    void syncCredentialMetadata();

signals:
    void providerStateChanged(bool enabled);

private slots:
    void handleDatabaseUnlocked();
    void handleDatabaseLocked();

private:
    Q_DISABLE_COPY(OsPasskeyService)

    HRESULT runCeremonyOnUiThread(bool registerMode, HWND hwnd, const QByteArray& request, QByteArray* response);
    QByteArray handleRegisterOnUi(HWND hwnd, const QByteArray& ctapRequest, int* errorCode);
    QByteArray handleAssertOnUi(HWND hwnd, const QByteArray& ctapRequest, int* errorCode);
    bool performUserVerification(HWND hwnd) const;

    DatabaseTabWidget* m_tabWidget = nullptr;
    QMutex m_cancelMutex;
    QUuid m_activeTransaction;
    bool m_cancelRequested = false;
    bool m_initialized = false;
};

static inline OsPasskeyService* osPasskeyService()
{
    return OsPasskeyService::instance();
}

#endif
