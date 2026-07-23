/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#include "OsPasskeysSettingsPage.h"

#include "core/Config.h"
#include "gui/MessageBox.h"
#include "config-keepassx.h"

#ifdef Q_OS_WIN
#include "ospasskeys/windows/OsPasskeyService.h"
#include "ospasskeys/windows/PluginRegistrationManager.h"
#elif defined(Q_OS_LINUX)
#include "ospasskeys/linux/OsPasskeyService.h"
#endif

#ifdef MessageBox
#undef MessageBox
#endif

#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

QString OsPasskeysSettingsPage::name()
{
    return QObject::tr("OS Passkeys");
}

QIcon OsPasskeysSettingsPage::icon()
{
    return QIcon(QStringLiteral(":/icons/application/scalable/actions/passkey.svg"));
}

QWidget* OsPasskeysSettingsPage::createWidget()
{
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

#ifdef Q_OS_WIN
    auto* intro = new QLabel(
        QObject::tr("Register KeePassXC as a Windows passkey provider so browsers and apps can use "
                    "passkeys from your unlocked database without the browser extension.\n\n"
                    "You must run the MSIX build (Start → KeePassXC OS Passkeys). Private keys never leave "
                    "KeePassXC except into your encrypted database.\n\n"
                    "KeePassXC-Browser remains fully supported for password fill, TOTP, and passkeys."));
    auto* enable = new QCheckBox(QObject::tr("Enable as Windows passkey provider (opt-in)"));
    auto* note = new QLabel(
        QObject::tr("After enabling: unlock a database, then in Brave try again. If Windows still appears alone, "
                    "open Windows Settings → Accounts → Passkeys and look for “KeePassXC Passkeys”."));
#elif defined(Q_OS_LINUX)
    auto* intro = new QLabel(
        QObject::tr("Expose KeePassXC as a virtual FIDO2 security key (via /dev/uhid) so Chromium/Firefox "
                    "can use passkeys from your unlocked database without the browser extension.\n\n"
                    "Requires the uhid kernel module and permission to open /dev/uhid. This usually needs a "
                    "native Linux install or VM — WSL often cannot create virtual HID devices.\n\n"
                    "KeePassXC-Browser remains fully supported for password fill, TOTP, and passkeys."));
    auto* enable = new QCheckBox(QObject::tr("Enable Linux OS passkey provider (virtual FIDO2, opt-in)"));
    auto* note = new QLabel(
        QObject::tr("After enabling: unlock a database, then use a site like webauthn.io. Choose the "
                    "KeePassXC Passkeys security key when the browser asks."));
#else
    auto* intro = new QLabel(QObject::tr("OS passkeys are not supported on this platform."));
    auto* enable = new QCheckBox(QObject::tr("Enable OS passkey provider"));
    enable->setEnabled(false);
    auto* note = new QLabel();
#endif
    intro->setWordWrap(true);
    layout->addWidget(intro);

    enable->setObjectName(QStringLiteral("osPasskeysEnable"));
    layout->addWidget(enable);

    auto* status = new QLabel();
    status->setObjectName(QStringLiteral("osPasskeysStatus"));
    status->setWordWrap(true);
    layout->addWidget(status);

    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    return widget;
}

void OsPasskeysSettingsPage::loadSettings(QWidget* widget)
{
    auto* enable = widget->findChild<QCheckBox*>(QStringLiteral("osPasskeysEnable"));
    auto* status = widget->findChild<QLabel*>(QStringLiteral("osPasskeysStatus"));
    if (enable) {
        enable->setChecked(config()->get(Config::OsPasskeys_Enabled).toBool());
    }
    if (!status) {
        return;
    }

#ifdef Q_OS_WIN
    auto* reg = PluginRegistrationManager::instance();
    if (!reg->isRunningPackaged()) {
        status->setText(QObject::tr("Status: not running as MSIX — Windows will ignore registration. "
                                    "Launch “KeePassXC OS Passkeys” from Start."));
    } else if (reg->isRegistered()) {
        status->setText(QObject::tr("Status: registered with Windows."));
    } else if (config()->get(Config::OsPasskeys_Enabled).toBool()) {
        status->setText(QObject::tr("Status: enabled in settings but NOT registered with Windows."));
    } else {
        status->setText(QObject::tr("Status: disabled."));
    }
#elif defined(Q_OS_LINUX)
    if (osPasskeyService()->isDeviceActive()) {
        status->setText(QObject::tr("Status: virtual FIDO2 device active (/dev/uhid)."));
    } else if (config()->get(Config::OsPasskeys_Enabled).toBool()) {
        status->setText(QObject::tr("Status: enabled but device not active — %1")
                            .arg(osPasskeyService()->lastError().isEmpty()
                                     ? QObject::tr("unknown error")
                                     : osPasskeyService()->lastError()));
    } else {
        status->setText(QObject::tr("Status: disabled."));
    }
#else
    status->setText(QObject::tr("Status: unsupported."));
#endif
}

void OsPasskeysSettingsPage::saveSettings(QWidget* widget)
{
    auto* enable = widget->findChild<QCheckBox*>(QStringLiteral("osPasskeysEnable"));
    if (!enable) {
        return;
    }

    const bool wanted = enable->isChecked();

#ifdef Q_OS_WIN
    auto* reg = PluginRegistrationManager::instance();

    if (wanted && !reg->isRunningPackaged()) {
        MessageBox::critical(widget,
                             QObject::tr("OS Passkeys"),
                             QObject::tr("This copy is not the MSIX install.\n\n"
                                         "Close this window and open “KeePassXC OS Passkeys” from the Start menu, "
                                         "then enable the provider again."));
        enable->setChecked(false);
        config()->set(Config::OsPasskeys_Enabled, false);
        return;
    }

    if (wanted != config()->get(Config::OsPasskeys_Enabled).toBool() || (wanted && !reg->isRegistered())) {
        const bool ok = osPasskeyService()->setEnabled(wanted);
        if (wanted && !ok) {
            MessageBox::critical(
                widget,
                QObject::tr("OS Passkeys"),
                QObject::tr("Windows refused to register KeePassXC as a passkey provider (%1).\n\n"
                            "Use the MSIX app from Start, unlock a database, then try again.")
                    .arg(reg->lastErrorString()));
            enable->setChecked(false);
            config()->set(Config::OsPasskeys_Enabled, false);
        }
    }
#elif defined(Q_OS_LINUX)
    if (wanted != config()->get(Config::OsPasskeys_Enabled).toBool() || (wanted && !osPasskeyService()->isDeviceActive())) {
        const bool ok = osPasskeyService()->setEnabled(wanted);
        if (wanted && !ok) {
            MessageBox::critical(widget,
                                 QObject::tr("OS Passkeys"),
                                 QObject::tr("Could not start the virtual FIDO2 device:\n\n%1")
                                     .arg(osPasskeyService()->lastError()));
            enable->setChecked(false);
            config()->set(Config::OsPasskeys_Enabled, false);
        }
    }
#else
    Q_UNUSED(wanted)
#endif
}
