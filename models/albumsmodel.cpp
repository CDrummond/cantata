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

#include <QtCore/QModelIndex>
#include <QtCore/QDataStream>
#include <QtCore/QMimeData>
#include <QtCore/QStringList>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#include <KDE/KGlobal>
#endif
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
#include "debugtimer.h"

static int sortAlbums=AlbumsModel::Sort_AlbumArtist;

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(AlbumsModel, instance)
#endif

AlbumsModel * AlbumsModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static AlbumsModel *instance=0;;
    if(!instance) {
        instance=new AlbumsModel;
    }
    return instance;
    #endif
}

static MusicLibraryItemAlbum::CoverSize coverSize=MusicLibraryItemAlbum::CoverMedium;
static QPixmap *theDefaultIcon=0;
static QSize itemSize;
static bool useLibraryCoverSizes=false;

int AlbumsModel::iconSize()
{
    if (useLibraryCoverSizes) {
        return MusicLibraryItemAlbum::iconSize(coverSize);
    }
    switch (coverSize) {
    default:
    case MusicLibraryItemAlbum::CoverNone:       return 0;
    case MusicLibraryItemAlbum::CoverSmall:      return 76;
    case MusicLibraryItemAlbum::CoverMedium:     return 100;
    case MusicLibraryItemAlbum::CoverLarge:      return 128;
    case MusicLibraryItemAlbum::CoverExtraLarge: return 160;
    }
}

static int stdIconSize()
{
    if (useLibraryCoverSizes) {
        return MusicLibraryItemAlbum::iconSize(coverSize);
    }
    return 128;
}

void AlbumsModel::setUseLibrarySizes(bool u)
{
    if (useLibraryCoverSizes!=u && theDefaultIcon) {
        delete theDefaultIcon;
        theDefaultIcon=0;
    }
    useLibraryCoverSizes=u;
}

