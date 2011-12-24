/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#endif
#include "albumsmodel.h"
#include "settings.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "playqueuemodel.h"
#include "song.h"
#include "covers.h"
#include "itemview.h"
#include "mpdparseutils.h"

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
    case MusicLibraryItemAlbum::CoverNone:   return 0;
    case MusicLibraryItemAlbum::CoverSmall:  return 76;
    case MusicLibraryItemAlbum::CoverMedium: return 100;
    case MusicLibraryItemAlbum::CoverLarge:  return 128;
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

AlbumsModel::AlbumItem::AlbumItem(const QString &ar, const QString &al)
    : artist(ar)
    , album(al)
    , cover(0)
    , updated(false)
    , coverRequested(false)
{
    name=album+QLatin1String(" - ")+artist;
}

AlbumsModel::AlbumsModel(QObject *parent)
    : QAbstractItemModel(parent)
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
                return QIcon::fromTheme("media-optical-audio");
            }

            if (!theDefaultIcon) {
                int cSize=iSize;
                int stdSize=stdIconSize();
                if (0==cSize) {
                    cSize=stdSize=22;
                }
                theDefaultIcon = new QPixmap(QIcon::fromTheme("media-optical-audio").pixmap(stdSize, stdSize)
                                            .scaled(QSize(cSize, cSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            if (!al->coverRequested && iSize) {
                Song s;
                s.artist=al->artist;
                s.album=al->album;
                if (al->songs.count()) {
                    s.file=al->songs.first()->file;
                }
                Covers::self()->get(s);
                al->coverRequested=true;
            }
            return *theDefaultIcon;
        }
        case Qt::ToolTipRole:
            return 0==al->songs.count()
                ? al->name
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Track", "%1\n%2 Tracks", al->name, al->songs.count());
                    #else
                    (al->songs.count()>1
                        ? tr("%1\n%2 Tracks").arg(al->name).arg(al->songs.count())
                        : tr("%1\n1 Track").arg(al->name));
                    #endif
        case Qt::DisplayRole:
            return al->album;
        case ItemView::Role_ImageSize: {
            return iconSize();
        }
        case ItemView::Role_SubText:
            return al->artist;
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
            QString duration=MPDParseUtils::formatDuration(si->time);
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(3);
            }
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(1);
            }
            return data(index, Qt::DisplayRole).toString()+QChar('\n')+duration;
        }
        case Qt::DisplayRole:
            if (si->track>9) {
                return QString::number(si->track)+QChar(' ')+si->title;
            } else if (si->track>0) {
                return QChar('0')+QString::number(si->track)+QChar(' ')+si->title;
            }
        case ItemView::Role_SubText: {
            QString text=MPDParseUtils::formatDuration(si->time);
            if (text.startsWith(QLatin1String("00:"))) {
                return text.mid(3);
            }
            if (text.startsWith(QLatin1String("00:"))) {
                return text.mid(1);
            }
            return text.mid(1);
        }
    }
    }

    return QVariant();
}

Qt::ItemFlags AlbumsModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::NoItemFlags;
}

QMimeData * AlbumsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList filenames;
    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isAlbum()) {
            foreach (const SongItem *s, static_cast<AlbumItem*>(item)->songs) {
                filenames << s->file;
            }
        } else {
            filenames << static_cast<SongItem*>(item)->file;
        }
    }

    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames);
    return mimeData;
}

void AlbumsModel::update(const MusicLibraryItemRoot *root)
{
    QList<AlbumItem *>::Iterator it=items.begin();
    QList<AlbumItem *>::Iterator end=items.end();

    for (; it!=end; ++it) {
        (*it)->updated=false;
        (*it)->coverRequested=false;
    }

    bool changed=false;
    for (int i = 0; i < root->childCount(); i++) {
        MusicLibraryItemArtist *artistItem = static_cast<MusicLibraryItemArtist*>(root->child(i));
        QString artist=artistItem->data();
        for (int j = 0; j < artistItem->childCount(); j++) {
            MusicLibraryItemAlbum *albumItem = static_cast<MusicLibraryItemAlbum*>(artistItem->child(j));
            QString album=albumItem->data();
            bool found=false;
            it=items.begin();
            end=items.end();
            for (; it!=end; ++it) {
                if ((*it)->artist==artist && (*it)->album==album) {
                    (*it)->setSongs(albumItem);
                    (*it)->genres=albumItem->genres();
                    (*it)->updated=true;
                    found=true;
                    break;
                }
            }

            if (!found) {
                AlbumItem *a=new AlbumItem(artist, album);
                a->setSongs(albumItem);
                a->genres=albumItem->genres();
                a->updated=true;
                items.append(a);
                changed=true;
            }
        }
    }

    for (it=items.begin(); it!=items.end();) {
        if (!(*it)->updated) {
            changed=true;
            QList<AlbumItem *>::Iterator cur=it;
            delete (*it);
            ++it;
            items.erase(cur);
        } else {
            ++it;
        }
    }

    if (changed) {
        beginResetModel();
        endResetModel();
        emit updated();
    }
}

void AlbumsModel::setCover(const QString &artist, const QString &album, const QImage &img)
{
    if (img.isNull()) {
        return;
    }

    QList<AlbumItem *>::Iterator it=items.begin();
    QList<AlbumItem *>::Iterator end=items.end();

    for (int row=0; it!=end; ++it, ++row) {
        if ((*it)->artist==artist && (*it)->album==album) {
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
}

AlbumsModel::AlbumItem::~AlbumItem()
{
    qDeleteAll(songs);
    songs.clear();
    delete cover;
}

void AlbumsModel::AlbumItem::setSongs(MusicLibraryItemAlbum *ai)
{
    for (int j = 0; j < ai->childCount(); j++) {
        MusicLibraryItemSong *songItem = static_cast<MusicLibraryItemSong*>(ai->child(j));
        songs.append(new SongItem(songItem->song(), this));
    }
}

