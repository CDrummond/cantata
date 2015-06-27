/*
 * Cantata
 *
 * Copyright (c) 2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "sqllibrarymodel.h"
#include "playqueuemodel.h"
#include "gui/plurals.h"
#include "gui/settings.h"
#include "widgets/icons.h"
#include "support/localize.h"
#include "support/configuration.h"
#include "roles.h"
#include <QMimeData>
#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

static QString parentData(const SqlLibraryModel::Item *i)
{
    QString data;
    const SqlLibraryModel::Item *itm=i;

    while (itm->getParent()) {
        if (!itm->getParent()->getText().isEmpty()) {
            if (SqlLibraryModel::T_Root==itm->getParent()->getType()) {
                data="<b>"+itm->getParent()->getText()+"</b><br/>"+data;
            } else {
                data=itm->getParent()->getText()+"<br/>"+data;
            }
        }
        itm=itm->getParent();
    }
    return data;
}

const QLatin1String SqlLibraryModel::constGroupGenre("genre");
const QLatin1String SqlLibraryModel::constGroupArtist("artist");
const QLatin1String SqlLibraryModel::constGroupAlbum("album");

SqlLibraryModel::SqlLibraryModel(LibraryDb *d, QObject *p, Type top)
    : ActionModel(p)
    , tl(top)
    , root(0)
    , db(d)
    , librarySort(LibraryDb::AS_Year)
    , albumSort(LibraryDb::AS_Album)
{
    connect(db, SIGNAL(libraryUpdated()), SLOT(libraryUpdated()));
    #if defined ENABLE_MODEL_TEST
    new ModelTest(this, this);
    #endif
}

void SqlLibraryModel::clear()
{
    beginResetModel();
    delete root;
    root=0;
    endResetModel();
}

void SqlLibraryModel::clearDb()
{
    db->clear();
}

void SqlLibraryModel::settings(Type top, LibraryDb::AlbumSort lib, LibraryDb::AlbumSort al)
{
    bool changed=top!=tl || (T_Album!=top && lib!=librarySort) || (T_Album==top && al!=albumSort);
    tl=top;
    librarySort=lib;
    albumSort=al;
    if (changed) {
        libraryUpdated();
    }
}

void SqlLibraryModel::setTopLevel(Type t)
{
    if (t!=tl) {
        tl=t;
        libraryUpdated();
    }
}

void SqlLibraryModel::setLibraryAlbumSort(LibraryDb::AlbumSort s)
{
    if (s!=librarySort) {
        librarySort=s;
        if (T_Album!=tl) {
            libraryUpdated();
        }
    }
}

void SqlLibraryModel::setAlbumAlbumSort(LibraryDb::AlbumSort s)
{
    if (s!=albumSort) {
        albumSort=s;
        if (T_Album==tl) {
            libraryUpdated();
        }
    }
}

static QLatin1String constAlbumSortKey("albumSort");
static QLatin1String constLibrarySortKey("librarySort");

void SqlLibraryModel::load(Configuration &config)
{
    albumSort=LibraryDb::toAlbumSort(config.get(constAlbumSortKey, LibraryDb::albumSortStr(albumSort)));
    librarySort=LibraryDb::toAlbumSort(config.get(constLibrarySortKey, LibraryDb::albumSortStr(librarySort)));
}

void SqlLibraryModel::save(Configuration &config)
{
    config.set(constAlbumSortKey, LibraryDb::albumSortStr(albumSort));
    config.set(constLibrarySortKey, LibraryDb::albumSortStr(librarySort));
}

void SqlLibraryModel::libraryUpdated()
{
    beginResetModel();
    delete root;
    root=new CollectionItem(T_Root, QString());
    switch (tl) {
    case T_Genre: {
        QList<LibraryDb::Genre> genres=db->getGenres();
        if (!genres.isEmpty())  {
            foreach (const LibraryDb::Genre &genre, genres) {
                root->add(new CollectionItem(T_Genre, genre.name, genre.name, Plurals::artists(genre.artistCount), root));
            }
        }
        break;
    }
    case T_Artist: {
        QList<LibraryDb::Artist> artists=db->getArtists();
        if (!artists.isEmpty())  {
            foreach (const LibraryDb::Artist &artist, artists) {
                root->add(new CollectionItem(T_Artist, artist.name, artist.name, Plurals::albums(artist.albumCount), root));
            }
        }
        break;
    }
    case T_Album: {
        QList<LibraryDb::Album> albums=db->getAlbums(QString(), QString(), albumSort);
        if (!albums.isEmpty())  {
            foreach (const LibraryDb::Album &album, albums) {
                root->add(new AlbumItem(album.artist, album.id, Song::displayAlbum(album.name, album.year),
                                        T_Album==tl
                                            ? album.artist
                                            : Plurals::tracksWithDuration(album.trackCount, Utils::formatTime(album.duration, true)), root));
            }
        }
        break;
    }
    default:
        break;
    }

    endResetModel();
}

void SqlLibraryModel::search(const QString &str)
{
    if (db->setFilter(str)) {
        libraryUpdated();
    }
}

Qt::ItemFlags SqlLibraryModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    }
    return Qt::ItemIsDropEnabled;
}

QModelIndex SqlLibraryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    const CollectionItem * p = parent.isValid() ? static_cast<CollectionItem *>(parent.internalPointer()) : root;
    const Item * c = row<p->getChildCount() ? p->getChildren().at(row) : 0;
    return c ? createIndex(row, column, (void *)c) : QModelIndex();
}

QModelIndex SqlLibraryModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }

    const Item * const item = static_cast<Item *>(child.internalPointer());
    Item * const parentItem = item->getParent();
    if (parentItem == root || 0==parentItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->getRow(), 0, parentItem);
}

int SqlLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    const CollectionItem *parentItem=parent.isValid() ? static_cast<CollectionItem *>(parent.internalPointer()) : root;
    return parentItem ? parentItem->getChildCount() : 0;
}

int SqlLibraryModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool SqlLibraryModel::hasChildren(const QModelIndex &index) const
{
    Item *item=toItem(index);
    return item && T_Track!=item->getType();
}

bool SqlLibraryModel::canFetchMore(const QModelIndex &index) const
{
    if (index.isValid()) {
        Item *item = toItem(index);
        return item && T_Track!=item->getType() && 0==item->getChildCount();
    } else {
        return false;
    }
}

void SqlLibraryModel::fetchMore(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    CollectionItem *item = static_cast<CollectionItem *>(toItem(index));
    switch (item->getType()) {
    case T_Root:
        break;
    case T_Genre: {
        QList<LibraryDb::Artist> artists=db->getArtists(item->getId());
        if (!artists.isEmpty())  {
            beginInsertRows(index, 0, artists.count()-1);
            foreach (const LibraryDb::Artist &artist, artists) {
                item->add(new CollectionItem(T_Artist, artist.name, artist.name, Plurals::albums(artist.albumCount), item));
            }
            endInsertRows();
        }
        break;
    }
    case T_Artist: {
        QList<LibraryDb::Album> albums=db->getAlbums(item->getId(), T_Genre==tl ? item->getParent()->getId() : QString(), librarySort);
        if (!albums.isEmpty())  {
            beginInsertRows(index, 0, albums.count()-1);
            foreach (const LibraryDb::Album &album, albums) {
                item->add(new CollectionItem(T_Album, album.id, Song::displayAlbum(album.name, album.year),
                                             Plurals::tracksWithDuration(album.trackCount, Utils::formatTime(album.duration, true)), item));
            }
            endInsertRows();
        }
        break;
    }
    case T_Album: {
        QList<Song> songs=T_Album==tl
                            ? db->getTracks(static_cast<AlbumItem *>(item)->getArtistId(), item->getId(), QString(), albumSort)
                            : db->getTracks(item->getParent()->getId(), item->getId(),
                                            T_Genre==tl ? item->getParent()->getParent()->getId() : QString(), librarySort);

        if (!songs.isEmpty())  {
            beginInsertRows(index, 0, songs.count()-1);
            foreach (const Song &song, songs) {
                item->add(new TrackItem(song, item));
            }
            endInsertRows();
        }
        break;
    }
    default:
        break;
    }
}

QVariant SqlLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Item *item = static_cast<Item *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (item->getType()) {
        case T_Artist:
            return /*item-> composer? ? Icons::self()->composerIcon :*/ Icons::self()->artistIcon;
        case T_Album:
            return Icons::self()->albumIcon;
        case T_Track:
            return Song::Playlist==item->getSong().type ? Icons::self()->playlistIcon : Icons::self()->audioFileIcon;
        default:
            return QVariant();
        }
    case Cantata::Role_LoadCoverInUIThread:
        return T_Album==item->getType() && T_Album!=tl;
    case Cantata::Role_BriefMainText:
        if (T_Album==item->getType()) {
            return item->getText();
        }
    case Cantata::Role_MainText:
    case Qt::DisplayRole:
        if (T_Track==item->getType()) {
            TrackItem *track = static_cast<TrackItem *>(item);
            if (Song::Playlist==track->getSong().type) {
                return track->getSong().isCueFile() ? i18n("Cue Sheet") : i18n("Playlist");
            }
        }
        return item->getText();
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        if (T_Track==item->getType()) {
            return static_cast<TrackItem *>(item)->getSong().toolTip();
        }
        return parentData(item)+
                (0==item->getChildCount()
                 ? item->getText()
                 : (item->getText()+"<br/>"+data(index, Cantata::Role_SubText).toString()));
    case Cantata::Role_SubText:
        return item->getSubText();
    case Cantata::Role_ListImage:
        return T_Album==item->getType();
    case Cantata::Role_TitleText:
        return item->getText();
    default:
        break;
    }
    return ActionModel::data(index, role);
}

