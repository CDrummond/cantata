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

#include "albumsproxymodel.h"

AlbumsProxyModel::AlbumsProxyModel(QObject *parent)
    : ProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
    sort(0);
}

bool AlbumsProxyModel::filterAcceptsAlbum(AlbumsModel::Item *item) const
{
    AlbumsModel::AlbumItem *ai = static_cast<AlbumsModel::AlbumItem *>(item);
    if (!filterGenre.isEmpty() && !ai->genres.contains(filterGenre)) {
        return false;
    }

    if(ai->artist.contains(filterRegExp()) || ai->album.contains(filterRegExp())) {
        return true;
    }

    foreach (const AlbumsModel::SongItem *song, ai->songs) {
        if (song->displayTitle().contains(filterRegExp())) {
            return true;
        }
    }
    return false;
}

bool AlbumsProxyModel::filterAcceptsSong(AlbumsModel::Item *item) const
{
    AlbumsModel::SongItem *si = static_cast<AlbumsModel::SongItem *>(item);

    if (!filterGenre.isEmpty() && si->genre!=filterGenre) {
        return false;
    }

    return si->displayTitle().contains(filterRegExp()) || si->parent->artist.contains(filterRegExp()) || si->parent->album.contains(filterRegExp());
}

bool AlbumsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!filterEnabled) {
        return true;
    }
    if (!isChildOfRoot(sourceParent)) {
        return true;
    }

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    AlbumsModel::Item *item = static_cast<AlbumsModel::Item *>(index.internalPointer());

    if (!item) {
        return false;
    }

    return item->isAlbum() ? filterAcceptsAlbum(item) : filterAcceptsSong(item);
}

bool AlbumsProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    AlbumsModel::Item *l=static_cast<AlbumsModel::Item *>(left.internalPointer());
    AlbumsModel::Item *r=static_cast<AlbumsModel::Item *>(right.internalPointer());

    if (l->isAlbum() && r->isAlbum()) {
        const AlbumsModel::AlbumItem * const leftItem = static_cast<AlbumsModel::AlbumItem *>(l);
        const AlbumsModel::AlbumItem * const rightItem = static_cast<AlbumsModel::AlbumItem *>(r);
        if (leftItem->isSingleTracks != rightItem->isSingleTracks) {
            return leftItem->isSingleTracks > rightItem->isSingleTracks;
        }
        return compareStrings(leftItem->name, rightItem->name)<0;
    } else if(!l->isAlbum() && !r->isAlbum()) {
        const AlbumsModel::SongItem * const leftItem = static_cast<AlbumsModel::SongItem *>(l);
        const AlbumsModel::SongItem * const rightItem = static_cast<AlbumsModel::SongItem *>(r);
        bool singleTracks=leftItem->parent->isSingleTracks;

        if (singleTracks) {
            int compare=compareStrings(leftItem->artistSong(), rightItem->artistSong());
            if (0!=compare) {
                return compare<0;
            }
        }
        if (leftItem->disc != rightItem->disc) {
            return leftItem->disc < rightItem->disc;
        }
        return leftItem->track < rightItem->track;
    }

    return false;
}
