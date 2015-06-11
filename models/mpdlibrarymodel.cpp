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

#include "mpdlibrarymodel.h"
#include "support/globalstatic.h"
#include "db/mpdlibrarydb.h"
#include "gui/settings.h"
#include "gui/covers.h"
#include "roles.h"
#include <QDebug>

GLOBAL_STATIC(MpdLibraryModel, instance)

MpdLibraryModel::MpdLibraryModel()
    : SqlLibraryModel(new MpdLibraryDb(0), 0)
{
    readSettings();
    connect(Covers::self(), SIGNAL(cover(Song,QImage,QString)), this, SLOT(cover(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(coverUpdated(Song,QImage,QString)), this, SLOT(coverUpdated(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(artistImage(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(composerImage(Song,QImage,QString)), this, SLOT(artistImage(Song,QImage,QString)));
}

QVariant MpdLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    switch (role) {
    case Cantata::Role_CoverSong: {
        QVariant v;
        Item *item = static_cast<Item *>(index.internalPointer());
        switch (item->getType()) {
        case T_Album:
            if (item->getSong().isEmpty()) {
                Song song=static_cast<MpdLibraryDb *>(db)->getCoverSong(T_Album==topLevel() ? static_cast<AlbumItem *>(item)->getArtistId() : item->getParent()->getId(), item->getId());
                item->setSong(song);
                if (T_Genre==topLevel()) {
                    song.genre=item->getParent()->getParent()->getId();
                }
            }
            v.setValue<Song>(item->getSong());
            break;
        case T_Artist:
            if (item->getSong().isEmpty()) {
                Song song=static_cast<MpdLibraryDb *>(db)->getCoverSong(item->getId());
                if (song.useComposer()) {
                    song.setComposerImageRequest();
                } else {
                    song.setArtistImageRequest();
                }
                if (T_Genre==topLevel()) {
                    song.genre=item->getParent()->getId();
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
    }
    return SqlLibraryModel::data(index, role);
}

void MpdLibraryModel::readSettings()
{
    settings(Settings::self()->libraryGrouping(), Settings::self()->librarySort(), Settings::self()->libraryAlbumSort());
}

void MpdLibraryModel::cover(const Song &song, const QImage &img, const QString &file)
{
    if (file.isEmpty() || img.isNull() || song.isFromOnlineService()) {
        return;
    }
    switch(topLevel()) {
    case T_Genre: {
        const Item *genre=root ? root->getChild(song.genre) : 0;
        if (genre) {
            const Item *artist=static_cast<const CollectionItem *>(genre)->getChild(song.artistOrComposer());
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
        const Item *artist=root ? root->getChild(song.artistOrComposer()) : 0;
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
        const Item *album=root ? root->getChild(song.artistOrComposer()+song.albumId()) : 0;
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
        const Item *genre=root ? root->getChild(song.genre) : 0;
        if (genre) {
            const Item *artist=static_cast<const CollectionItem *>(genre)->getChild(song.artistOrComposer());
            if (artist) {
                QModelIndex idx=index(artist->getRow(), 0, index(genre->getRow(), 0, QModelIndex()));
                emit dataChanged(idx, idx);
            }
        }
        break;
    }
    case T_Artist: {
        const Item *artist=root ? root->getChild(song.artistOrComposer()) : 0;
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
