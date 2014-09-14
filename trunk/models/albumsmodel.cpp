/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 * Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
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

#include <QModelIndex>
#include <QDataStream>
#include <QMimeData>
#include <QStringList>
#include <QPainter>
#include <QFile>
#include "support/localize.h"
#include "gui/plurals.h"
#include "support/globalstatic.h"
#include "albumsmodel.h"
#include "gui/settings.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "musiclibrarymodel.h"
#include "playqueuemodel.h"
#include "proxymodel.h"
#include "mpd/song.h"
#include "gui/covers.h"
#include "roles.h"
#include "mpd/mpdparseutils.h"
#include "widgets/icons.h"
#include "support/utils.h"
#include "config.h"
#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

static int sortAlbums=AlbumsModel::Sort_AlbumArtist;

GLOBAL_STATIC(AlbumsModel, instance)

AlbumsModel::Sort AlbumsModel::toSort(const QString &str)
{
    for (int i=0; i<=Sort_YearArtist; ++i) {
        if (sortStr((Sort)i)==str) {
            return (Sort)i;
        }
    }
    return AlbumsModel::Sort_AlbumArtist;
}

QString AlbumsModel::sortStr(Sort m)
{
    switch (m) {
    default:
    case Sort_AlbumArtist: return QLatin1String("album-artist");
    case Sort_AlbumYear:   return QLatin1String("album-year");
    case Sort_ArtistAlbum: return QLatin1String("artist-album");
    case Sort_ArtistYear:  return QLatin1String("artist-year");
    case Sort_YearAlbum:   return QLatin1String("year-album");
    case Sort_YearArtist:  return QLatin1String("year-artist");
    }
}


AlbumsModel::AlbumsModel(QObject *parent)
    : ActionModel(parent)
    , enabled(false)
//     , coversRequested(false)
{
    #if defined ENABLE_MODEL_TEST
    new ModelTest(this, this);
    #endif
}

AlbumsModel::~AlbumsModel()
{
}

QVariant AlbumsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int AlbumsModel::rowCount(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return items.size();
    }

    Item *item=static_cast<Item *>(index.internalPointer());
    if (item->isAlbum()) {
        AlbumItem *al=static_cast<AlbumItem *>(index.internalPointer());
        return al->songs.count();
    }
    return 0;
}

bool AlbumsModel::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid() || static_cast<Item *>(parent.internalPointer())->isAlbum();
}

QModelIndex AlbumsModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isAlbum()) {
        return QModelIndex();
    } else {
        SongItem *song=static_cast<SongItem *>(item);

        if (song->parent) {
            return createIndex(items.indexOf(song->parent), 0, song->parent);
        }
    }

    return QModelIndex();
}

QModelIndex AlbumsModel::index(int row, int col, const QModelIndex &parent) const
{
    if (!hasIndex(row, col, parent)) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        Item *p=static_cast<Item *>(parent.internalPointer());

        if (p->isAlbum()) {
            AlbumItem *pl=static_cast<AlbumItem *>(p);
            return row<pl->songs.count() ? createIndex(row, col, pl->songs.at(row)) : QModelIndex();
        }
    }

    return row<items.count() ? createIndex(row, col, items.at(row)) : QModelIndex();
}

#ifdef ENABLE_UBUNTU
static const QString constDefaultCover=QLatin1String("qrc:/album.svg");
#endif

QVariant AlbumsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isAlbum()) {
        AlbumItem *al=static_cast<AlbumItem *>(item);

        switch (role) {
        default:
            return ActionModel::data(index, role);
        #ifdef ENABLE_UBUNTU
        case Cantata::Role_Image: {
            QString cover=al->cover();
            return cover.isEmpty() ? constDefaultCover : cover;
        }
        #endif
        case Cantata::Role_ListImage:
            return true;
        #ifndef ENABLE_UBUNTU
        case Qt::DecorationRole:
            return Icons::self()->albumIcon;
        #endif
        case Qt::ToolTipRole:
            if (!Settings::self()->infoTooltips()) {
                return QVariant();
            }
            return 0==al->songs.count()
                    ? QString()
                    : (al->artist+QLatin1Char('\n')+al->albumDisplay()+QLatin1Char('\n')+
                        Plurals::tracksWithDuration(al->trackCount(), Utils::formatTime(al->totalTime(), true)));
        case Qt::DisplayRole:
            return al->album;
        case Qt::FontRole:
            if (al->isNew) {
                QFont f=ActionModel::data(index, role).value<QFont>();
                f.setBold(true);
                return f;
            }
            break;
        case Cantata::Role_MainText:
            return al->albumDisplay();
        case Cantata::Role_BriefMainText:
            return al->album;
        case Cantata::Role_SubText:
            return al->artist;
        case Cantata::Role_TitleText:
            return i18nc("Album by Artist", "%1 by %2", al->album, al->artist);
        case Cantata::Role_CoverSong: {
            QVariant v;
            v.setValue<Song>(al->coverSong());
            return v;
        }
        }
    } else {
        SongItem *si=static_cast<SongItem *>(item);

        switch (role) {
        default:
            return ActionModel::data(index, role);
        #ifdef ENABLE_UBUNTU
        case Cantata::Role_Image:
            return QString();
        #endif
        case Cantata::Role_ListImage:
            return false;
        #ifndef ENABLE_UBUNTU
        case Qt::DecorationRole:
            return Song::Playlist==si->type ? Icons::self()->playlistIcon : Icons::self()->audioFileIcon;
        #endif
        case Qt::ToolTipRole:
            if (!Settings::self()->infoTooltips()) {
                return QVariant();
            }
            return si->toolTip();
        case Cantata::Role_MainText:
        case Qt::DisplayRole:
            if (Song::Playlist==si->type) {
                return si->isCueFile() ? i18n("Cue Sheet") : i18n("Playlist");
            }
            if (Song::SingleTracks==si->parent->type) {
                return si->artistSong();
            }
            return si->trackAndTitleStr();
        case Cantata::Role_SubText:
            return Utils::formatTime(si->time, true);
        }
    }

    return QVariant();
}

