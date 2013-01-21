/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "localize.h"
#include "musiclibraryitemalbum.h"
#include "onoffbutton.h"
#include <QtGui/QComboBox>
#include <QtCore/qglobal.h>

static void addImageSizes(QComboBox *box)
{
    box->addItem(i18n("None"), MusicLibraryItemAlbum::CoverNone);
    box->addItem(i18n("Small"), MusicLibraryItemAlbum::CoverSmall);
    box->addItem(i18n("Medium"), MusicLibraryItemAlbum::CoverMedium);
    box->addItem(i18n("Large"), MusicLibraryItemAlbum::CoverLarge);
    box->addItem(i18n("Extra Large"), MusicLibraryItemAlbum::CoverExtraLarge);
    box->addItem(i18n("Automatic"), MusicLibraryItemAlbum::CoverAuto);
}

static void addViewTypes(QComboBox *box, bool iconMode=false, bool groupedTree=false)
{
    box->addItem(i18n("Simple Tree"), ItemView::Mode_SimpleTree);
    box->addItem(i18n("Detailed Tree"), ItemView::Mode_DetailedTree);
    if (groupedTree) {
        box->addItem(i18n("Grouped Albums"), ItemView::Mode_GroupedTree);
    }
    box->addItem(i18n("List"), ItemView::Mode_List);
    if (iconMode) {
        box->addItem(i18n("Icon/List"), ItemView::Mode_IconTop);
    }
}

static void selectEntry(QComboBox *box, int v)
{
    for (int i=1; i<box->count(); ++i) {
        if (box->itemData(i).toInt()==v) {
            box->setCurrentIndex(i);
            return;
        }
    }
}

static inline int getViewType(QComboBox *box)
{
    return box->itemData(box->currentIndex()).toInt();
}

InterfaceSettings::InterfaceSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    addImageSizes(libraryCoverSize);
    addImageSizes(albumsCoverSize);
    addViewTypes(libraryView, true);
    addViewTypes(albumsView, true);
    addViewTypes(folderView);
    addViewTypes(playlistsView, false, true);
    addViewTypes(streamsView);
    addViewTypes(onlineView);
    #ifdef ENABLE_DEVICES_SUPPORT
    addViewTypes(devicesView);
    #endif
    groupMultiple->addItem(i18n("Grouped by \'Album Artist\'"));
    groupMultiple->addItem(i18n("Grouped under \'Various Artists\'"));
    connect(libraryView, SIGNAL(currentIndexChanged(int)), SLOT(libraryViewChanged()));
    connect(libraryCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(libraryCoverSizeChanged()));
    connect(albumsView, SIGNAL(currentIndexChanged(int)), SLOT(albumsViewChanged()));
    connect(albumsCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(albumsCoverSizeChanged()));
    connect(playlistsView, SIGNAL(currentIndexChanged(int)), SLOT(playListsStyleChanged()));
    connect(playQueueGrouped, SIGNAL(currentIndexChanged(int)), SLOT(playQueueGroupedChanged()));
    #ifndef ENABLE_DEVICES_SUPPORT
    devicesView->setVisible(false);
    devicesViewLabel->setVisible(false);
    showDeleteAction->setVisible(false);
    showDeleteActionLabel->setVisible(false);
    #endif
    #ifdef Q_OS_WIN
    gnomeMediaKeys->setVisible(false);
    gnomeMediaKeysLabel->setVisible(false);
    #endif
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnClose, SLOT(setEnabled(bool)));
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnCloseLabel, SLOT(setEnabled(bool)));
};

void InterfaceSettings::load()
{
    libraryArtistImage->setChecked(Settings::self()->libraryArtistImage());
    selectEntry(libraryView, Settings::self()->libraryView());
    libraryCoverSize->setCurrentIndex(Settings::self()->libraryCoverSize());
    libraryYear->setChecked(Settings::self()->libraryYear());
    selectEntry(albumsView, Settings::self()->albumsView());
    albumsCoverSize->setCurrentIndex(Settings::self()->albumsCoverSize());
    albumSort->setCurrentIndex(Settings::self()->albumSort());
    selectEntry(folderView, Settings::self()->folderView());
    selectEntry(playlistsView, Settings::self()->playlistsView());
    playListsStartClosed->setChecked(Settings::self()->playListsStartClosed());
    selectEntry(streamsView, Settings::self()->streamsView());
    selectEntry(onlineView, Settings::self()->onlineView());
    groupSingle->setChecked(Settings::self()->groupSingle());
    groupMultiple->setCurrentIndex(Settings::self()->groupMultiple() ? 1 : 0);
    #ifdef ENABLE_DEVICES_SUPPORT
    showDeleteAction->setChecked(Settings::self()->showDeleteAction());
    selectEntry(devicesView, Settings::self()->devicesView());
    #endif
    playQueueGrouped->setCurrentIndex(Settings::self()->playQueueGrouped() ? 1 : 0);
    playQueueAutoExpand->setChecked(Settings::self()->playQueueAutoExpand());
    playQueueStartClosed->setChecked(Settings::self()->playQueueStartClosed());
    playQueueScroll->setChecked(Settings::self()->playQueueScroll());
    storeCoversInMpdDir->setChecked(Settings::self()->storeCoversInMpdDir());
    storeLyricsInMpdDir->setChecked(Settings::self()->storeLyricsInMpdDir());
    albumsViewChanged();
    albumsCoverSizeChanged();
    playListsStyleChanged();
    playQueueGroupedChanged();
    lyricsBgnd->setChecked(Settings::self()->lyricsBgnd());
    forceSingleClick->setChecked(Settings::self()->forceSingleClick());
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    systemTrayPopup->setChecked(Settings::self()->showPopups());
    minimiseOnClose->setChecked(Settings::self()->minimiseOnClose());
    minimiseOnClose->setEnabled(systemTrayCheckBox->isChecked());
    minimiseOnCloseLabel->setEnabled(systemTrayCheckBox->isChecked());
    #ifndef Q_OS_WIN
    gnomeMediaKeys->setChecked(Settings::self()->gnomeMediaKeys());
    #endif
}

