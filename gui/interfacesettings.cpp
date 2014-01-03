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
#include <QComboBox>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

static void addImageSizes(QComboBox *box)
{
    box->addItem(i18n("None"), MusicLibraryItemAlbum::CoverNone);
    box->addItem(i18n("Small"), MusicLibraryItemAlbum::CoverSmall);
    box->addItem(i18n("Medium"), MusicLibraryItemAlbum::CoverMedium);
    box->addItem(i18n("Large"), MusicLibraryItemAlbum::CoverLarge);
    box->addItem(i18n("Extra Large"), MusicLibraryItemAlbum::CoverExtraLarge);
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

static void addStartupState(QComboBox *box)
{
    box->addItem(i18n("Show main window"), Settings::SS_ShowMainWindow);
    box->addItem(i18n("Hide main window"), Settings::SS_HideMainWindow);
    box->addItem(i18n("Restore previous state"), Settings::SS_Previous);
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

static inline int getValue(QComboBox *box)
{
    return box->itemData(box->currentIndex()).toInt();
}

InterfaceSettings::InterfaceSettings(QWidget *p, const QStringList &hiddenPages)
    : QWidget(p)
{
    int removeCount=0;
    enabledViews=V_All;
    if (hiddenPages.contains(QLatin1String("LibraryPage"))) {
        enabledViews-=V_Artists;
    }
    if (hiddenPages.contains(QLatin1String("AlbumsPage"))) {
        enabledViews-=V_Albums;
    }
    if (hiddenPages.contains(QLatin1String("FolderPage"))) {
        enabledViews-=V_Folders;
    }
    if (hiddenPages.contains(QLatin1String("PlaylistsPage"))) {
        enabledViews-=V_Playlists;
    }
//    if (hiddenPages.contains(QLatin1String("DynamicPage"))) {
//        enabledViews-=V_Dynamic;
//    }
    #ifdef ENABLE_STREAMS
    if (hiddenPages.contains(QLatin1String("StreamsPage"))) {
        enabledViews-=V_Streams;
    }
    #else
    enabledViews-=V_Streams;
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    if (hiddenPages.contains(QLatin1String("OnlineServicesPage"))) {
        enabledViews-=V_Online;
    }
    #else
    enabledViews-=V_Online;
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    if (hiddenPages.contains(QLatin1String("DevicesPage"))) {
        enabledViews-=V_Devices;
    }
    #else
    enabledViews-=V_Devices;
    #endif

    setupUi(this);
    if (enabledViews&V_Artists) {
        addImageSizes(libraryCoverSize);
        addViewTypes(libraryView, true);
    } else {
        tabWidget->removeTab(0);
        removeCount++;
    }
    if (enabledViews&V_Albums) {
        addImageSizes(albumsCoverSize);
        addViewTypes(albumsView, true);
    } else {
        tabWidget->removeTab(1-removeCount);
        removeCount++;
    }
    if (enabledViews&V_Folders) {
        addViewTypes(folderView);
    }  else {
        REMOVE(folderView)
        REMOVE(folderViewLabel)
    }
    if (enabledViews&V_Playlists) {
        addViewTypes(playlistsView, false, true);
    } else {
        tabWidget->removeTab(2-removeCount);
        removeCount++;
    }
    if (enabledViews&V_Streams) {
        addViewTypes(streamsView);
    } else {
        REMOVE(streamsView)
        REMOVE(streamsViewLabel)
    }
    if (enabledViews&V_Online) {
        addViewTypes(onlineView);
    } else {
        REMOVE(onlineView)
        REMOVE(onlineViewLabel)
    }

    if (enabledViews&V_Devices) {
        addViewTypes(devicesView);
    }
    else {
        REMOVE(devicesView)
        REMOVE(devicesViewLabel)
    }

    int otherViewCount=(enabledViews&V_Folders ? 1 : 0)+(enabledViews&V_Streams ? 1 : 0)+(enabledViews&V_Online ? 1 : 0)+(enabledViews&V_Devices ? 1 : 0);

    if (0==otherViewCount) {
        tabWidget->removeTab(3-removeCount);
        removeCount++;
    } else if (1==otherViewCount) {
        if (enabledViews&V_Folders) {
            tabWidget->setTabText(3-removeCount, i18n("Folders"));
            folderViewLabel->setText(i18n("Style:"));
        } else if (enabledViews&V_Streams) {
            tabWidget->setTabText(3-removeCount, i18n("Streams"));
            streamsViewLabel->setText(i18n("Style:"));
        } else if (enabledViews&V_Online) {
            tabWidget->setTabText(3-removeCount, i18n("Online"));
            onlineViewLabel->setText(i18n("Style:"));
        } else if (enabledViews&V_Devices) {
            tabWidget->setTabText(3-removeCount, i18n("Devices"));
            devicesViewLabel->setText(i18n("Style:"));
        }
    }

    groupMultiple->addItem(i18n("Grouped by \'Album Artist\'"));
    groupMultiple->addItem(i18n("Grouped under \'Various Artists\'"));
    addStartupState(startupState);
    if (enabledViews&V_Artists) {
        connect(libraryView, SIGNAL(currentIndexChanged(int)), SLOT(libraryViewChanged()));
        connect(libraryCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(libraryCoverSizeChanged()));
    }
    if (enabledViews&V_Albums) {
        connect(albumsView, SIGNAL(currentIndexChanged(int)), SLOT(albumsViewChanged()));
        connect(albumsCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(albumsCoverSizeChanged()));
    }
    if (enabledViews&V_Playlists) {
        connect(playlistsView, SIGNAL(currentIndexChanged(int)), SLOT(playListsStyleChanged()));
    }
    connect(playQueueGrouped, SIGNAL(currentIndexChanged(int)), SLOT(playQueueGroupedChanged()));
    #ifndef ENABLE_DEVICES_SUPPORT
    REMOVE(showDeleteAction);
    REMOVE(showDeleteActionLabel);
    #endif
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnClose, SLOT(setEnabled(bool)));
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnCloseLabel, SLOT(setEnabled(bool)));
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), SLOT(enableStartupState()));
    connect(minimiseOnClose, SIGNAL(toggled(bool)), SLOT(enableStartupState()));
    connect(forceSingleClick, SIGNAL(toggled(bool)), SLOT(forceSingleClickChanged()));
}