QMimeData * SqlLibraryModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files=filenames(indexes, true);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
    return mimeData;
}

static bool containsParent(const QSet<QModelIndex> &set, const QModelIndex &idx)
{
    QModelIndex p=idx.parent();
    while (p.isValid()) {
        if (set.contains(p)) {
            return true;
        }
        p=p.parent();
    }
    return false;
}

QList<Song> SqlLibraryModel::songs(const QModelIndexList &list, bool allowPlaylists) const
{
    QList<Song> songList;
    QSet<QModelIndex> set=list.toSet();
    populate(list);
    foreach (const QModelIndex &idx, list) {
        if (!containsParent(set, idx)) {
            songList+=songs(idx, allowPlaylists);
        }
    }

    return songList;
}

QStringList SqlLibraryModel::filenames(const QModelIndexList &list, bool allowPlaylists) const
{
    QStringList files;
    QList<Song> songList=songs(list, allowPlaylists);
    foreach (const Song &s, songList) {
        files.append(s.file);
    }
    return files;
}

QModelIndex SqlLibraryModel::findSongIndex(const Song &song)
{
    if (root) {
        QModelIndex albumIndex=findAlbumIndex(song.artistOrComposer(), song.album);
        if (albumIndex.isValid()) {
            if (canFetchMore(albumIndex)) {
                fetchMore(albumIndex);
            }
            CollectionItem *al=static_cast<CollectionItem *>(albumIndex.internalPointer());
            foreach (Item *t, al->getChildren()) {
                if (static_cast<TrackItem *>(t)->getSong().title==song.title) {
                    return index(al->getRow(), 0, albumIndex);
                }
            }
        }
    }
    return QModelIndex();
}

