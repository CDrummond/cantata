/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "musiclibraryitem.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryproxymodel.h"

MusicLibraryProxyModel::MusicLibraryProxyModel(QObject *parent)
    : ProxyModel(parent) // ,
//       filterField(3)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
}

bool MusicLibraryProxyModel::filterAcceptsRoot(const MusicLibraryItem *item) const
{
    foreach (const MusicLibraryItem *i, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
        if (filterAcceptsArtist(i)) {
            return true;
        }
    }

    return false;
}

bool MusicLibraryProxyModel::filterAcceptsArtist(const MusicLibraryItem *item) const
{
    foreach (const MusicLibraryItem *i, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
        if (filterAcceptsAlbum(i)) {
            return true;
        }
    }

    return false;
}

bool MusicLibraryProxyModel::filterAcceptsAlbum(const MusicLibraryItem *item) const
{
    foreach (const MusicLibraryItem *i, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
        if (filterAcceptsSong(i)) {
            return true;
        }
    }

    return false;
}

bool MusicLibraryProxyModel::filterAcceptsSong(const MusicLibraryItem *item) const
{
    return matchesFilter(static_cast<const MusicLibraryItemSong *>(item)->song());
}

bool MusicLibraryProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!filterEnabled) {
        return true;
    }
    if (!isChildOfRoot(sourceParent)) {
        return true;
    }

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    if (!filterGenre.isEmpty() && !item->hasGenre(filterGenre)) {
        return false;
    }

    switch (item->itemType()) {
    case MusicLibraryItem::Type_Root:
        return filterAcceptsRoot(item);
    case MusicLibraryItem::Type_Artist:
        return filterAcceptsArtist(item);
    case MusicLibraryItem::Type_Album:
        return filterAcceptsAlbum(item);
    case MusicLibraryItem::Type_Song:
        return filterAcceptsSong(item);
    default:
        break;
    }

    return false;
}

bool MusicLibraryProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (left.row()<0 || right.row()<0) {
        return left.row()<0;
    }
    if (static_cast<MusicLibraryItem *>(left.internalPointer())->itemType() == MusicLibraryItem::Type_Song) {
        const MusicLibraryItemSong *leftItem = static_cast<MusicLibraryItemSong *>(left.internalPointer());
        const MusicLibraryItemSong *rightItem = static_cast<MusicLibraryItemSong *>(right.internalPointer());
        bool isSingleTracks=static_cast<MusicLibraryItemAlbum *>(leftItem->parentItem())->isSingleTracks();

        if (isSingleTracks) {
            int compare=compareStrings(leftItem->song().artistSong(), rightItem->song().artistSong());
            if (0!=compare) {
                return compare<0;
            }
        }
        if (leftItem->song().type != rightItem->song().type) {
            return leftItem->song().type < rightItem->song().type;
        }
        if (leftItem->disc() != rightItem->disc()) {
            return leftItem->disc() < rightItem->disc();
        }
        return leftItem->track() < rightItem->track();
    } else if (static_cast<MusicLibraryItem *>(left.internalPointer())->itemType() == MusicLibraryItem::Type_Album) {
        const MusicLibraryItemAlbum *leftItem = static_cast<MusicLibraryItemAlbum *>(left.internalPointer());
        const MusicLibraryItemAlbum *rightItem = static_cast<MusicLibraryItemAlbum *>(right.internalPointer());

        if (leftItem->isSingleTracks() != rightItem->isSingleTracks()) {
            return leftItem->isSingleTracks() > rightItem->isSingleTracks();
        }
        if (MusicLibraryItemAlbum::showDate() && (leftItem->year() != rightItem->year())) {
            return leftItem->year() < rightItem->year();
        }
    } else if (static_cast<MusicLibraryItem *>(left.internalPointer())->itemType() == MusicLibraryItem::Type_Artist) {
        const MusicLibraryItemArtist *leftItem = static_cast<MusicLibraryItemArtist *>(left.internalPointer());
        const MusicLibraryItemArtist *rightItem = static_cast<MusicLibraryItemArtist *>(right.internalPointer());
        if (leftItem->isVarious() != rightItem->isVarious()) {
            return leftItem->isVarious() > rightItem->isVarious();
        }
        return compareStrings(leftItem->baseArtist(), rightItem->baseArtist())<0;
    }

    return QSortFilterProxyModel::lessThan(left, right);
}
