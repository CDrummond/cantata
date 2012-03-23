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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

static void addViewTypes(QComboBox *box, bool iconMode=false, bool groupedTree=false)
{
    #ifdef ENABLE_KDE_SUPPORT
    box->addItem(i18n("Tree"), ItemView::Mode_Tree);
    if (groupedTree) {
        box->addItem(i18n("Grouped Albums"), ItemView::Mode_GroupedTree);
    }
    box->addItem(i18n("List"), ItemView::Mode_List);
    if (iconMode) {
        box->addItem(i18n("Icon/List"), ItemView::Mode_IconTop);
    }
    #else
    box->addItem(QObject::tr("Tree"), ItemView::Mode_Tree);
    if (groupedTree) {
        box->addItem(QObject::tr("Grouped Albums"), ItemView::Mode_GroupedTree);
    }
    box->addItem(QObject::tr("List"), ItemView::Mode_List);
    if (iconMode) {
        box->addItem(QObject::tr("Icon/List"), ItemView::Mode_IconTop);
    }
    #endif
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
    addViewTypes(libraryView);
    addViewTypes(albumsView, true);
    addViewTypes(folderView);
    addViewTypes(playlistsView, false, true);
    addViewTypes(streamsView);
    #ifdef ENABLE_DEVICES_SUPPORT
    addViewTypes(devicesView);
    #endif
    #ifdef ENABLE_KDE_SUPPORT
    groupMultiple->addItem(i18n("Grouped by \'Album Artist\'"));
    groupMultiple->addItem(i18n("Grouped under \'Various Artists\'"));
    #else
    groupMultiple->addItem(tr("Grouped by \'Album Artist\'"));
    groupMultiple->addItem(tr("Grouped under \'Various Artists\'"));
    #endif
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
};

void InterfaceSettings::load()
{
    selectEntry(libraryView, Settings::self()->libraryView());
    libraryCoverSize->setCurrentIndex(Settings::self()->libraryCoverSize());
    libraryYear->setChecked(Settings::self()->libraryYear());
    selectEntry(albumsView, Settings::self()->albumsView());
    albumsCoverSize->setCurrentIndex(Settings::self()->albumsCoverSize());
    albumFirst->setCurrentIndex(Settings::self()->albumFirst() ? 0 : 1);
    selectEntry(folderView, Settings::self()->folderView());
    selectEntry(playlistsView, Settings::self()->playlistsView());
    playListsStartClosed->setChecked(Settings::self()->playListsStartClosed());
    selectEntry(streamsView, Settings::self()->streamsView());
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
    albumsViewChanged();
    albumsCoverSizeChanged();
    playListsStyleChanged();
    playQueueGroupedChanged();
}

void InterfaceSettings::save()
{
    Settings::self()->saveLibraryView(getViewType(libraryView));
    Settings::self()->saveLibraryCoverSize(libraryCoverSize->currentIndex());
    Settings::self()->saveLibraryYear(libraryYear->isChecked());
    Settings::self()->saveAlbumsView(getViewType(albumsView));
    Settings::self()->saveAlbumsCoverSize(albumsCoverSize->currentIndex());
    Settings::self()->saveAlbumFirst(0==albumFirst->currentIndex());
    Settings::self()->saveFolderView(getViewType(folderView));
    Settings::self()->savePlaylistsView(getViewType(playlistsView));
    Settings::self()->savePlayListsStartClosed(playListsStartClosed->isChecked());
    Settings::self()->saveStreamsView(getViewType(streamsView));
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
