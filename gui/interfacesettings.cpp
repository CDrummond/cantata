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
    connect(libraryView, SIGNAL(currentIndexChanged(int)), SLOT(libraryViewChanged()));
    connect(libraryCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(libraryCoverSizeChanged()));
};

void InterfaceSettings::load()
{
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    systemTrayPopup->setChecked(Settings::self()->showPopups());
    libraryView->setCurrentIndex(Settings::self()->libraryView());
    libraryCoverSize->setCurrentIndex(Settings::self()->libraryCoverSize());
    albumCoverSize->setCurrentIndex(Settings::self()->albumCoverSize());
    folderView->setCurrentIndex(Settings::self()->folderView());
    playlistsView->setCurrentIndex(Settings::self()->playlistsView());
}

void InterfaceSettings::save()
{
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    Settings::self()->saveLibraryView(libraryView->currentIndex());
    Settings::self()->saveLibraryCoverSize(libraryCoverSize->currentIndex());
    Settings::self()->saveAlbumCoverSize(albumCoverSize->currentIndex());
    Settings::self()->saveFolderView(folderView->currentIndex());
    Settings::self()->savePlaylistsView(playlistsView->currentIndex());
}

void InterfaceSettings::libraryViewChanged()
{
    if (0!=libraryView->currentIndex() && 0==libraryCoverSize->currentIndex()) {
        libraryCoverSize->setCurrentIndex(2);
    }
}

void InterfaceSettings::libraryCoverSizeChanged()
{
    if (0!=libraryView->currentIndex() && 0==libraryCoverSize->currentIndex()) {
        libraryView->setCurrentIndex(0);
    }
}