void InterfaceSettings::load()
{
    if (enabledViews&V_Artists) {
        libraryArtistImage->setChecked(Settings::self()->libraryArtistImage());
        selectEntry(libraryView, Settings::self()->libraryView());
        libraryCoverSize->setCurrentIndex(Settings::self()->libraryCoverSize());
        libraryYear->setChecked(Settings::self()->libraryYear());
    }
    if (enabledViews&V_Albums) {
        selectEntry(albumsView, Settings::self()->albumsView());
        albumsCoverSize->setCurrentIndex(Settings::self()->albumsCoverSize());
        albumSort->setCurrentIndex(Settings::self()->albumSort());
    }
    if (enabledViews&V_Folders) {
        selectEntry(folderView, Settings::self()->folderView());
    }
    if (enabledViews&V_Playlists) {
        selectEntry(playlistsView, Settings::self()->playlistsView());
        playListsStartClosed->setChecked(Settings::self()->playListsStartClosed());
    }
    if (enabledViews&V_Streams) {
        selectEntry(streamsView, Settings::self()->streamsView());
    }
    if (enabledViews&V_Online) {
        selectEntry(onlineView, Settings::self()->onlineView());
    }
    groupSingle->setChecked(Settings::self()->groupSingle());
    useComposer->setChecked(Settings::self()->useComposer());
    groupMultiple->setCurrentIndex(Settings::self()->groupMultiple() ? 1 : 0);
    #ifdef ENABLE_DEVICES_SUPPORT
    if (showDeleteAction) {
        showDeleteAction->setChecked(Settings::self()->showDeleteAction());
    }
    if (devicesView) {
        selectEntry(devicesView, Settings::self()->devicesView());
    }
    #endif
    playQueueGrouped->setCurrentIndex(Settings::self()->playQueueGrouped() ? 1 : 0);
    playQueueAutoExpand->setChecked(Settings::self()->playQueueAutoExpand());
    playQueueStartClosed->setChecked(Settings::self()->playQueueStartClosed());
    playQueueScroll->setChecked(Settings::self()->playQueueScroll());
    playQueueBackground->setChecked(Settings::self()->playQueueBackground());
    playQueueConfirmClear->setChecked(Settings::self()->playQueueConfirmClear());
    if (enabledViews&V_Albums) {
        albumsViewChanged();
        albumsCoverSizeChanged();
    }
    if (enabledViews&V_Playlists) {
        playListsStyleChanged();
    }
    playQueueGroupedChanged();
    forceSingleClick->setChecked(Settings::self()->forceSingleClick());
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    systemTrayPopup->setChecked(Settings::self()->showPopups());
    minimiseOnClose->setChecked(Settings::self()->minimiseOnClose());
    minimiseOnClose->setEnabled(systemTrayCheckBox->isChecked());
    minimiseOnCloseLabel->setEnabled(systemTrayCheckBox->isChecked());
    selectEntry(startupState, Settings::self()->startupState());
    enableStartupState();
    cacheScaledCovers->setChecked(Settings::self()->cacheScaledCovers());
}