bool AlbumsModel::useLibrarySizes()
{
    return useLibraryCoverSizes;
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
    : QAbstractItemModel(parent)
    , enabled(false)
//     , coversRequested(false)
{
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

    if(item->isAlbum())
        return QModelIndex();
    else
    {
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
        case ItemView::Role_Image:
        case Qt::DecorationRole: {
            if (al->cover) {
                return *(al->cover);
            }
            int iSize=iconSize();

            if (Qt::DecorationRole==role && 0==iSize) {
                return QIcon::fromTheme(DEFAULT_ALBUM_ICON);
            }

            if (!theDefaultIcon) {
                int cSize=iSize;
                int stdSize=stdIconSize();
                if (0==cSize) {
                    cSize=stdSize=22;
                }
                theDefaultIcon = new QPixmap(QIcon::fromTheme(DEFAULT_ALBUM_ICON).pixmap(stdSize, stdSize)
                                            .scaled(QSize(cSize, cSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            if (!al->coverRequested && iSize) {
                if (!al->isSingleTracks) {
                    al->getCover(true);
                    al->coverRequested=true;
                }
            }
            return *theDefaultIcon;
        }
        case Qt::ToolTipRole: {
            quint32 year=al->songs.count() ? al->songs.at(0)->year : 0;
            return 0==al->songs.count()
                ? al->name
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    year>0
                        ? i18np("%1\n%2 (%3)\n1 Track (%5)", "%1\n%2 (%3)\n%4 Tracks (%5)", al->artist, al->album,
                                QString::number(year), al->songs.count(), Song::formattedTime(al->totalTime()))
                        : i18np("%1\n%2\n1 Track (%4)", "%1\n%2\n Tracks (%4)", al->artist, al->album,
                                al->songs.count(), Song::formattedTime(al->totalTime()));
                    #else
                    (al->songs.count()>1
                        ? year>0
                            ? tr("%1\n%2 (%3)\n%4 Tracks (%5)").arg(al->artist).arg(al->album).arg(QString::number(year))
                                 .arg(al->songs.count()).arg(Song::formattedTime(al->totalTime()))
                            : tr("%1\n%2\n%3 Tracks (%4)").arg(al->artist).arg(al->album)
                                 .arg(al->songs.count()).arg(Song::formattedTime(al->totalTime()))
                        : year>0
                            ? tr("%1\n%2 (%3)\n1 Track (%4)").arg(al->artist).arg(al->album).arg(QString::number(year))
                                .arg(Song::formattedTime(al->totalTime()))
                            : tr("%1\n%2\n1 Track (%3)").arg(al->artist).arg(al->album)
                                .arg(Song::formattedTime(al->totalTime())));
                    #endif
        }
        case ItemView::Role_Search:
            return al->album;
        case Qt::DisplayRole:
            return al->name;
        case ItemView::Role_MainText:
            return Sort_AlbumArtist==sortAlbums ? al->album : al->artist;
        case ItemView::Role_ImageSize: {
            return iconSize();
        }
        case ItemView::Role_SubText:
            return Sort_AlbumArtist==sortAlbums ? al->artist : al->album;
        case Qt::SizeHintRole:
            if (!itemSize.isNull()) {
                return itemSize;
            }
        }
    } else {
        SongItem *si=static_cast<SongItem *>(item);

        switch (role) {
        case Qt::DecorationRole:
            return QIcon::fromTheme("audio-x-generic");
        case Qt::ToolTipRole: {
            quint32 year=si->parent->songs.count() ? si->parent->songs.at(0)->year : 0;
            return si->parent->artist+QLatin1String("<br/>")+
                   si->parent->album+(year>0 ? (QLatin1String(" (")+QString::number(year)+QChar(')')) : QString())+QLatin1String("<br/>")+
                   data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+
                   Song::formattedTime(si->time)+QLatin1String("<br/><small><i>")+si->file+QLatin1String("</i></small>");
        }
        case Qt::DisplayRole:
            if (si->parent->isSingleTracks) {
                return si->artistSong();
            }
            else {
                return si->trackAndTitleStr(Song::isVariousArtists(si->parent->artist) && !Song::isVariousArtists(si->artist));
            }
        case ItemView::Role_SubText: {
            return Song::formattedTime(si->time);
        }
        }
    }

    return QVariant();
}

Qt::ItemFlags AlbumsModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    } else {
        return Qt::NoItemFlags;
    }
}

QStringList AlbumsModel::filenames(const QModelIndexList &indexes) const
{
    QList<Song> songList=songs(indexes);
    QStringList fnames;
    foreach (const Song &s, songList) {
        fnames.append(s.file);
    }
    return fnames;
}

QList<Song> AlbumsModel::songs(const QModelIndexList &indexes) const
{
    QList<Song> songs;
    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isAlbum()) {
            foreach (const SongItem *s, static_cast<AlbumItem*>(item)->songs) {
                if (!songs.contains(*s)) {
                    songs << *s;
                }
            }
        } else if (!songs.contains(*static_cast<SongItem*>(item))) {
            songs << *static_cast<SongItem*>(item);
        }
    }
    qSort(songs);
    return songs;
}

