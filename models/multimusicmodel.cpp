/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "multimusicmodel.h"
#include <QStringList>

MultiMusicModel::MultiMusicModel(QObject *parent)
    : MusicModel(parent)
{
}

MultiMusicModel::~MultiMusicModel()
{
    qDeleteAll(collections);
}

QModelIndex MultiMusicModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        MusicLibraryItem *p=static_cast<MusicLibraryItem *>(parent.internalPointer());

        if (p) {
            return row<p->childCount() ? createIndex(row, column, p->childItem(row)) : QModelIndex();
        }
    } else {
        return row<collections.count() ? createIndex(row, column, collections.at(row)) : QModelIndex();
    }

    return QModelIndex();
}

QModelIndex MultiMusicModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    MusicLibraryItem *childItem = static_cast<MusicLibraryItem *>(index.internalPointer());
    MusicLibraryItem *parentItem = childItem->parentItem();

    if (parentItem) {
        return createIndex(parentItem->parentItem() ? parentItem->row() : row(parentItem), 0, parentItem);
    } else {
        return QModelIndex();
    }
}

int MultiMusicModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    return parent.isValid() ? static_cast<MusicLibraryItem *>(parent.internalPointer())->childCount() : collections.count();
}

void MultiMusicModel::updateGenres()
{
    QSet<QString> newGenres;
    foreach (MusicLibraryItemRoot *col, collections) {
        newGenres+=col->genres();
    }
    if (newGenres!=colGenres) {
        colGenres=newGenres;
        emit updateGenres(colGenres);
    }
}

void MultiMusicModel::toggleGrouping()
{
    beginResetModel();
    foreach (MusicLibraryItemRoot *col, collections) {
        col->toggleGrouping();
    }
    endResetModel();
}

void MultiMusicModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres)
{
    foreach (MusicLibraryItemRoot *col, collections) {
        col->getDetails(artists, albumArtists, albums, genres);
    }
}

int MultiMusicModel::indexOf(const QString &id)
{
    int i=0;
    foreach (MusicLibraryItemRoot *col, collections) {
        if (col->id()==id) {
            return i;
        }
        i++;
    }
    return -1;
}

QList<Song> MultiMusicModel::songs(const QModelIndexList &indexes, bool playableOnly, bool fullPath) const
{
    QMap<MusicLibraryItem *, QList<Song> > colSongs;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());
        MusicLibraryItem *p=item;

        while (p->parentItem()) {
            p=p->parentItem();
        }

        if (!p) {
            continue;
        }

        MusicLibraryItemRoot *parent=static_cast<MusicLibraryItemRoot *>(p);

        if (playableOnly && !parent->canPlaySongs()) {
            continue;
        }

        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            if (static_cast<MusicLibraryItemRoot *>(parent)->flat()) {
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                    if (MusicLibraryItem::Type_Song==song->itemType() && !colSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                        colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), fullPath);
                    }
                }
            } else {
                // First, sort all artists as they would appear in UI...
                QList<MusicLibraryItem *> artists=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
                qSort(artists.begin(), artists.end(), MusicLibraryItemArtist::lessThan);
                foreach (MusicLibraryItem *a, artists) {
                    const MusicLibraryItemContainer *artist=static_cast<const MusicLibraryItemContainer *>(a);
                    // Now sort all albums as they would appear in UI...
                    QList<MusicLibraryItem *> artistAlbums=static_cast<const MusicLibraryItemContainer *>(artist)->childItems();
                    qSort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);
                    foreach (MusicLibraryItem *i, artistAlbums) {
                        const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                        foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                            if (MusicLibraryItem::Type_Song==song->itemType() && !colSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                                colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), fullPath);
                            }
                        }
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Artist: {
            // First, sort all albums as they would appear in UI...
            QList<MusicLibraryItem *> artistAlbums=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
            qSort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);

            foreach (MusicLibraryItem *i, artistAlbums) {
                const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                    if (MusicLibraryItem::Type_Song==song->itemType() && !colSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                        colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), fullPath);
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album:
            foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                if (MusicLibraryItem::Type_Song==song->itemType() && !colSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                    colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), fullPath);
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (!colSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(item)->song())) {
                colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(item)->song(), fullPath);
            }
            break;
        default:
            break;
        }
    }

    QList<Song> songs;
    QMap<MusicLibraryItem *, QList<Song> >::Iterator it(colSongs.begin());
    QMap<MusicLibraryItem *, QList<Song> >::Iterator end(colSongs.end());

    for (; it!=end; ++it) {
        songs.append(it.value());
    }

    return songs;
}

QStringList MultiMusicModel::filenames(const QModelIndexList &indexes, bool playableOnly, bool fullPath) const
{
    QList<Song> songList=songs(indexes, playableOnly, fullPath);
    QStringList fnames;
    foreach (const Song &s, songList) {
        fnames.append(s.file);
    }
    return fnames;
}
