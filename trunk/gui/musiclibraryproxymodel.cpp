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
#include "musiclibraryitemsong.h"
#include "musiclibraryproxymodel.h"

MusicLibraryProxyModel::MusicLibraryProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent),
      _filterField(3)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
}

bool MusicLibraryProxyModel::filterAcceptsArtist(const MusicLibraryItem * const item) const
{
    switch (_filterField) {
    case 0: // Artist
        return item->data(0).toString().contains(filterRegExp());
    case 1: // Album
        for (int i = 0; i < item->childCount(); i++) {
            if (item->child(i)->data(0).toString().contains(filterRegExp()))
                return true;
        }
        break;
    case 2: // Song
        for (int i = 0; i < item->childCount(); i++) {
            for (int j = 0; j < item->child(i)->childCount(); j++) {
                if (item->child(i)->child(j)->data(0).toString().contains(filterRegExp()))
                    return true;
            }
        }
        break;
    case 3: // Any
        if (item->data(0).toString().contains(filterRegExp()))
            return true;

        for (int i = 0; i < item->childCount(); i++) {
            if (item->child(i)->data(0).toString().contains(filterRegExp()))
                return true;
        }

        for (int i = 0; i < item->childCount(); i++) {
            for (int j = 0; j < item->child(i)->childCount(); j++) {
                if (item->child(i)->child(j)->data(0).toString().contains(filterRegExp()))
                    return true;
            }
        }
        break;
    default:
        break;
    }

    return false;
}

bool MusicLibraryProxyModel::filterAcceptsAlbum(const MusicLibraryItem * const item) const
{
    switch (_filterField) {
    case 0: // Artist
        return item->parent()->data(0).toString().contains(filterRegExp());
    case 1: // Album
        return item->data(0).toString().contains(filterRegExp());
    case 2: // Song
        for (int i = 0; i < item->childCount(); i++) {
            if (item->child(i)->data(0).toString().contains(filterRegExp()))
                return true;
        }
        break;
    case 3: // Any
        if (item->parent()->data(0).toString().contains(filterRegExp()) || \
                item->data(0).toString().contains(filterRegExp()))
            return true;
        for (int i = 0; i < item->childCount(); i++) {
            if (item->child(i)->data(0).toString().contains(filterRegExp()))
                return true;
        }
        break;
    default:
        break;
    }

    return false;
}

bool MusicLibraryProxyModel::filterAcceptsSong(const MusicLibraryItem * const item) const
{
    switch (_filterField) {
    case 0: // Artist
        return item->parent()->parent()->data(0).toString().contains(filterRegExp());
    case 1: // Album
        return item->parent()->data(0).toString().contains(filterRegExp());
    case 2: // Song
        return item->data(0).toString().contains(filterRegExp());
    case 3: // Any
        return item->parent()->parent()->data(0).toString().contains(filterRegExp()) || \
               item->parent()->data(0).toString().contains(filterRegExp()) || \
               item->data(0).toString().contains(filterRegExp());
    default:
        break;
    }

    return false;
}

bool MusicLibraryProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const MusicLibraryItem * const item = static_cast<MusicLibraryItem *>(index.internalPointer());

    if (!_filterGenre.isEmpty() && !item->hasGenre(_filterGenre)) {
        return false;
    }

    switch (item->type()) {
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
    if (static_cast<MusicLibraryItem *>(left.internalPointer())->type() == MusicLibraryItem::Type_Song) {
        const MusicLibraryItemSong * const leftItem =
            static_cast<MusicLibraryItemSong *>(left.internalPointer());
        const MusicLibraryItemSong * const rightItem =
            static_cast<MusicLibraryItemSong *>(right.internalPointer());
        if (leftItem->disc() != rightItem->disc())
            return leftItem->disc() < rightItem->disc();
        return leftItem->track() < rightItem->track();
    } else if (static_cast<MusicLibraryItem *>(left.internalPointer())->type() == MusicLibraryItem::Type_Artist) {
        return static_cast<MusicLibraryItemArtist *>(left.internalPointer())->baseArtist().localeAwareCompare(
                static_cast<MusicLibraryItemArtist *>(right.internalPointer())->baseArtist())<0;
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

void MusicLibraryProxyModel::setFilterField(int field)
{
    _filterField = field;
}

int MusicLibraryProxyModel::filterField() const
{
    return _filterField;
}

void MusicLibraryProxyModel::setFilterGenre(const QString &genre)
{
    if (_filterGenre!=genre) {
        invalidate();
    }
    _filterGenre=genre;
}