void InterfaceSettings::save()
{
    Settings::self()->saveLibraryArtistImage(libraryArtistImage->isChecked());
    Settings::self()->saveLibraryView(getViewType(libraryView));
    Settings::self()->saveLibraryCoverSize(libraryCoverSize->currentIndex());
    Settings::self()->saveLibraryYear(libraryYear->isChecked());
    Settings::self()->saveAlbumsView(getViewType(albumsView));
    Settings::self()->saveAlbumsCoverSize(albumsCoverSize->currentIndex());
    Settings::self()->saveAlbumSort(albumSort->currentIndex());
    Settings::self()->saveFolderView(getViewType(folderView));
    Settings::self()->savePlaylistsView(getViewType(playlistsView));
    Settings::self()->savePlayListsStartClosed(playListsStartClosed->isChecked());
    Settings::self()->saveStreamsView(getViewType(streamsView));
    Settings::self()->saveOnlineView(getViewType(onlineView));
    Settings::self()->saveGroupSingle(groupSingle->isChecked());
    Settings::self()->saveGroupMultiple(1==groupMultiple->currentIndex());
    #ifdef ENABLE_DEVICES_SUPPORT
    Settings::self()->saveShowDeleteAction(showDeleteAction->isChecked());
    Settings::self()->saveDevicesView(getViewType(devicesView));
    #endif
    Settings::self()->savePlayQueueGrouped(1==playQueueGrouped->currentIndex());
    Settings::self()->savePlayQueueAutoExpand(playQueueAutoExpand->isChecked());
    Settings::self()->savePlayQueueStartClosed(playQueueStartClosed->isChecked());
    Settings::self()->savePlayQueueScroll(playQueueScroll->isChecked());
    Settings::self()->saveStoreCoversInMpdDir(storeCoversInMpdDir->isChecked());
    Settings::self()->saveStoreLyricsInMpdDir(storeLyricsInMpdDir->isChecked());
    Settings::self()->saveLyricsBgnd(lyricsBgnd->isChecked());
    Settings::self()->saveForceSingleClick(forceSingleClick->isChecked());
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    Settings::self()->saveMinimiseOnClose(minimiseOnClose->isChecked());
    #ifndef Q_OS_WIN
    Settings::self()->saveGnomeMediaKeys(gnomeMediaKeys->isChecked());
    #endif
}

void InterfaceSettings::libraryViewChanged()
{
    int vt=getViewType(libraryView);
    if (ItemView::Mode_IconTop==vt && 0==libraryCoverSize->currentIndex()) {
        libraryCoverSize->setCurrentIndex(2);
    }

    bool isIcon=ItemView::Mode_IconTop==vt;
    libraryArtistImage->setEnabled(!isIcon);
    libraryArtistImageLabel->setEnabled(libraryArtistImage->isEnabled());
    if (isIcon) {
        libraryArtistImage->setChecked(true);
    }
}

void InterfaceSettings::libraryCoverSizeChanged()
{
    if (ItemView::Mode_IconTop==getViewType(libraryView) && 0==libraryCoverSize->currentIndex()) {
        libraryView->setCurrentIndex(1);
    }
    if (0==libraryCoverSize->currentIndex()) {
        libraryArtistImage->setChecked(false);
    }
}

void InterfaceSettings::albumsViewChanged()
{
    if (ItemView::Mode_IconTop==getViewType(albumsView) && 0==albumsCoverSize->currentIndex()) {
        albumsCoverSize->setCurrentIndex(2);
    }
}

void InterfaceSettings::albumsCoverSizeChanged()
{
    if (ItemView::Mode_IconTop==getViewType(albumsView) && 0==albumsCoverSize->currentIndex()) {
        albumsView->setCurrentIndex(1);
    }
}

void InterfaceSettings::playQueueGroupedChanged()
{
    playQueueAutoExpand->setEnabled(1==playQueueGrouped->currentIndex());
    playQueueAutoExpandLabel->setEnabled(1==playQueueGrouped->currentIndex());
    playQueueStartClosed->setEnabled(1==playQueueGrouped->currentIndex());
    playQueueStartClosedLabel->setEnabled(1==playQueueGrouped->currentIndex());
}

void InterfaceSettings::playListsStyleChanged()
{
    bool grouped=getViewType(playlistsView)==ItemView::Mode_GroupedTree;
    playListsStartClosed->setEnabled(grouped);
    playListsStartClosedLabel->setEnabled(grouped);
}
