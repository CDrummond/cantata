/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/messagebox.h"
#include "support/actioncollection.h"
#include "models/mpdlibrarymodel.h"
#include "widgets/menubutton.h"
#include "widgets/genrecombo.h"

LibraryPage::LibraryPage(QWidget *p)
    : SinglePageWidget(p)
{
    genreCombo=new GenreCombo(this);
    connect(StdActions::self()->addRandomAlbumToPlayQueueAction, SIGNAL(triggered()), SLOT(addRandomAlbum()));
    connect(MPDConnection::self(), SIGNAL(updatingLibrary(time_t)), view, SLOT(updating()));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), view, SLOT(updated()));
    connect(MPDConnection::self(), SIGNAL(updatingDatabase()), view, SLOT(updating()));
    connect(MPDConnection::self(), SIGNAL(updatedDatabase()), view, SLOT(updated()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    view->setModel(MpdLibraryModel::self());
    connect(MpdLibraryModel::self(), SIGNAL(modelReset()), this, SLOT(modelReset()));

    view->allowCategorized();
    // Settings...
    Configuration config(metaObject()->className());
    view->setMode(ItemView::Mode_DetailedTree);
    MpdLibraryModel::self()->load(config);

    config.beginGroup(SqlLibraryModel::groupingStr(MpdLibraryModel::self()->topLevel()));
    view->load(config);
    view->setSearchToolTip(tr("<p>Enter a string to search artist, album, title, etc. To filter based on year, add <i>#year-range</i> to search string - e.g.</p><ul>"
                              "<li><b><i>#2000</i></b> return tracks from 2000</li>"
                              "<li><b><i>#1980-1989</i></b> return tracks from the 80's</li>"
                              "<li><b><i>Blah #2000</i></b> to search for string <i>Blah</i> and only return tracks from 2000</li>"
                              "</ul></p>"));

    showArtistImagesAction=new QAction(tr("Show Artist Images"), this);
    showArtistImagesAction->setCheckable(true);
    libraryAlbumSortAction=createMenuGroup(tr("Sort Albums"), QList<MenuItem>() << MenuItem(tr("Name"), LibraryDb::AS_AlArYr)
                                                                                  << MenuItem(tr("Year"), LibraryDb::AS_YrAlAr),
                                           MpdLibraryModel::self()->libraryAlbumSort(), this, SLOT(libraryAlbumSortChanged()));
    albumAlbumSortAction=createMenuGroup(tr("Sort Albums"), QList<MenuItem>() << MenuItem(tr("Album, Artist, Year"), LibraryDb::AS_AlArYr)
                                                                                << MenuItem(tr("Album, Year, Artist"), LibraryDb::AS_AlYrAr)
                                                                                << MenuItem(tr("Artist, Album, Year"), LibraryDb::AS_ArAlYr)
                                                                                << MenuItem(tr("Artist, Year, Album"), LibraryDb::AS_ArYrAl)
                                                                                << MenuItem(tr("Year, Album, Artist"), LibraryDb::AS_YrAlAr)
                                                                                << MenuItem(tr("Year, Artist, Album"), LibraryDb::AS_YrArAl)
                                                                                << MenuItem(tr("Modified Date"), LibraryDb::AS_Modified),
                                         MpdLibraryModel::self()->albumAlbumSort(), this, SLOT(albumAlbumSortChanged()));

    MenuButton *menu=new MenuButton(this);
    viewAction=createViewMenu(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                              << ItemView::Mode_DetailedTree << ItemView::Mode_List
                              << ItemView::Mode_IconTop << ItemView::Mode_Categorized);
    menu->addAction(viewAction);

    menu->addAction(createMenuGroup(tr("Group By"), QList<MenuItem>() << MenuItem(tr("Genre"), SqlLibraryModel::T_Genre)
                                                                        << MenuItem(tr("Artist"), SqlLibraryModel::T_Artist)
                                                                        << MenuItem(tr("Album"), SqlLibraryModel::T_Album),
                                    MpdLibraryModel::self()->topLevel(), this, SLOT(groupByChanged())));
    genreCombo->setVisible(SqlLibraryModel::T_Genre!=MpdLibraryModel::self()->topLevel());

    menu->addAction(libraryAlbumSortAction);
    menu->addAction(albumAlbumSortAction);
    showArtistImagesAction->setChecked(MpdLibraryModel::self()->useArtistImages());
    menu->addAction(showArtistImagesAction);
    connect(showArtistImagesAction, SIGNAL(toggled(bool)), this, SLOT(showArtistImagesChanged(bool)));
    showArtistImagesAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel() && ItemView::Mode_IconTop!=view->viewMode());
    albumAlbumSortAction->setVisible(SqlLibraryModel::T_Album==MpdLibraryModel::self()->topLevel());
    libraryAlbumSortAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel());
    genreCombo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    init(ReplacePlayQueue|AppendToPlayQueue, QList<QWidget *>() << menu << genreCombo);
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(doSearch()));
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
    view->setInfoText(tr("No music? Looks like your MPD is not configured correctly."));

    for (QAction *act: viewAction->menu()->actions()) {
        if (ItemView::Mode_Categorized==act->property(constValProp).toInt()) {
            act->setVisible(SqlLibraryModel::T_Album==MpdLibraryModel::self()->topLevel());
            break;
        }
    }
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

            if (SqlLibraryModel::T_Artist==static_cast<SqlLibraryModel::Item *>(selected.first().internalPointer())->getType()) {
                if (s.useComposer()) {
                    s.setComposerImageRequest();
                } else {
                    s.setArtistImageRequest();
                }
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
        if (MessageBox::Yes==MessageBox::warningYesNo(this, tr("Are you sure you wish to delete the selected songs?\n\nThis cannot be undone."),
                                                      tr("Delete Songs"), StdGuiItem::del(), StdGuiItem::cancel())) {
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
    for (const Song &s: songs) {
        if (!s.file.isEmpty() && !s.hasProtocolOrIsAbsolute()) {
            sngs.append(s);
        }
    }

    if (sngs.isEmpty()) {
        return;
    }

    view->clearSearchText();

    bool first=true;
    for (const Song &s: sngs) {
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
            while (idx.isValid()) {
                view->setExpanded(idx);
                idx=idx.parent();
            }
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
            while (idx.isValid()) {
                view->setExpanded(idx);
                idx=idx.parent();
            }
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

void LibraryPage::modelReset()
{
    genreCombo->update(MpdLibraryModel::self()->getGenres());
    int count = MpdLibraryModel::self()->trackCount();
    view->setMinSearchDebounce(count <= 12500 ? 1000u : count <= 18000 ? 1500u : 2000u);
}

void LibraryPage::groupByChanged()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (!act) {
        return;
    }
    int mode=act->property(constValProp).toInt();

    Configuration config(metaObject()->className());
    config.beginGroup(SqlLibraryModel::groupingStr(MpdLibraryModel::self()->topLevel()));
    view->save(config);

    MpdLibraryModel::self()->setTopLevel((SqlLibraryModel::Type)mode);
    albumAlbumSortAction->setVisible(SqlLibraryModel::T_Album==MpdLibraryModel::self()->topLevel());
    libraryAlbumSortAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel());
    showArtistImagesAction->setVisible(SqlLibraryModel::T_Album!=MpdLibraryModel::self()->topLevel() && ItemView::Mode_IconTop!=view->viewMode());
    genreCombo->setVisible(SqlLibraryModel::T_Genre!=MpdLibraryModel::self()->topLevel());

    config.endGroup();
    config.beginGroup(SqlLibraryModel::groupingStr(MpdLibraryModel::self()->topLevel()));
    if (!config.hasEntry(ItemView::constViewModeKey)) {
        view->setMode(SqlLibraryModel::T_Album==mode ? ItemView::Mode_IconTop : ItemView::Mode_DetailedTree);
    }
    view->load(config);
    for (QAction *act: viewAction->menu()->actions()) {
        int viewMode = act->property(constValProp).toInt();
        if (viewMode==view->viewMode()) {
            act->setChecked(true);
        }
        if (ItemView::Mode_Categorized==viewMode) {
            act->setVisible(SqlLibraryModel::T_Album==MpdLibraryModel::self()->topLevel());
        }
        if (ItemView::Mode_IconTop==viewMode) {
            act->setVisible(SqlLibraryModel::T_Genre!=MpdLibraryModel::self()->topLevel());
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
        emit add(files, replace ? MPDConnection::ReplaceAndplay : MPDConnection::Append, 0, false);
    }
}

void LibraryPage::addRandomAlbum()
{
    if (!isVisible()) {
        return;
    }

    QStringList genres;
    QStringList artists;
    QList<SqlLibraryModel::AlbumItem *> albums;
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    for (const QModelIndex &idx: selected) {
        SqlLibraryModel::Item *item=static_cast<SqlLibraryModel::Item *>(idx.internalPointer());
        switch (item->getType()) {
        case SqlLibraryModel::T_Genre:
            genres.append(item->getId());
            break;
        case SqlLibraryModel::T_Artist:
            artists.append(item->getId());
            break;
        case SqlLibraryModel::T_Album:
            // Can only have albums selected if set to group by albums
            // ...controlActions ensures this!
            albums.append(static_cast<SqlLibraryModel::AlbumItem *>(item));
            break;
        default:
            break;
        }
    }

    LibraryDb::Album album;
    if (!albums.isEmpty()) {
        // We have albums selected, so choose a random one of these...
        SqlLibraryModel::AlbumItem *al=albums.at(Utils::random(albums.size()));
        album.artist=al->getArtistId();
        album.id=al->getId();
    } else {
        // If all items selected, then just choose random of all albums
        switch(MpdLibraryModel::self()->topLevel()) {
        case SqlLibraryModel::T_Genre:
            if (genres.size()==MpdLibraryModel::self()->rowCount(QModelIndex())) {
                genres=QStringList();
            }
            break;
        case SqlLibraryModel::T_Artist:
            if (artists.size()==MpdLibraryModel::self()->rowCount(QModelIndex())) {
                artists=QStringList();
            }
            break;
        case SqlLibraryModel::T_Album:
            genres=artists=QStringList();
            break;
        default:
            break;
        }
        album=MpdLibraryModel::self()->getRandomAlbum(genres, artists);
    }

    if (album.artist.isEmpty() || album.id.isEmpty()) {
        return;
    }
    QList<Song> songs=MpdLibraryModel::self()->getAlbumTracks(album.artist, album.id);
    if (!songs.isEmpty()) {
        QStringList files;
        for (const Song &s: songs) {
            files.append(s.file);
        }
        emit add(files, /*replace ? MPDConnection::ReplaceAndplay : */MPDConnection::Append, 0, false);
    }
}

void LibraryPage::doSearch()
{
    MpdLibraryModel::self()->search(view->searchText(),
                                    genreCombo->isHidden() || genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
}

void LibraryPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enable=selected.count()>0;

    CustomActions::self()->setEnabled(enable);
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
        bool groupingAlbums=SqlLibraryModel::T_Album==MpdLibraryModel::self()->topLevel();
        for (const QModelIndex &idx: selected) {
            if (SqlLibraryModel::T_Track==static_cast<SqlLibraryModel::Item *>(idx.internalPointer())->getType() ||
                (!groupingAlbums && SqlLibraryModel::T_Album==static_cast<SqlLibraryModel::Item *>(idx.internalPointer())->getType())) {
                allowRandomAlbum=false;
                break;
            }
        }
    }
    StdActions::self()->addRandomAlbumToPlayQueueAction->setVisible(allowRandomAlbum);
}

#include "moc_librarypage.cpp"
