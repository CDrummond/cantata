/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "librarypage.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdstats.h"
#include "covers.h"
#include "settings.h"
#include "stdactions.h"
#include "customactions.h"
#include "support/utils.h"
#include "support/localize.h"
#include "support/messagebox.h"
#include "support/actioncollection.h"
//#include "support/combobox.h"
#include "models/mpdlibrarymodel.h"
#include "widgets/menubutton.h"

LibraryPage::LibraryPage(QWidget *p)
    : SinglePageWidget(p)
{
    connect(StdActions::self()->addRandomAlbumToPlayQueueAction, SIGNAL(triggered()), SLOT(addRandomAlbum()));
    connect(MPDConnection::self(), SIGNAL(updatingLibrary(time_t)), view, SLOT(updating()));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), view, SLOT(updated()));
    connect(MPDConnection::self(), SIGNAL(updatingDatabase()), view, SLOT(updating()));
    connect(MPDConnection::self(), SIGNAL(updatedDatabase()), view, SLOT(updated()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    view->setModel(MpdLibraryModel::self());

//    groupCombo=new ComboBox(this);
//    groupCombo->addItem(i18n("Genre"), SqlLibraryModel::T_Genre);
//    groupCombo->addItem(i18n("Atists"), SqlLibraryModel::T_Artist);
//    groupCombo->addItem(i18n("Albums"), SqlLibraryModel::T_Album);

    // Settings...
    Configuration config(metaObject()->className());
    view->setMode(ItemView::Mode_DetailedTree);
    MpdLibraryModel::self()->load(config);
//    for (int i=0; i<groupCombo->count(); ++i) {
//        if (groupCombo->itemData(i).toInt()==MpdLibraryModel::self()->topLevel()) {
//            groupCombo->setCurrentIndex(i);
//            break;
//        }
//    }

    config.beginGroup(SqlLibraryModel::groupingStr(MpdLibraryModel::self()->topLevel()));
    view->load(config);

    showArtistImagesAction=new QAction(i18n("Show Artist Images"), this);
    showArtistImagesAction->setCheckable(true);
    libraryAlbumSortAction=createMenuGroup(i18n("Sort Albums"), QList<MenuItem>() << MenuItem(i18n("Name"), LibraryDb::AS_Album)
                                                                                  << MenuItem(i18n("Year"), LibraryDb::AS_Year),
                                           MpdLibraryModel::self()->libraryAlbumSort(), this, SLOT(libraryAlbumSortChanged()));
    albumAlbumSortAction=createMenuGroup(i18n("Sort Albums"), QList<MenuItem>() << MenuItem(i18n("Name"), LibraryDb::AS_Album)
                                                                                << MenuItem(i18n("Artist"), LibraryDb::AS_Artist)
                                                                                << MenuItem(i18n("Year"), LibraryDb::AS_Year)
                                                                                << MenuItem(i18n("Modified Date"), LibraryDb::AS_Modified),
                                         MpdLibraryModel::self()->albumAlbumSort(), this, SLOT(albumAlbumSortChanged()));

    MenuButton *menu=new MenuButton(this);
    viewAction=createViewMenu(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                              << ItemView::Mode_DetailedTree << ItemView::Mode_List
                              << ItemView::Mode_IconTop);
    menu->addAction(viewAction);

    menu->addAction(createMenuGroup(i18n("Group By"), QList<MenuItem>() << MenuItem(i18n("Genre"), SqlLibraryModel::T_Genre)
                                                                        << MenuItem(i18n("Artist"), SqlLibraryModel::T_Artist)
                                                                        << MenuItem(i18n("Album"), SqlLibraryModel::T_Album),
                                    MpdLibraryModel::self()->topLevel(), this, SLOT(groupByChanged())));

    menu->addAction(libraryAlbumSortAction);
    menu->addAction(albumAlbumSortAction);
    showArtistImagesAction->setChecked(MpdLibraryModel::self()->useArtistImages());
    menu->addAction(showArtistImagesAction);
    connect(showArtistImagesAction, SIGNAL(toggled(bool)), this, SLOT(showArtistImagesChanged(bool)));
    albumAlbumSortAction->setVisible(SqlLibraryModel::T_Album==MpdLibraryModel::self()->topLevel());
    libraryAlbumSortAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel());
    showArtistImagesAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel());
    init(ReplacePlayQueue|AppendToPlayQueue, QList<QWidget *>() << menu/* << groupCombo*/);