QModelIndex SqlLibraryModel::findAlbumIndex(const QString &artist, const QString &album)
{
    if (root) {
        if (T_Album==tl) {
            foreach (Item *a, root->getChildren()) {
                if (a->getId()==album && static_cast<AlbumItem *>(a)->getArtistId()==artist) {
                    return index(a->getRow(), 0, QModelIndex());
                }
            }
        } else {
            QModelIndex artistIndex=findArtistIndex(artist);
            if (artistIndex.isValid()) {
                if (canFetchMore(artistIndex)) {
                    fetchMore(artistIndex);
                }
                CollectionItem *ar=static_cast<CollectionItem *>(artistIndex.internalPointer());
                foreach (Item *al, ar->getChildren()) {
                    if (al->getId()==album) {
                        return index(al->getRow(), 0, artistIndex);
                    }
                }
            }
        }
    }
    return QModelIndex();
}

QModelIndex SqlLibraryModel::findArtistIndex(const QString &artist)
{
    if (root) {
        if (T_Genre==tl) {
            foreach (Item *g, root->getChildren()) {
                QModelIndex gIndex=index(g->getRow(), 0, QModelIndex());
                if (canFetchMore(gIndex)) {
                    fetchMore(gIndex);
                }
                foreach (Item *a, static_cast<CollectionItem *>(g)->getChildren()) {
                    if (a->getId()==artist) {
                        return index(a->getRow(), 0, gIndex);
                    }
                }
            }
        } if (T_Artist==tl) {
            foreach (Item *a, root->getChildren()) {
                if (a->getId()==artist) {
                    return index(a->getRow(), 0, QModelIndex());
                }
            }
        }
    }
    return QModelIndex();
}

