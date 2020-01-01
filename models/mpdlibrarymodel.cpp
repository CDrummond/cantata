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

#include "mpdlibrarymodel.h"
#include "support/globalstatic.h"
#include "support/configuration.h"
#include "db/mpdlibrarydb.h"
#include "gui/settings.h"
#include "gui/covers.h"
#include "roles.h"
#include "kcategorizedview/kcategorizedsortfilterproxymodel.h"
#include <QTimer>
#include <QDebug>

GLOBAL_STATIC(MpdLibraryModel, instance)
#define COVERS_DBUG if (Covers::verboseDebugEnabled()) qWarning() << metaObject()->className() << QThread::currentThread()->objectName() << __FUNCTION__

MpdLibraryModel::MpdLibraryModel()
    : SqlLibraryModel(new MpdLibraryDb(nullptr), nullptr)
    , showArtistImages(false)
{
    connect(Covers::self(), SIGNAL(cover(Song,QImage,QString)), this, SLOT(cover(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(coverUpdated(Song,QImage,QString)), this, SLOT(coverUpdated(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(artistImage(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(composerImage(Song,QImage,QString)), this, SLOT(artistImage(Song,QImage,QString)));
    if (MPDConnection::self()->isConnected()) {
        static_cast<MpdLibraryDb *>(db)->connectionChanged(MPDConnection::self()->getDetails());
    }
}

QVariant MpdLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    switch (role) {
    case Cantata::Role_ListImage: {
        Item *item = static_cast<Item *>(index.internalPointer());
        return T_Album==item->getType() || (T_Artist==item->getType() && useArtistImages());
    }
    case Cantata::Role_GridCoverSong:
    case Cantata::Role_CoverSong: {
        QVariant v;
        Item *item = static_cast<Item *>(index.internalPointer());
        COVERS_DBUG << index.data().toString() << item->getType() << role;
        switch (item->getType()) {
        case T_Album:
            if (item->getSong().isEmpty()) {
                Song song=static_cast<MpdLibraryDb *>(db)->getCoverSong(T_Album==topLevel() ? static_cast<AlbumItem *>(item)->getArtistId() : item->getParent()->getId(), item->getId());
                if (song.isEmpty()) {
                    COVERS_DBUG << "Failed to locate album cover song";
                    return QVariant();
                }
                item->setSong(song);
                COVERS_DBUG << "Set album cover song" << song.albumArtist() << song.album << song.file;
                if (T_Genre==topLevel()) {
                    song.addGenre(item->getParent()->getParent()->getId());
                }
            }
            v.setValue<Song>(item->getSong());
            break;
        case T_Artist:
            if (!showArtistImages && Cantata::Role_CoverSong==role) {
                COVERS_DBUG << "Not showing artist images";
                return QVariant();
            }
            if (item->getSong().isEmpty()) {
                Song song=static_cast<MpdLibraryDb *>(db)->getCoverSong(item->getId());
                if (song.isEmpty()) {
                    COVERS_DBUG << "Failed to locate artist cover song";
                    return QVariant();
                }
                COVERS_DBUG << "Set artist cover song" << song.albumArtist() << song.album << song.file;
                if (song.useComposer()) {
                    song.setComposerImageRequest();
                } else {
                    song.setArtistImageRequest();
                }
                if (T_Genre==topLevel()) {
                    song.addGenre(item->getParent()->getId());
                }
                item->setSong(song);
            }
            v.setValue<Song>(item->getSong());
            break;
        default:
            break;
        }
        return v;
    }
    case KCategorizedSortFilterProxyModel::CategoryDisplayRole: {
        Item *item = static_cast<Item *>(index.internalPointer());
        if (T_Album==item->getType()) {
            int cat = static_cast<AlbumItem *>(item)->getCategory();
            return cat>=0 && cat<categories.length() ? categories.at(cat) : "";
        }
        break;
    }
    case KCategorizedSortFilterProxyModel::CategorySortRole: {
        Item *item = static_cast<Item *>(index.internalPointer());
        if (T_Album==item->getType()) {
            return (long long)static_cast<AlbumItem *>(item)->getCategory();
        }
        break;
    }
    }
    return SqlLibraryModel::data(index, role);
}

void MpdLibraryModel::setUseArtistImages(bool u)
{
    if (u!=showArtistImages) {
        showArtistImages=u;
        switch(topLevel()) {
        case T_Genre: {
            for (const Item *g: root->getChildren()) {
                const CollectionItem *genre=static_cast<const CollectionItem *>(g);
                if (genre->getChildCount()) {
                    QModelIndex idx=index(genre->getRow(), 0, QModelIndex());
                    emit dataChanged(index(0, 0, idx), index(genre->getChildCount()-1, 0, idx));
                }
            }
            break;
        }
        case T_Artist:
            if (root->getChildCount()) {
                emit dataChanged(index(0, 0, QModelIndex()), index(root->getChildCount()-1, 0, QModelIndex()));
            }
            break;
        default:
            break;
        }
    }
}

static QLatin1String constUseArtistImagesKey("artistImages");

void MpdLibraryModel::load(Configuration &config)
{
    showArtistImages=config.get(constUseArtistImagesKey, showArtistImages);
    SqlLibraryModel::load(config);
}

void MpdLibraryModel::save(Configuration &config)
{
    config.set(constUseArtistImagesKey, showArtistImages);
    SqlLibraryModel::save(config);
}

void MpdLibraryModel::listSongs()
{
    listingTotal=db->trackCount();
    listingCurrent=0;
    if (listingTotal>0) {
        QTimer::singleShot(0, this, SLOT(listNextChunk()));
    } else {
        emit songListing(QList<Song>(), 100.0);
    }
}

void MpdLibraryModel::cancelListing()
{
    listingTotal=0;
}

static const int constMaxSongsInList=1000;

void MpdLibraryModel::listNextChunk()
{
    if (listingTotal<=0) {
        return;
    }

    QList<Song> songs=db->getTracks(listingCurrent, constMaxSongsInList);
    listingCurrent+=songs.count();
    emit songListing(songs, (listingCurrent*100.0)/(listingTotal*1.0));
    if (songs.count()>0) {
        if (listingCurrent==listingTotal) {
            emit songListing(QList<Song>(), 100.0);
        } else {
            QTimer::singleShot(0, this, SLOT(listNextChunk()));
        }
    }
}

void MpdLibraryModel::cover(const Song &song, const QImage &img, const QString &file)
{
    if (file.isEmpty() || img.isNull() || song.isFromOnlineService()) {
        return;
    }
    switch(topLevel()) {
    case T_Genre: {
        const Item *genre=root ? root->getChild(song.genres[0]) : nullptr;
        if (genre) {
            const Item *artist=static_cast<const CollectionItem *>(genre)->getChild(song.albumArtistOrComposer());
            if (artist) {
                const Item *album=static_cast<const CollectionItem *>(artist)->getChild(song.albumId());
                if (album) {
                    QModelIndex idx=index(album->getRow(), 0, index(artist->getRow(), 0, index(genre->getRow(), 0, QModelIndex())));
                    emit dataChanged(idx, idx);
                }
            }
        }
        break;
    }
    case T_Artist: {
        const Item *artist=root ? root->getChild(song.albumArtistOrComposer()) : nullptr;
        if (artist) {
            const Item *album=static_cast<const CollectionItem *>(artist)->getChild(song.albumId());
            if (album) {
                QModelIndex idx=index(album->getRow(), 0, index(artist->getRow(), 0, QModelIndex()));
                emit dataChanged(idx, idx);
            }
        }
        break;
    }
    case T_Album: {
        const Item *album=root ? root->getChild(song.albumArtistOrComposer()+song.albumId()) : nullptr;
        if (album) {
            QModelIndex idx=index(album->getRow(), 0, QModelIndex());
            emit dataChanged(idx, idx);
        }
        break;
    }
    default:
        break;
    }
}

void MpdLibraryModel::coverUpdated(const Song &song, const QImage &img, const QString &file)
{
    if (file.isEmpty() || img.isNull() || (T_Album==topLevel() && (song.isArtistImageRequest() || song.isComposerImageRequest()))) {
        return;
    }
    cover(song, img, file);
}

void MpdLibraryModel::artistImage(const Song &song, const QImage &img, const QString &file)
{
    if (file.isEmpty() || img.isNull() || T_Album==topLevel()) {
        return;
    }
    switch(topLevel()) {
    case T_Genre: {
        const Item *genre=root ? root->getChild(song.genres[0]) : nullptr;
        if (genre) {
            const Item *artist=static_cast<const CollectionItem *>(genre)->getChild(song.albumArtistOrComposer());
            if (artist) {
                QModelIndex idx=index(artist->getRow(), 0, index(genre->getRow(), 0, QModelIndex()));
                emit dataChanged(idx, idx);
            }
        }
        break;
    }
    case T_Artist: {
        const Item *artist=root ? root->getChild(song.albumArtistOrComposer()) : nullptr;
        if (artist) {
            QModelIndex idx=index(artist->getRow(), 0, QModelIndex());
            emit dataChanged(idx, idx);
        }
        break;
    }
    default:
        break;
    }
}

#include "moc_mpdlibrarymodel.cpp"
