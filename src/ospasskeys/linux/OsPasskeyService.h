/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_OSPASSKEYSERVICE_LINUX_H
#define KEEPASSXC_OSPASSKEYSERVICE_LINUX_H

#include <QObject>
#include <memory>

class DatabaseTabWidget;
class UhidCtapDevice;

class OsPasskeyService : public QObject
{
    Q_OBJECT

public:
    explicit OsPasskeyService(QObject* parent = nullptr);
    static OsPasskeyService* instance();

    void setDatabaseTabWidget(DatabaseTabWidget* tabWidget);
    void initialize();
    void shutdown();

    bool isEnabled() const;
    bool setEnabled(bool enabled);

    bool isDeviceActive() const;
    QString lastError() const;

signals:
    void providerStateChanged(bool enabled);

private:
    Q_DISABLE_COPY(OsPasskeyService)

    QByteArray handleCtap(quint8 cmd, const QByteArray& cbor);
    bool startDevice();
    void stopDevice();

    DatabaseTabWidget* m_tabWidget = nullptr;
    std::unique_ptr<UhidCtapDevice> m_device;
    bool m_initialized = false;
    QString m_lastError;
};

static inline OsPasskeyService* osPasskeyService()
{
    return OsPasskeyService::instance();
}

#endif