void InterfaceSettings::save()
{
    if (enabledViews&V_Artists) {
        Settings::self()->saveLibraryArtistImage(libraryArtistImage->isChecked());
        Settings::self()->saveLibraryView(getValue(libraryView));
        Settings::self()->saveLibraryCoverSize(libraryCoverSize->currentIndex());
        Settings::self()->saveLibraryYear(libraryYear->isChecked());
    }
    if (enabledViews&V_Albums) {
        Settings::self()->saveAlbumsView(getValue(albumsView));
        Settings::self()->saveAlbumsCoverSize(albumsCoverSize->currentIndex());
        Settings::self()->saveAlbumSort(albumSort->currentIndex());
    }
    if (enabledViews&V_Folders) {
        Settings::self()->saveFolderView(getValue(folderView));
    }
    if (enabledViews&V_Playlists) {
        Settings::self()->savePlaylistsView(getValue(playlistsView));
        Settings::self()->savePlayListsStartClosed(playListsStartClosed->isChecked());
    }
    if (enabledViews&V_Streams) {
        Settings::self()->saveStreamsView(getValue(streamsView));
    }
    if (enabledViews&V_Online) {
        Settings::self()->saveOnlineView(getValue(onlineView));
    }
    Settings::self()->saveGroupSingle(groupSingle->isChecked());
    Settings::self()->saveUseComposer(useComposer->isChecked());
    Settings::self()->saveGroupMultiple(1==groupMultiple->currentIndex());
    #ifdef ENABLE_DEVICES_SUPPORT
    if (showDeleteAction) {
        Settings::self()->saveShowDeleteAction(showDeleteAction->isChecked());
    }
    if (enabledViews&V_Devices) {
        Settings::self()->saveDevicesView(getValue(devicesView));
    }
    #endif
    Settings::self()->savePlayQueueGrouped(1==playQueueGrouped->currentIndex());
    Settings::self()->savePlayQueueAutoExpand(playQueueAutoExpand->isChecked());
    Settings::self()->savePlayQueueStartClosed(playQueueStartClosed->isChecked());
    Settings::self()->savePlayQueueScroll(playQueueScroll->isChecked());
    Settings::self()->savePlayQueueBackground(playQueueBackground->isChecked());
    Settings::self()->savePlayQueueConfirmClear(playQueueConfirmClear->isChecked());
    Settings::self()->saveForceSingleClick(forceSingleClick->isChecked());
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    Settings::self()->saveMinimiseOnClose(minimiseOnClose->isChecked());
    Settings::self()->saveStartupState(getValue(startupState));
    Settings::self()->saveCacheScaledCovers(cacheScaledCovers->isChecked());
}

void InterfaceSettings::libraryViewChanged()
{
    int vt=getValue(libraryView);
    if (ItemView::Mode_IconTop==vt && 0==libraryCoverSize->currentIndex()) {
        libraryCoverSize->setCurrentIndex(2);
    }

    bool isIcon=ItemView::Mode_IconTop==vt;
    bool isSimpleTree=ItemView::Mode_SimpleTree==vt;
    libraryArtistImage->setEnabled(!isIcon && !isSimpleTree);
    libraryArtistImageLabel->setEnabled(libraryArtistImage->isEnabled());
    if (isIcon) {
        libraryArtistImage->setChecked(true);
    } else if (isSimpleTree) {
        libraryArtistImage->setChecked(false);
    }
}

void InterfaceSettings::libraryCoverSizeChanged()
{
    if (ItemView::Mode_IconTop==getValue(libraryView) && 0==libraryCoverSize->currentIndex()) {
        libraryView->setCurrentIndex(1);
    }
    if (0==libraryCoverSize->currentIndex()) {
        libraryArtistImage->setChecked(false);
    }
}

void InterfaceSettings::albumsViewChanged()
{
    if (ItemView::Mode_IconTop==getValue(albumsView) && 0==albumsCoverSize->currentIndex()) {
        albumsCoverSize->setCurrentIndex(2);
    }
}

void InterfaceSettings::albumsCoverSizeChanged()
{
    if (ItemView::Mode_IconTop==getValue(albumsView) && 0==albumsCoverSize->currentIndex()) {
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
    bool grouped=getValue(playlistsView)==ItemView::Mode_GroupedTree;
    playListsStartClosed->setEnabled(grouped);
    playListsStartClosedLabel->setEnabled(grouped);
}

void InterfaceSettings::forceSingleClickChanged()
{
    singleClickLabel->setOn(forceSingleClick->isChecked()!=Settings::self()->forceSingleClick());
}

void InterfaceSettings::enableStartupState()
{
    startupState->setEnabled(systemTrayCheckBox->isChecked() && minimiseOnClose->isChecked());
    startupStateLabel->setEnabled(startupState->isEnabled());
}