Qt::ItemFlags AlbumsModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    }
    return Qt::NoItemFlags;
}

QStringList AlbumsModel::filenames(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<Song> songList=songs(indexes, allowPlaylists);
    QStringList fnames;
    foreach (const Song &s, songList) {
        fnames.append(s.file);
    }
    return fnames;
}

QList<Song> AlbumsModel::songs(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<Song> songs;
    QSet<QString> files;

    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isAlbum()) {
            QList<Song> albumSongs;
            const SongItem *cue=allowPlaylists ? static_cast<AlbumItem*>(item)->getCueFile() : 0;
            if (cue) {
                if (!files.contains(cue->file)) {
                    albumSongs << *cue;
                    files << cue->file;
                }
            } else {
                foreach (const SongItem *s, static_cast<AlbumItem*>(item)->songs) {
                    if ((/*allowPlaylists || */Song::Playlist!=s->type) && !files.contains(s->file)) {
                        albumSongs << *s;
                        files << s->file;
                    }
                }
            }
            qSort(albumSongs);
            songs << albumSongs;
        } else if ((allowPlaylists || Song::Playlist!=static_cast<SongItem*>(item)->type) && !files.contains(static_cast<SongItem*>(item)->file)) {
            songs << *static_cast<SongItem*>(item);
            files << static_cast<SongItem*>(item)->file;
        }
    }
    return songs;
}

#ifndef ENABLE_UBUNTU
QMimeData * AlbumsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files=filenames(indexes, true);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
//    if (!MPDConnection::self()->getDetails().dir.isEmpty()) {
//        QStringList paths;
//        foreach (const QString &f, files) {
//            paths << MPDConnection::self()->getDetails().dir+f;
//        }
//        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
//    }
    return mimeData;
}
#endif

void AlbumsModel::update(const MusicLibraryItemRoot *root)
{
    if (!enabled) {
        return;
    }

    bool changesMade=false;
    bool resettingModel=items.isEmpty() || 0==root->childCount();
    if (resettingModel) {
        beginResetModel();
    }
    QList<AlbumItem *>::Iterator it=items.begin();
    QList<AlbumItem *>::Iterator end=items.end();

    for (; it!=end; ++it) {
        (*it)->updated=false;
    }

    for (int i = 0; i < root->childCount(); i++) {
        MusicLibraryItemArtist *artistItem = static_cast<MusicLibraryItemArtist*>(root->childItem(i));
        const QString &artist=artistItem->data();
        for (int j = 0; j < artistItem->childCount(); j++) {
            MusicLibraryItemAlbum *albumItem = static_cast<MusicLibraryItemAlbum*>(artistItem->childItem(j));
            const QString &albumId=albumItem->albumId();
            bool found=false;
            it=items.begin();
            end=items.end();
            for (; it!=end; ++it) {
                if ((*it)->year==albumItem->year() && (*it)->artist==artist && (*it)->albumId()==albumId) {
                    if (!resettingModel) {
                        QModelIndex albumIndex=index(items.indexOf(*it), 0, QModelIndex());
                        bool hadSongs=!(*it)->songs.isEmpty();
                        if (hadSongs) {
                            beginRemoveRows(albumIndex, 0, (*it)->songs.count()-1);
                        }
                        (*it)->clearSongs();
                        if (hadSongs) {
                            endRemoveRows();
                        }
                        if (albumItem->childCount()) {
                            beginInsertRows(albumIndex, 0, albumItem->childCount()-1);
                        }
                    }
                    (*it)->setSongs(albumItem);
                    if (!resettingModel && albumItem->childCount()) {
                        endInsertRows();
                    }
                    (*it)->genres=albumItem->genres();
                    (*it)->updated=true;
                    found=true;
                    if ((*it)->isNew!=albumItem->isNew()) {
                        (*it)->isNew=albumItem->isNew();
                        if (!resettingModel) {
                            QModelIndex albumIndex=index(items.indexOf(*it), 0, QModelIndex());
                            emit dataChanged(albumIndex, albumIndex);
                        }
                    }
                    break;
                }
            }

            if (!found) {
                changesMade=true;
                AlbumItem *a=new AlbumItem(artist, albumItem->data(), albumId, albumItem->year());
                a->setSongs(albumItem);
                a->genres=albumItem->genres();
                a->updated=true;
                a->type=albumItem->songType();
                a->isNew=albumItem->isNew();
                if (!resettingModel) {
                    beginInsertRows(QModelIndex(), items.count(), items.count());
                }
                items.append(a);
                if (!resettingModel) {
                    endInsertRows();
                }
            }
        }
    }

    if (resettingModel) {
        endResetModel();
    } else {
        for (int i=0; i<items.count();) {
            AlbumItem *ai=items.at(i);
            if (!ai->updated) {
                beginRemoveRows(QModelIndex(), i, i);
                delete items.takeAt(i);
                endRemoveRows();
            } else {
                ++i;
            }
        }
    }

    if (changesMade) {
        emit updated();
    }
}