//    connect(groupCombo, SIGNAL(activated(int)), SLOT(groupByChanged()));
    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    view->addAction(CustomActions::self());
    #ifdef TAGLIB_FOUND
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(StdActions::self()->copyToDeviceAction);
    #endif
    view->addAction(StdActions::self()->organiseFilesAction);
    view->addAction(StdActions::self()->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(StdActions::self()->replaygainAction);
    #endif
    view->addAction(StdActions::self()->setCoverAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addSeparator();
    view->addAction(StdActions::self()->deleteSongsAction);
    #endif
    #endif // TAGLIB_FOUND
    connect(view, SIGNAL(updateToPlayQueue(QModelIndex,bool)), this, SLOT(updateToPlayQueue(QModelIndex,bool)));
    view->setOpenAfterSearch(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel());
}

LibraryPage::~LibraryPage()
{
    Configuration config(metaObject()->className());
    MpdLibraryModel::self()->save(config);
    config.beginGroup(SqlLibraryModel::groupingStr(MpdLibraryModel::self()->topLevel()));
    view->save(config);
}

void LibraryPage::clear()
{
    MpdLibraryModel::self()->clear();
    view->goToTop();
}

static inline QString nameKey(const QString &artist, const QString &album)
{
    return '{'+artist+"}{"+album+'}';
}

QStringList LibraryPage::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }
    return MpdLibraryModel::self()->filenames(selected, allowPlaylists);
}

QList<Song> LibraryPage::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return MpdLibraryModel::self()->songs(selected, allowPlaylists);
}

Song LibraryPage::coverRequest() const
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1==selected.count()) {
        QList<Song> songs=MpdLibraryModel::self()->songs(QModelIndexList() << selected.first(), false);
        if (!songs.isEmpty()) {
            Song s=songs.at(0);

            if (SqlLibraryModel::T_Artist==static_cast<SqlLibraryModel::Item *>(selected.first().internalPointer())->getType() && !s.useComposer()) {
                s.setArtistImageRequest();
            }
            return s;
        }
    }
    return Song();
}

#ifdef ENABLE_DEVICES_SUPPORT
void LibraryPage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void LibraryPage::deleteSongs()
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Are you sure you wish to delete the selected songs?\n\nThis cannot be undone."),
                                                      i18n("Delete Songs"), StdGuiItem::del(), StdGuiItem::cancel())) {
            emit deleteSongs(QString(), songs);
        }
        view->clearSelection();
    }
}
#endif

void LibraryPage::showSongs(const QList<Song> &songs)
{
    // Filter out non-mpd file songs...
    QList<Song> sngs;
    foreach (const Song &s, songs) {
        if (!s.file.isEmpty() && !s.file.contains(":/") && !s.file.startsWith('/')) {
            sngs.append(s);
        }
    }

    if (sngs.isEmpty()) {
        return;
    }

    view->clearSearchText();

    bool first=true;
    foreach (const Song &s, sngs) {
        QModelIndex idx=MpdLibraryModel::self()->findSongIndex(s);
        if (idx.isValid()) {
            if (ItemView::Mode_SimpleTree==view->viewMode() || ItemView::Mode_DetailedTree==view->viewMode() || first) {
                view->showIndex(idx, first);
            }
            if (first) {
                first=false;
            }
            if (ItemView::Mode_SimpleTree!=view->viewMode() && ItemView::Mode_DetailedTree!=view->viewMode()) {
                return;
            }
        }
    }
}

void LibraryPage::showArtist(const QString &artist)
{
    view->clearSearchText();
    QModelIndex idx=MpdLibraryModel::self()->findArtistIndex(artist);
    if (idx.isValid()) {
        view->showIndex(idx, true);
        if (ItemView::Mode_SimpleTree==view->viewMode() || ItemView::Mode_DetailedTree==view->viewMode()) {
            view->setExpanded(idx);
        }
    }
}

void LibraryPage::showAlbum(const QString &artist, const QString &album)
{
    view->clearSearchText();
    QModelIndex idx=MpdLibraryModel::self()->findAlbumIndex(artist, album);
    if (idx.isValid()) {
        view->showIndex(idx, true);
        if (ItemView::Mode_SimpleTree==view->viewMode() || ItemView::Mode_DetailedTree==view->viewMode()) {
            view->setExpanded(idx.parent());
            view->setExpanded(idx);
        }
    }
}

void LibraryPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return; //doubleclick should only have one selected item
    }
    SqlLibraryModel::Item *item = static_cast<SqlLibraryModel::Item *>(selected.at(0).internalPointer());
    if (SqlLibraryModel::T_Track==item->getType()) {
        addSelectionToPlaylist();
    }
}

void LibraryPage::setView(int v)
{
    SinglePageWidget::setView(v);
    showArtistImagesAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel() && ItemView::Mode_IconTop!=view->viewMode());
}

void LibraryPage::groupByChanged()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (!act) {
        return;
    }
    int mode=act->property(constValProp).toInt();
