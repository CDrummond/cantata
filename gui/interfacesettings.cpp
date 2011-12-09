/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "interfacesettings.h"
#include "settings.h"
#include <QtGui/QComboBox>

InterfaceSettings::InterfaceSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
};

void InterfaceSettings::load()
{
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    systemTrayPopup->setChecked(Settings::self()->showPopups());
    libraryCoverSize->setCurrentIndex(Settings::self()->libraryCoverSize());
    albumCoverSize->setCurrentIndex(Settings::self()->albumCoverSize());
}

void InterfaceSettings::save()
{
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    Settings::self()->saveLibraryCoverSize(libraryCoverSize->currentIndex());
    Settings::self()->saveAlbumCoverSize(albumCoverSize->currentIndex());
}
