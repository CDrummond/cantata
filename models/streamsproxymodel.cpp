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
#include "streamsproxymodel.h"
#include "streamsmodel.h"

StreamsProxyModel::StreamsProxyModel(QObject *parent)
    : ProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
    sort(0);
}

bool StreamsProxyModel::filterAcceptsItem(const void *i, QStringList strings) const
{
    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(i);
    strings << item->name;
    if (matchesFilter(strings)) {
        return true;
    }

    if (item->isCategory()) {
        const StreamsModel::CategoryItem *cat=static_cast<const StreamsModel::CategoryItem *>(item);
        foreach (const StreamsModel::Item *c, cat->children) {
            if (filterAcceptsItem(c, strings)) {
                return true;
            }
        }
    }

    return false;
}

bool StreamsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!filterEnabled) {
        return true;
    }
    if (!isChildOfRoot(sourceParent)) {
        return true;
    }

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    StreamsModel::Item *item = static_cast<StreamsModel::Item *>(index.internalPointer());
    QModelIndex idx=index.parent();
    QStringList strings;

    // Traverse back up tree, so we get parent strings...
    while (idx.isValid()) {
        StreamsModel::Item *i = static_cast<StreamsModel::Item *>(idx.internalPointer());
        if (!i->isCategory()) {
            break;
        }
        strings << i->name;
        idx=idx.parent();
    }

    if (item->isCategory()) {
        // Check *all* children...
        if (filterAcceptsItem(item, strings)) {
            return true;
        }
    } else {
        strings << item->name;
        return matchesFilter(strings);
    }

    return false;
}

bool StreamsProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const StreamsModel::Item * leftItem = static_cast<const StreamsModel::Item *>(left.internalPointer());
    const StreamsModel::Item * rightItem = static_cast<const StreamsModel::Item *>(right.internalPointer());

    if (leftItem->isCategory() && !rightItem->isCategory()) {
        return true;
    }
    if (!leftItem->isCategory() && rightItem->isCategory()) {
        return false;
    }

    if (leftItem->isCategory() && rightItem->isCategory()) {
        const StreamsModel::CategoryItem * leftCat = static_cast<const StreamsModel::CategoryItem *>(leftItem);
        const StreamsModel::CategoryItem * rightCat = static_cast<const StreamsModel::CategoryItem *>(rightItem);

        if (leftCat->isFavourites() && !rightCat->isFavourites()) {
            return true;
        }
        if (!leftCat->isFavourites() && rightCat->isFavourites()) {
            return false;
        }
        if (leftCat->isAll && !rightCat->isAll) {
            return true;
        }
        if (!leftCat->isAll && rightCat->isAll) {
            return false;
        }
    }

    return QSortFilterProxyModel::lessThan(left, right);
}