QMimeData * AlbumsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files=filenames(indexes);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
    if (!Settings::self()->mpdDir().isEmpty()) {
        QStringList paths;
        foreach (const QString &f, files) {
            paths << Settings::self()->mpdDir()+f;
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

    TF_DEBUG
    bool resettingModel=items.isEmpty() || 0==root->childCount();
    if (resettingModel) {
        beginResetModel();
    }
    QList<AlbumItem *>::Iterator it=items.begin();
    QList<AlbumItem *>::Iterator end=items.end();

    for (; it!=end; ++it) {
        (*it)->updated=false;
        //(*it)->coverRequested=false;
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
                    break;
                }
            }

            if (!found) {
                AlbumItem *a=new AlbumItem(artist, album, albumItem->year());
                a->setSongs(albumItem);
                a->genres=albumItem->genres();
                a->updated=true;
                a->isSingleTracks=albumItem->isSingleTracks();
                if (!resettingModel) {
                    beginInsertRows(QModelIndex(), items.count(), items.count());
                }
                items.append(a);
                if (!resettingModel) {
                    endInsertRows();
                }
//                 if (!a->isSingleTracks ) {
//                     a->getCover(false);
//                 }
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
}

// void AlbumsModel::getCovers()
// {
//     if (!enabled || coversRequested || items.isEmpty()) {
//         return;
//     }
//
//     coversRequested=true;
//     foreach (AlbumItem *a, items) {
//         if (!a->isSingleTracks && !a->coverRequested) {
//             a->getCover(false);
//         }
//     }
// }

void AlbumsModel::setCover(const Song &song, const QImage &img, const QString &file)
{
    Q_UNUSED(file)
    if (img.isNull()) {
        return;
    }

    QList<AlbumItem *>::Iterator it=items.begin();
    QList<AlbumItem *>::Iterator end=items.end();

    for (int row=0; it!=end; ++it, ++row) {
        if ((*it)->year==song.year && (*it)->artist==song.albumArtist() && (*it)->album==song.album) {
            (*it)->cover=new QPixmap(QPixmap::fromImage(img.scaled(QSize(iconSize(), iconSize()), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            QModelIndex idx=index(row, 0, QModelIndex());
            emit dataChanged(idx, idx);
            return;
        }
    }
}

void AlbumsModel::clear()
{
    beginResetModel();
    qDeleteAll(items);
    items.clear();
    endResetModel();
//     coversRequested=false;
}

void AlbumsModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;

    if (enabled) {
        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
//         connect(MusicLibraryModel::self(), SIGNAL(updated(const MusicLibraryItemRoot *)), AlbumsModel::self(), SLOT(update(const MusicLibraryItemRoot *)));
        update(MusicLibraryModel::self()->root());
    } else {
        clear();
        disconnect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                    this, SLOT(setCover(const Song&, const QImage &, const QString &)));
//         disconnect(MusicLibraryModel::self(), SIGNAL(updated(const MusicLibraryItemRoot *)), AlbumsModel::self(), SLOT(update(const MusicLibraryItemRoot *)));
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
        foreach (AlbumItem *a, items) {
            a->setName();
        }
        endResetModel();
    }
}

AlbumsModel::AlbumItem::AlbumItem(const QString &ar, const QString &al, quint16 y)
    : artist(ar)
    , album(al)
    , year(y)
    , cover(0)
    , updated(false)
    , coverRequested(false)
    , time(0)
{
    setName();
}

AlbumsModel::AlbumItem::~AlbumItem()
{
    clearSongs();
    delete cover;
}

bool AlbumsModel::AlbumItem::operator<(const AlbumItem &o) const
{
    if (AlbumsModel::Sort_ArtistAlbumYear==sortAlbums) {
        int compare=ProxyModel::compareStrings(artist, o.artist);
        if (0==compare) {
            return year<o.year || (year==o.year && ProxyModel::compareStrings(album, o.album)<0);
        } else {
            return compare<0;
        }
    } else {
        return ProxyModel::compareStrings(name, o.name)<0;
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

void AlbumsModel::AlbumItem::setName()
{
    name=AlbumsModel::Sort_AlbumArtist==sortAlbums
            ? (album+QLatin1String(" - ")+artist)
            : (artist+QLatin1String(" - ")+album);
}

quint32 AlbumsModel::AlbumItem::totalTime()
{
    if (0==time) {
        foreach (SongItem *s, songs) {
            time+=s->time;
        }
    }
    return time;
}

void AlbumsModel::AlbumItem::getCover(bool urgent)
{
    if (!isSingleTracks && songs.count()) {
        Song s;
        s.artist=artist;
        s.album=album;
        s.year=year;
        s.file=songs.first()->file;
        Covers::self()->requestCover(s, urgent);
    }
}