QSet<QString> SqlLibraryModel::getArtists() const
{
    return db->get("albumArtist");
}

QList<Song> SqlLibraryModel::getAlbumTracks(const Song &song) const
{
    return db->getTracks(song.artistOrComposer(), song.album, QString(), LibraryDb::AS_Artist, false);
}

QList<Song> SqlLibraryModel::songs(const QStringList &files, bool allowPlaylists) const
{
    return db->songs(files, allowPlaylists);
}

QList<LibraryDb::Album> SqlLibraryModel::getArtistAlbums(const QString &artist) const
{
    return db->getAlbumsWithArtist(artist);
}

void SqlLibraryModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres)
{
    db->getDetails(artists, albumArtists, composers, albums, genres);
}

bool SqlLibraryModel::songExists(const Song &song)
{
    return db->songExists(song);
}

void SqlLibraryModel::populate(const QModelIndexList &list) const
{
    foreach (const QModelIndex &idx, list) {
        if (canFetchMore(idx)) {
            const_cast<SqlLibraryModel *>(this)->fetchMore(idx);
            populate(children(idx));
        }
    }
}

QModelIndexList SqlLibraryModel::children(const QModelIndex &parent) const
{
    QModelIndexList list;

    for(int r=0; r<rowCount(parent); ++r) {
        list.append(index(r, 0, parent));
    }
    return list;
}

QList<Song> SqlLibraryModel::songs(const QModelIndex &idx, bool allowPlaylists) const
{
    QList<Song> list;
    if (hasChildren(idx)) {
        foreach (const QModelIndex &c, children(idx)) {
            list+=songs(c, allowPlaylists);
        }
    } else {
        const Item *i=static_cast<const Item *>(idx.internalPointer());
        if (i && T_Track==i->getType() && (allowPlaylists || Song::Playlist!=i->getSong().type)) {
            Song s(i->getSong());
            list.append(fixPath(s));
        }
    }
    return list;
}

void SqlLibraryModel::CollectionItem::add(Item *i)
{
    children.append(i);
    i->setRow(children.count()-1);
    childMap.insert(i->getUniqueId(), i);
}

const SqlLibraryModel::Item *SqlLibraryModel::CollectionItem::getChild(const QString &id) const
{
    QMap<QString, Item *>::ConstIterator it=childMap.find(id);
    return childMap.constEnd()==it ? 0 : it.value();
}
