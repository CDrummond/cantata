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

bool AlbumsProxyModel::filterAcceptsAlbum(const AlbumsModel::AlbumItem *item) const
{
    if (!filterGenre.isEmpty() && !item->genres.contains(filterGenre)) {
        return false;
    }

    foreach (const AlbumsModel::SongItem *song, item->songs) {
        if (filterAcceptsSong(song)) {
            return true;
        }
    }
    return false;
}

bool AlbumsProxyModel::filterAcceptsSong(const AlbumsModel::SongItem *item) const
{
    if (!filterGenre.isEmpty() && item->genre!=filterGenre) {
        return false;
    }

    return matchesFilter(*item);
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

    return item->isAlbum() ? filterAcceptsAlbum(static_cast<AlbumsModel::AlbumItem *>(item)) : filterAcceptsSong(static_cast<AlbumsModel::SongItem *>(item));
}

bool AlbumsProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    AlbumsModel::Item *l=static_cast<AlbumsModel::Item *>(left.internalPointer());
    AlbumsModel::Item *r=static_cast<AlbumsModel::Item *>(right.internalPointer());

    if (l->isAlbum() && r->isAlbum()) {
        const AlbumsModel::AlbumItem * const leftItem = static_cast<AlbumsModel::AlbumItem *>(l);
        const AlbumsModel::AlbumItem * const rightItem = static_cast<AlbumsModel::AlbumItem *>(r);

        if (AlbumsModel::Sort_AlbumArtist!=AlbumsModel::self()->albumSort() && leftItem->type != rightItem->type) {
            return leftItem->type > rightItem->type;
        }
        return *leftItem < *rightItem;
    } else if(!l->isAlbum() && !r->isAlbum()) {
        const AlbumsModel::SongItem * const leftItem = static_cast<AlbumsModel::SongItem *>(l);
        const AlbumsModel::SongItem * const rightItem = static_cast<AlbumsModel::SongItem *>(r);
        return *leftItem < *rightItem;
    }

    return false;
}