void AlbumsModel::setCover(const Song &song, const QImage &img, const QString &file)
{
    #ifdef ENABLE_UBUNTU
    if (img.isNull()) {
        if (!file.isEmpty()) return; //If empty, we need to execute the stuff below to set m_coverRequested to false
    }

    QList<AlbumItem *>::Iterator it=items.begin();
    QList<AlbumItem *>::Iterator end=items.end();
    QString artist=MusicLibraryItemRoot::artistName(song);
    const QString &albumId=song.albumId();

    for (int row=0; it!=end; ++it, ++row) {
        if ((*it)->artist==artist && (*it)->albumId()==albumId) {
            if ((*it)->coverRequested) {
                (*it)->coverRequested=false;
                if (!file.isEmpty()) {
                    (*it)->coverFile="file://"+file;
                    QModelIndex idx=index(row, 0, QModelIndex());
                    emit dataChanged(idx, idx);
                }
            }
            return;
        }
    }
    #else
    Q_UNUSED(song)
    Q_UNUSED(img)
    Q_UNUSED(file)
    #endif
}

void AlbumsModel::coverLoaded(const Song &song, int s)
{
    Q_UNUSED(s)
    #ifdef ENABLE_UBUNTU
    Q_UNUSED(song)
    #else
    if (!song.isArtistImageRequest()) {
        QList<AlbumItem *>::Iterator it=items.begin();
        QList<AlbumItem *>::Iterator end=items.end();
        const QString &albumArtist=song.albumArtist();
        const QString &albumId=song.albumId();

        for (int row=0; it!=end; ++it, ++row) {
            if ((*it)->artist==albumArtist && (*it)->albumId()==albumId) {
                QModelIndex idx=index(row, 0, QModelIndex());
                emit dataChanged(idx, idx);
            }
        }
    }
    #endif
}

void AlbumsModel::clearNewState()
{
    for (int i=0; i<items.count(); ++i) {
        AlbumItem *al=items.at(i);
        if (al->isNew) {
            al->isNew=false;
            QModelIndex idx=index(i, 0, QModelIndex());
            emit dataChanged(idx, idx);
        }
    }
}

void AlbumsModel::clear()
{
    beginResetModel();
    qDeleteAll(items);
    items.clear();
    endResetModel();

    #ifdef ENABLE_UBUNTU //For displaying a message when there is no album
    emit updated();
    #endif
}

void AlbumsModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;

    if (enabled) {
        #ifdef ENABLE_UBUNTU
        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
        #else
        connect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
        #endif
        update(MusicLibraryModel::self()->root());
    } else {
        clear();
        #ifdef ENABLE_UBUNTU
        disconnect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                   this, SLOT(setCover(const Song &, const QImage &, const QString &)));
        #else
        disconnect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
        #endif
    }
}

int AlbumsModel::albumSort() const
{
    return sortAlbums;
}

void AlbumsModel::setAlbumSort(int s)
{
    if (s!=sortAlbums) {
        beginResetModel();
        sortAlbums=s;
        endResetModel();
    }
}

AlbumsModel::AlbumItem::AlbumItem(const QString &ar, const QString &al, const QString &i, quint16 y)
    : artist(ar)
    , album(al)
    , id(i)
    , year(y)
    , updated(false)
    , numTracks(0)
    , time(0)
    , isNew(false)
    #ifdef ENABLE_UBUNTU
    , coverRequested(false)
    #endif
{
    if (artist.startsWith(QLatin1String("The "))) {
        nonTheArtist=artist.mid(4);
    }
}

