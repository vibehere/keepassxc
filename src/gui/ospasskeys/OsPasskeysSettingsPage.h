/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 */

#ifndef KEEPASSXC_OSPASSKEYSSETTINGSPAGE_H
#define KEEPASSXC_OSPASSKEYSSETTINGSPAGE_H

#include "gui/ApplicationSettingsWidget.h"

class OsPasskeysSettingsPage : public ISettingsPage
{
public:
    QString name() override;
    QIcon icon() override;
    QWidget* createWidget() override;
    void loadSettings(QWidget* widget) override;
    void saveSettings(QWidget* widget) override;
};

#endif