//    int mode=groupCombo->itemData(groupCombo->currentIndex()).toInt();

    Configuration config(metaObject()->className());
    config.beginGroup(SqlLibraryModel::groupingStr(MpdLibraryModel::self()->topLevel()));
    view->save(config);

    MpdLibraryModel::self()->setTopLevel((SqlLibraryModel::Type)mode);
    albumAlbumSortAction->setVisible(SqlLibraryModel::T_Album==MpdLibraryModel::self()->topLevel());
    libraryAlbumSortAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel());
    showArtistImagesAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel() && ItemView::Mode_IconTop!=view->viewMode());

    config.endGroup();
    config.beginGroup(SqlLibraryModel::groupingStr(MpdLibraryModel::self()->topLevel()));
    if (!config.hasEntry(ItemView::constViewModeKey)) {
        view->setMode(SqlLibraryModel::T_Album==mode ? ItemView::Mode_IconTop : ItemView::Mode_DetailedTree);
    }
    view->load(config);
    foreach (QAction *act, viewAction->menu()->actions()) {
        if (act->property(constValProp).toInt()==view->viewMode()) {
            act->setChecked(true);
            break;
        }
    }
    view->setOpenAfterSearch(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel());
}

void LibraryPage::libraryAlbumSortChanged()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (act) {
        MpdLibraryModel::self()->setLibraryAlbumSort((LibraryDb::AlbumSort)act->property(constValProp).toInt());
    }
}

void LibraryPage::albumAlbumSortChanged()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (act) {
        MpdLibraryModel::self()->setAlbumAlbumSort((LibraryDb::AlbumSort)act->property(constValProp).toInt());
    }
}

void LibraryPage::showArtistImagesChanged(bool u)
{
    MpdLibraryModel::self()->setUseArtistImages(u);
}

void LibraryPage::updateToPlayQueue(const QModelIndex &idx, bool replace)
{
    QStringList files=MpdLibraryModel::self()->filenames(QModelIndexList() << idx, true);
    if (!files.isEmpty()) {
        emit add(files, replace ? MPDConnection::ReplaceAndplay : MPDConnection::Append, 0);
    }
}

void LibraryPage::addRandomAlbum()
{
    if (!isVisible()) {
        return;
    }
    QStringList genres;
    QStringList artists;
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    foreach (const QModelIndex &idx, selected) {
        SqlLibraryModel::Item *item=static_cast<SqlLibraryModel::Item *>(idx.internalPointer());
        switch (item->getType()) {
        case SqlLibraryModel::T_Genre:
            genres.append(item->getId());
            break;
        case SqlLibraryModel::T_Artist:
            artists.append(item->getId());
        default:
            break;
        }
    }

    // If all items selected, then just choose random of all albums
    if (SqlLibraryModel::T_Genre==MpdLibraryModel::self()->topLevel() && genres.size()==MpdLibraryModel::self()->rowCount(QModelIndex())) {
        genres=QStringList();
    }
    if (SqlLibraryModel::T_Artist==MpdLibraryModel::self()->topLevel() && artists.size()==MpdLibraryModel::self()->rowCount(QModelIndex())) {
        artists=QStringList();
    }

    LibraryDb::Album album=MpdLibraryModel::self()->getRandomAlbum(genres, artists);
    if (album.artist.isEmpty() || album.id.isEmpty()) {
        return;
    }
    QList<Song> songs=MpdLibraryModel::self()->getAlbumTracks(album.artist, album.id);
    if (!songs.isEmpty()) {
        QStringList files;
        foreach (const Song &s, songs) {
            files.append(s.file);
        }
        emit add(files, /*replace ? MPDConnection::ReplaceAndplay : */MPDConnection::Append, 0);
    }
}

void LibraryPage::doSearch()
{
    MpdLibraryModel::self()->search(view->searchText());
}

void LibraryPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enable=selected.count()>0;

    StdActions::self()->enableAddToPlayQueue(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enable);
    #ifdef TAGLIB_FOUND
    StdActions::self()->organiseFilesAction->setEnabled(enable && MPDConnection::self()->getDetails().dirReadable);
    StdActions::self()->editTagsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    StdActions::self()->replaygainAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->deleteSongsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    StdActions::self()->copyToDeviceAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #endif // TAGLIB_FOUND

    if (1==selected.count()) {
        SqlLibraryModel::Item *item=static_cast<SqlLibraryModel::Item *>(selected.at(0).internalPointer());
        SqlLibraryModel::Type type=item->getType();
        StdActions::self()->setCoverAction->setEnabled((SqlLibraryModel::T_Artist==type/* && !static_cast<MusicLibraryItemArtist *>(item)->isComposer()*/) ||
                                                        SqlLibraryModel::T_Album==type);
    } else {
        StdActions::self()->setCoverAction->setEnabled(false);
    }

    bool allowRandomAlbum=isVisible() && !selected.isEmpty();
    if (allowRandomAlbum) {
        foreach (const QModelIndex &idx, selected) {
            if (SqlLibraryModel::T_Track==static_cast<SqlLibraryModel::Item *>(idx.internalPointer())->getType() ||
                SqlLibraryModel::T_Album==static_cast<SqlLibraryModel::Item *>(idx.internalPointer())->getType()) {
                allowRandomAlbum=false;
                break;
            }
        }
    }
    StdActions::self()->addRandomAlbumToPlayQueueAction->setVisible(allowRandomAlbum);
}