AlbumsModel::AlbumItem::~AlbumItem()
{
    clearSongs();
}

bool AlbumsModel::AlbumItem::operator<(const AlbumItem &o) const
{
    switch (sortAlbums) {
    default:
    case Sort_AlbumArtist:  {
        int compare=album.localeAwareCompare(o.album);
        if (0!=compare) {
            return compare<0;
        }
        compare=sortArtist().localeAwareCompare(o.sortArtist());
        if (0!=compare) {
            return compare<0;
        }
        return year==o.year ? id.compare(o.id)<0 : year<o.year;
    }
    case Sort_AlbumYear: {
        int compare=album.localeAwareCompare(o.album);
        if (0!=compare) {
            return compare<0;
        }
        if (year!=o.year) {
            return year<o.year;
        }
        compare=sortArtist().localeAwareCompare(o.sortArtist())<0;
        return 0==compare ? id.compare(o.id)<0 : compare<0;
    }
    case Sort_ArtistAlbum: {
        int compare=sortArtist().localeAwareCompare(o.sortArtist());
        if (0!=compare) {
            return compare<0;
        }
        compare=album.localeAwareCompare(o.album);
        if (0!=compare) {
            return compare<0;
        }
        return year==o.year ? id.compare(o.id)<0 : year<o.year;
    }
    case Sort_ArtistYear: {
        int compare=sortArtist().localeAwareCompare(o.sortArtist());
        if (0!=compare) {
            return compare<0;
        }
        if (year!=o.year) {
            return year<o.year;
        }
        compare=album.localeAwareCompare(o.album);
        return 0==compare ? id.compare(o.id)<0 : compare<0;
    }
    case Sort_YearAlbum:
        if (year==o.year) {
            int compare=album.localeAwareCompare(o.album);
            if (0!=compare) {
                return compare<0;
            }
            compare=sortArtist().localeAwareCompare(o.sortArtist());
            return 0==compare ? id.compare(o.id)<0 : compare<0;
        }
        return year<o.year;
    case Sort_YearArtist:
        if (year==o.year) {
            int compare=sortArtist().localeAwareCompare(o.sortArtist());
            if (0!=compare) {
                return compare<0;
            }
            compare=album.localeAwareCompare(o.album);
            return 0==compare ? id.compare(o.id)<0 : compare<0;
        }
        return year<o.year;
    }
}

void AlbumsModel::AlbumItem::clearSongs()
{
    time=0;
    qDeleteAll(songs);
    songs.clear();
}

void AlbumsModel::AlbumItem::setSongs(MusicLibraryItemAlbum *ai)
{
    foreach (MusicLibraryItem *item, ai->childItems()) {
        songs.append(new SongItem(static_cast<MusicLibraryItemSong*>(item)->song(), this));
    }
}

quint32 AlbumsModel::AlbumItem::trackCount()
{
    updateStats();
    return numTracks;
}

quint32 AlbumsModel::AlbumItem::totalTime()
{
    updateStats();
    return time;
}

void AlbumsModel::AlbumItem::updateStats()
{
    if (0==time) {
        numTracks=0;
        foreach (SongItem *s, songs) {
            if (Song::Playlist!=s->type) {
                time+=s->time;
                numTracks++;
            }
        }
    }
}

#ifdef ENABLE_UBUNTU
QString AlbumsModel::AlbumItem::cover()
{
    if (Song::SingleTracks!=type && songs.count() && coverFile.isEmpty() && !coverRequested) {
        coverRequested=true;
        coverFile=Covers::self()->requestImage(coverSong()).fileName;
        if (!coverFile.isEmpty()) {
            coverRequested=false;
            coverFile="file://"+coverFile;
        }
    }

    return coverFile;
}
#endif

const Song & AlbumsModel::AlbumItem::coverSong()
{
    if (cSong.isEmpty() && songs.count()) {
        SongItem *firstSong=songs.first();
        cSong.artist=firstSong->artist;
        cSong.albumartist=Song::useComposer() && !firstSong->composer().isEmpty()
                ? firstSong->albumArtist() : artist;
        cSong.album=album;
        cSong.year=year;
        cSong.file=firstSong->file;
        cSong.type=type;
        cSong.setComposer(firstSong->composer());
        cSong.setMbAlbumId(firstSong->mbAlbumId());
    }
    return cSong;
}

const AlbumsModel::SongItem *AlbumsModel::AlbumItem::getCueFile() const
{
    foreach (SongItem *s, songs) {
        if (s->isCueFile()) {
            return s;
        }
    }

    return 0;
}
