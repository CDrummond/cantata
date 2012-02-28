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

#include "interfacesettings.h"
#include "settings.h"
#include "itemview.h"
#include <QtGui/QComboBox>
#include <QtGui/QCheckBox>

InterfaceSettings::InterfaceSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    connect(albumsView, SIGNAL(currentIndexChanged(int)), SLOT(albumsViewChanged()));
    connect(albumsCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(albumsCoverSizeChanged()));
    connect(groupedPlayQueue, SIGNAL(currentIndexChanged(int)), SLOT(groupedPlayQueueChanged()));
    #ifndef ENABLE_DEVICES_SUPPORT
    devicesView->setVisible(false);
    devicesViewLabel->setVisible(false);
    showDeleteAction->setVisible(false);
    showDeleteActionLabel->setVisible(false);
    #endif
};

void InterfaceSettings::load()
{
    libraryView->setCurrentIndex(Settings::self()->libraryView());
    libraryCoverSize->setCurrentIndex(Settings::self()->libraryCoverSize());
    libraryYear->setChecked(Settings::self()->libraryYear());
    albumsView->setCurrentIndex(Settings::self()->albumsView());
    albumsCoverSize->setCurrentIndex(Settings::self()->albumsCoverSize());
    albumFirst->setCurrentIndex(Settings::self()->albumFirst() ? 0 : 1);
    folderView->setCurrentIndex(Settings::self()->folderView());
    playlistsView->setCurrentIndex(Settings::self()->playlistsView());
    streamsView->setCurrentIndex(Settings::self()->streamsView());
    groupSingle->setChecked(Settings::self()->groupSingle());
    #ifdef ENABLE_DEVICES_SUPPORT
    showDeleteAction->setChecked(Settings::self()->showDeleteAction());
    devicesView->setCurrentIndex(Settings::self()->devicesView());
    #endif
    groupedPlayQueue->setCurrentIndex(Settings::self()->groupedPlayQueue() ? 1 : 0);
    autoCollapsePlayQueue->setChecked(Settings::self()->autoCollapsePlayQueue());
}

void InterfaceSettings::save()
{
    Settings::self()->saveLibraryView(libraryView->currentIndex());
    Settings::self()->saveLibraryCoverSize(libraryCoverSize->currentIndex());
    Settings::self()->saveLibraryYear(libraryYear->isChecked());
    Settings::self()->saveAlbumsView(albumsView->currentIndex());
    Settings::self()->saveAlbumsCoverSize(albumsCoverSize->currentIndex());
    Settings::self()->saveAlbumFirst(0==albumFirst->currentIndex());
    Settings::self()->saveFolderView(folderView->currentIndex());
    Settings::self()->savePlaylistsView(playlistsView->currentIndex());
    Settings::self()->saveStreamsView(streamsView->currentIndex());
    Settings::self()->saveGroupSingle(groupSingle->isChecked());
    #ifdef ENABLE_DEVICES_SUPPORT
    Settings::self()->saveShowDeleteAction(showDeleteAction->isChecked());
    Settings::self()->saveDevicesView(devicesView->currentIndex());
    #endif
    Settings::self()->saveGroupedPlayQueue(1==groupedPlayQueue->currentIndex());
    Settings::self()->saveAutoCollapsePlayQueue(autoCollapsePlayQueue->isChecked());
}

void InterfaceSettings::albumsViewChanged()
{
    if (ItemView::Mode_IconTop==albumsView->currentIndex() && 0==albumsCoverSize->currentIndex()) {
        albumsCoverSize->setCurrentIndex(2);
    }
}

void InterfaceSettings::albumsCoverSizeChanged()
{
    if (ItemView::Mode_IconTop==albumsView->currentIndex() && 0==albumsCoverSize->currentIndex()) {
        albumsView->setCurrentIndex(1);
    }
}

void InterfaceSettings::groupedPlayQueueChanged()
{
    autoCollapsePlayQueue->setEnabled(1==groupedPlayQueue->currentIndex());
    autoCollapsePlayQueueLabel->setEnabled(1==groupedPlayQueue->currentIndex());
}

