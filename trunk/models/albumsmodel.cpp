/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "localize.h"
#include "plurals.h"
#include "globalstatic.h"
#include "albumsmodel.h"
#include "settings.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "musiclibrarymodel.h"
#include "playqueuemodel.h"
#include "proxymodel.h"
#include "song.h"
#include "covers.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "icons.h"
#include "utils.h"
#include "config.h"
#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

static int sortAlbums=AlbumsModel::Sort_AlbumArtist;

GLOBAL_STATIC(AlbumsModel, instance)

static MusicLibraryItemAlbum::CoverSize coverSize=MusicLibraryItemAlbum::CoverMedium;
static QPixmap *theDefaultIcon=0;
static QSize itemSize;
static bool iconMode=true;

int AlbumsModel::iconSize()
{
    return MusicLibraryItemAlbum::iconSize(coverSize, iconMode);
}

static int stdIconSize()
{
    return iconMode ? 128 : MusicLibraryItemAlbum::iconSize(coverSize);
}

void AlbumsModel::setIconMode(bool u)
{
    if (iconMode!=u && theDefaultIcon) {
        delete theDefaultIcon;
        theDefaultIcon=0;
    }
    iconMode=u;
}

MusicLibraryItemAlbum::CoverSize AlbumsModel::currentCoverSize()
{
    return coverSize;
}

void AlbumsModel::setItemSize(const QSize &sz)
{
    itemSize=sz;
}

void AlbumsModel::setCoverSize(MusicLibraryItemAlbum::CoverSize size)
{
    if (size!=coverSize) {
        if (theDefaultIcon) {
            delete theDefaultIcon;
            theDefaultIcon=0;
        }
        coverSize=size;
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
        case ItemView::Role_Image:
        case Qt::DecorationRole: {
            int iSize=iconSize();
            if (iSize) {
                QPixmap *pix=al->cover();
                if (pix) {
                    return *pix;
                }
            } else if (Qt::DecorationRole==role) {
                return Icons::self()->albumIcon;
            }

            if (!theDefaultIcon) {
                int cSize=iSize;
                int stdSize=stdIconSize();
                if (0==cSize) {
                    cSize=stdSize=22;
                }
                theDefaultIcon = new QPixmap(Icons::self()->albumIcon.pixmap(stdSize, stdSize)
                                            .scaled(QSize(cSize, cSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            }
            return *theDefaultIcon;
        }
        case Qt::ToolTipRole:
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
        case ItemView::Role_MainText:
            if (Sort_ArtistYearAlbum==sortAlbums) {
                return al->albumDisplay();
            }
        case ItemView::Role_BriefMainText:
            return al->album;
        case ItemView::Role_ImageSize:
            return iconSize();
        case ItemView::Role_SubText:
            return al->artist;
        case ItemView::Role_TitleText:
            return i18nc("Album by Artist", "%1 by %2", al->album, al->artist);
        case Qt::SizeHintRole:
            if (!itemSize.isNull()) {
                return itemSize;
            }
        }
    } else {
        SongItem *si=static_cast<SongItem *>(item);

        switch (role) {
        default:
            return ActionModel::data(index, role);
        case Qt::DecorationRole:
            return Song::Playlist==si->type ? Icons::self()->playlistIcon : Icons::self()->audioFileIcon;
        case Qt::ToolTipRole: {
            quint32 year=si->parent->songs.count() ? si->parent->songs.at(0)->year : 0;
            return si->parent->artist+QLatin1String("<br/>")+
                   si->parent->album+(year>0 ? (QLatin1String(" (")+QString::number(year)+QChar(')')) : QString())+QLatin1String("<br/>")+
                   data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+
                   Utils::formatTime(si->time, true)+QLatin1String("<br/>")+
                   QLatin1String("<small><i>")+si->filePath()+QLatin1String("</i></small>");
        }
        case Qt::DisplayRole:
            if (Song::Playlist==si->type) {
                return si->isCueFile() ? i18n("Cue Sheet") : i18n("Playlist");
            }
            if (Song::SingleTracks==si->parent->type) {
                return si->artistSong();
            }
            else {
                return si->trackAndTitleStr(Song::isVariousArtists(si->parent->artist) && !Song::isVariousArtists(si->artist));
            }
        case ItemView::Role_SubText:
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

QMimeData * AlbumsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files=filenames(indexes, true);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
    if (!MPDConnection::self()->getDetails().dir.isEmpty()) {
        QStringList paths;
        foreach (const QString &f, files) {
            paths << MPDConnection::self()->getDetails().dir+f;
        }
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}

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
            const QString &album=albumItem->data();
            bool found=false;
            it=items.begin();
            end=items.end();
            for (; it!=end; ++it) {
                if ((*it)->year==albumItem->year() && (*it)->artist==artist && (*it)->album==album) {
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
                AlbumItem *a=new AlbumItem(artist, album, albumItem->year());
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

void AlbumsModel::coverLoaded(const Song &song, int s)
{
    if (s==iconSize() && !song.isArtistImageRequest()) {
        QList<AlbumItem *>::Iterator it=items.begin();
        QList<AlbumItem *>::Iterator end=items.end();
        QString albumArtist=song.albumArtist();
        QString album=song.album;

        for (int row=0; it!=end; ++it, ++row) {
            if ((*it)->artist==albumArtist && (*it)->album==album) {
                QModelIndex idx=index(row, 0, QModelIndex());
                emit dataChanged(idx, idx);
            }
        }
    }
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
}

void AlbumsModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;

    if (enabled) {
        connect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
        update(MusicLibraryModel::self()->root());
    } else {
        clear();
        disconnect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
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

AlbumsModel::AlbumItem::AlbumItem(const QString &ar, const QString &al, quint16 y)
    : artist(ar)
    , album(al)
    , year(y)
    , updated(false)
    , numTracks(0)
    , time(0)
    , isNew(false)
{
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
        return compare<0 || (0==compare && artist.localeAwareCompare(o.artist)<0);
    }
    case Sort_ArtistAlbum: {
        int compare=artist.localeAwareCompare(o.artist);
        return compare<0 || (0==compare && album.localeAwareCompare(o.album)<0);
    }
    case Sort_ArtistYearAlbum: {
        int compare=artist.localeAwareCompare(o.artist);
        return compare<0 || (0==compare && (year<o.year || (year==o.year && album.localeAwareCompare(o.album)<0)));
    }
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

QPixmap * AlbumsModel::AlbumItem::cover()
{
    if (Song::SingleTracks!=type && songs.count()) {
        if (coverSong.isEmpty()) {
            SongItem *firstSong=songs.first();
            coverSong.artist=firstSong->artist;
            coverSong.albumartist=Song::useComposer() && !firstSong->composer.isEmpty()
                    ? firstSong->albumArtist() : artist;
            coverSong.album=album;
            coverSong.year=year;
            coverSong.file=firstSong->file;
            coverSong.type=type;
            coverSong.composer=firstSong->composer;
        }
        return Covers::self()->get(coverSong, iconSize());
    }

    return 0;
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
