/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "externalsettings.h"
#include "settings.h"
#include "onoffbutton.h"
#include <QtCore/qglobal.h>

ExternalSettings::ExternalSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef Q_OS_WIN
    gnomeMediaKeys->setVisible(false);
    gnomeMediaKeysLabel->setVisible(false);
    #endif
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnClose, SLOT(setEnabled(bool)));
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnCloseLabel, SLOT(setEnabled(bool)));
};

void ExternalSettings::load()
{
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    systemTrayPopup->setChecked(Settings::self()->showPopups());
    minimiseOnClose->setChecked(Settings::self()->minimiseOnClose());
    minimiseOnClose->setEnabled(systemTrayCheckBox->isChecked());
    minimiseOnCloseLabel->setEnabled(systemTrayCheckBox->isChecked());
    #ifndef Q_OS_WIN
    gnomeMediaKeys->setChecked(Settings::self()->gnomeMediaKeys());
    #endif
}

void ExternalSettings::save()
{
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    Settings::self()->saveMinimiseOnClose(minimiseOnClose->isChecked());
    #ifndef Q_OS_WIN
    Settings::self()->saveGnomeMediaKeys(gnomeMediaKeys->isChecked());
    #endif
}
