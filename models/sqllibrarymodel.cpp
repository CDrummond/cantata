/*
 * Cantata
 *
 * Copyright (c) 2017-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "gui/settings.h"
#include "widgets/icons.h"
#include "support/configuration.h"
#include "roles.h"
#include <QMimeData>
#include <time.h>

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

SqlLibraryModel::Type SqlLibraryModel::toGrouping(const QString &str)
{
    for (int i=T_Genre; i<T_Track; ++i) {
        if (groupingStr((Type)i)==str) {
            return (Type)i;
        }
    }
    return T_Artist;
}

QString SqlLibraryModel::groupingStr(Type m)
{
    switch(m) {
    default:
    case T_Artist: return "artist";
    case T_Album:  return "album";
    case T_Genre:  return "genre";
    }
}

SqlLibraryModel::SqlLibraryModel(LibraryDb *d, QObject *p, Type top)
    : ActionModel(p)
    , tl(top)
    , root(nullptr)
    , db(d)
    , librarySort(LibraryDb::AS_YrAlAr)
    , albumSort(LibraryDb::AS_AlArYr)
{
    connect(db, SIGNAL(libraryUpdated()), SLOT(libraryUpdated()));
    connect(db, SIGNAL(error(QString)), this, SIGNAL(error(QString)));
}

void SqlLibraryModel::clear()
{
    beginResetModel();
    delete root;
    root=nullptr;
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

static QLatin1String constGroupingKey("grouping");
static QLatin1String constAlbumSortKey("albumSort");
static QLatin1String constLibrarySortKey("librarySort");

void SqlLibraryModel::load(Configuration &config)
{
    tl=toGrouping(config.get(constGroupingKey, groupingStr(tl)));
    albumSort=LibraryDb::toAlbumSort(config.get(constAlbumSortKey, LibraryDb::albumSortStr(albumSort)));
    librarySort=LibraryDb::toAlbumSort(config.get(constLibrarySortKey, LibraryDb::albumSortStr(librarySort)));
}

void SqlLibraryModel::save(Configuration &config)
{
    config.set(constGroupingKey, groupingStr(tl));
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
            for (const LibraryDb::Genre &genre: genres) {
                root->add(new CollectionItem(T_Genre, genre.name, genre.name, tr("%n Artist(s)", "", genre.artistCount), root));
            }
        }
        break;
    }
    case T_Artist: {
        QList<LibraryDb::Artist> artists=db->getArtists();
        if (!artists.isEmpty())  {
            for (const LibraryDb::Artist &artist: artists) {
                root->add(new CollectionItem(T_Artist, artist.name, artist.name, tr("%n Album(s)", "", artist.albumCount), root));
            }
        }
        break;
    }
    case T_Album: {
        QList<LibraryDb::Album> albums=db->getAlbums(QString(), QString(), albumSort);
        categories.clear();
        if (!albums.isEmpty())  {
            time_t now = time(nullptr);
            const time_t aDay = 24 * 60 * 60;
            time_t week = now - (7 * aDay);
            time_t week2 = now - (14 * aDay);
            time_t days30 = now - (30 * aDay);
            time_t days60 = now - (60 * aDay);
            time_t days90 = now - (90 * aDay);
            time_t year = now - (365 * aDay);
            time_t year2 = now - (365 * 2 * aDay);
            time_t year3 = now - (35 * 3 * aDay);

            QMap<QString, int> knownCats;
            for (const LibraryDb::Album &album: albums) {
                QString name;
                switch(albumSort) {
                case LibraryDb::AS_AlArYr:
                case LibraryDb::AS_AlYrAr:
                    name=album.sort.isEmpty() ? album.name.at(0).toUpper() : album.sort.at(0).toUpper();
                    break;
                default:
                case LibraryDb::AS_ArAlYr:
                case LibraryDb::AS_ArYrAl:
                    name=album.artist;
                    break;
                case LibraryDb::AS_YrAlAr:
                case LibraryDb::AS_YrArAl:
                    name=QString::number(album.year);
                    break;
                case LibraryDb::AS_Modified:
                    if (album.lastModified>=week) {
                        name = tr("Modified in the last week");
                    } else if (album.lastModified>=week2) {
                        name = tr("Modified in the last 2 weeks");
                    } else if (album.lastModified>=days30) {
                        name = tr("Modified in the last 30 days");
                    } else if (album.lastModified>=days60) {
                        name = tr("Modified in the last 60 days");
                    } else if (album.lastModified>=days90) {
                        name = tr("Modified in the last 90 days");
                    } else if (album.lastModified>=year) {
                        name = tr("Modified in the last year");
                    } else if (album.lastModified>=year2) {
                        name = tr("Modified in the last 2 years");
                    } else if (album.lastModified>=year3) {
                        name = tr("Modified in the last 3 years");
                    } else {
                        name = tr("Modified more than 3 years ago");
                    }
                    break;
                }

                int cat = -1;
                if (!name.isEmpty()) {
                    const auto existing = knownCats.find(name);
                    if (knownCats.constEnd()==existing) {
                        cat=categories.size();
                        categories.append(name);
                        knownCats.insert(name, cat);
                    } else {
                        cat=existing.value();
                    }
                }

                QString trackInfo = tr("%n Tracks (%1)", "", album.trackCount).arg(Utils::formatTime(album.duration, true));
                root->add(new AlbumItem(T_Album==tl && album.identifyById ? QString() : album.artist,
                                        album.id, Song::displayAlbum(album.name, album.year),
                                        T_Album==tl ? album.artist : trackInfo, T_Album==tl ? trackInfo : QString(), root, cat));
            }
        }
        break;
    }
    default:
        break;
    }

    endResetModel();
}

void SqlLibraryModel::search(const QString &str, const QString &genre)
{
    if (db->setFilter(str, genre)) {
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
    const Item * c = row<p->getChildCount() ? p->getChildren().at(row) : nullptr;
    return c ? createIndex(row, column, (void *)c) : QModelIndex();
}

QModelIndex SqlLibraryModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }

    const Item * const item = static_cast<Item *>(child.internalPointer());
    Item * const parentItem = item->getParent();
    if (parentItem == root || nullptr==parentItem) {
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
            for (const LibraryDb::Artist &artist: artists) {
                item->add(new CollectionItem(T_Artist, artist.name, artist.name, tr("%n Album(s)", "", artist.albumCount), item));
            }
            endInsertRows();
        }
        break;
    }
    case T_Artist: {
        QList<LibraryDb::Album> albums=db->getAlbums(item->getId(), T_Genre==tl ? item->getParent()->getId() : QString(), librarySort);
        if (!albums.isEmpty())  {
            beginInsertRows(index, 0, albums.count()-1);
            for (const LibraryDb::Album &album: albums) {
                item->add(new CollectionItem(T_Album, album.id, Song::displayAlbum(album.name, album.year),
                                             tr("%n Tracks (%1)", "", album.trackCount).arg(Utils::formatTime(album.duration, true)), item));
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
            for (const Song &song: songs) {
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
        if (Cantata::Role_CatergizedHasSubText==role) {
            return T_Album!=tl || LibraryDb::AS_ArAlYr==albumSort || LibraryDb::AS_ArYrAl==albumSort;
        }
        return QVariant();
    }

    Item *item = static_cast<Item *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (item->getType()) {
        case T_Genre:
            return Icons::self()->genreIcon;
        case T_Artist:
            return Icons::self()->artistIcon;
        case T_Album:
            return Icons::self()->albumIcon(32);
        case T_Track:
            return Song::Playlist==item->getSong().type ? Icons::self()->playlistListIcon : Icons::self()->audioListIcon;
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
                return track->getSong().isCueFile() ? tr("Cue Sheet") : tr("Playlist");
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
    case Cantata::Role_TitleSubText:
        if (T_Album==tl && T_Album==item->getType()) {
            return static_cast<AlbumItem *>(item)->getTitleSub();
        }
    case Cantata::Role_SubText:
        return item->getSubText();
    case Cantata::Role_ListImage:
        return T_Album==item->getType();
    case Cantata::Role_TitleText:
        if (T_Album==item->getType()) {
            return tr("%1 by %2").arg(item->getText()).arg(T_Album==tl ? item->getSubText() : item->getParent()->getText());
        }
        return item->getText();
    case Cantata::Role_TitleActions:
        switch (item->getType()) {
        case T_Artist:
        case T_Album:
            return true;
        default:
            return false;
        }
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
    for (const QModelIndex &idx: list) {
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
    for (const Song &s: songList) {
        files.append(s.file);
    }
    return files;
}

QModelIndex SqlLibraryModel::findSongIndex(const Song &song)
{
    if (root) {
        QModelIndex albumIndex=findAlbumIndex(song.albumArtistOrComposer(), song.albumId());
        if (albumIndex.isValid()) {
            if (canFetchMore(albumIndex)) {
                fetchMore(albumIndex);
            }
            CollectionItem *al=static_cast<CollectionItem *>(albumIndex.internalPointer());
            for (Item *t: al->getChildren()) {
                if (static_cast<TrackItem *>(t)->getSong().title==song.title) {
                    return index(t->getRow(), 0, albumIndex);
                }
            }
        }
    }

    // Hmm... Find song details in db via file path - fixes SingleTracks songs
    if (!song.file.isEmpty()) {
        QList<Song> dbSongs=db->songs(QStringList() << song.file);
        if (!dbSongs.isEmpty() && dbSongs.first().albumId()!=song.albumId()) {
            Song dbSong=dbSongs.first();
            dbSong.file=QString(); // Prevent recursion!
            return findSongIndex(dbSong);
        }
    }
    return QModelIndex();
}

QModelIndex SqlLibraryModel::findAlbumIndex(const QString &artist, const QString &album)
{
    if (root) {
        if (T_Album==tl) {
            for (Item *a: root->getChildren()) {
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
                for (Item *al: ar->getChildren()) {
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
            for (Item *g: root->getChildren()) {
                QModelIndex gIndex=index(g->getRow(), 0, QModelIndex());
                if (canFetchMore(gIndex)) {
                    fetchMore(gIndex);
                }
                for (Item *a: static_cast<CollectionItem *>(g)->getChildren()) {
                    if (a->getId()==artist) {
                        return index(a->getRow(), 0, gIndex);
                    }
                }
            }
        } else if (T_Artist==tl) {
            for (Item *a: root->getChildren()) {
                if (a->getId()==artist) {
                    return index(a->getRow(), 0, QModelIndex());
                }
            }
        }
    }
    return QModelIndex();
}

QSet<QString> SqlLibraryModel::getGenres() const
{
    return db->get("genre");
}

QSet<QString> SqlLibraryModel::getArtists() const
{
    return db->get("albumArtist");
}

QList<Song> SqlLibraryModel::getAlbumTracks(const QString &artistId, const QString &albumId) const
{
    return db->getTracks(artistId, albumId, QString(), LibraryDb::AS_ArAlYr, false);
}

QList<Song> SqlLibraryModel::songs(const QStringList &files, bool allowPlaylists) const
{
    return db->songs(files, allowPlaylists);
}

QList<LibraryDb::Album> SqlLibraryModel::getArtistOrComposerAlbums(const QString &artist) const
{
    return db->getAlbumsWithArtistOrComposer(artist);
}

void SqlLibraryModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres)
{
    db->getDetails(artists, albumArtists, composers, albums, genres);
}

bool SqlLibraryModel::songExists(const Song &song)
{
    return db->songExists(song);
}

int SqlLibraryModel::trackCount() const
{
    return db->trackCount();
}

void SqlLibraryModel::populate(const QModelIndexList &list) const
{
    for (const QModelIndex &idx: list) {
        if (canFetchMore(idx)) {
            const_cast<SqlLibraryModel *>(this)->fetchMore(idx);
        }
        if (T_Track!=static_cast<Item *>(idx.internalPointer())->getType()) {
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
        for (const QModelIndex &c: children(idx)) {
            list+=songs(c, allowPlaylists);
        }
    } else {
        const Item *i=static_cast<const Item *>(idx.internalPointer());
        if (i && T_Track==i->getType() && (allowPlaylists || (Song::Playlist!=i->getSong().type && !i->getSong().isFromCue()))) {
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
    return childMap.constEnd()==it ? nullptr : it.value();
}

#include "moc_sqllibrarymodel.cpp"
