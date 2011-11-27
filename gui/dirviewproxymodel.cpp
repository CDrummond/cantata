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

#include "dirviewitem.h"
#include "dirviewproxymodel.h"

DirViewProxyModel::DirViewProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
}

bool DirViewProxyModel::filterAcceptsDirViewItem(const DirViewItem * const item, bool first) const
{
    DirViewItem *child;
    DirViewItem *parent;

    // check if self matches regex
    if (item->data(0).toString().contains(filterRegExp())) {
        return true;
    }

    // check if a parent matches regex
    if (first) {
        // only need to check parents on first level of recursion
        parent = item->parent();
        while (parent->type() != DirViewItem::Type_Root) {
            if (parent->data(0).toString().contains(filterRegExp())) {
                return true;
            }
            parent = parent->parent();
        }
    }

    // check if child matches regex
    for (int i = 0; i < item->childCount(); i++) {
        child = item->child(i);
        if (filterAcceptsDirViewItem(child, false)) {
            return true;
        }
    }

    return false;
}


bool DirViewProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    DirViewItem *item = static_cast<DirViewItem *>(index.internalPointer());

    return filterAcceptsDirViewItem(item, true);
}



bool DirViewProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const DirViewItem * const leftItem = static_cast<DirViewItem *>(left.internalPointer());
    const DirViewItem * const rightItem = static_cast<DirViewItem *>(right.internalPointer());

    if (leftItem->type() == DirViewItem::Type_Dir &&
            rightItem->type() == DirViewItem::Type_File)
        return true;

    if (leftItem->type() == DirViewItem::Type_File &&
            rightItem->type() == DirViewItem::Type_Dir)
        return false;

    return QSortFilterProxyModel::lessThan(left, right);
}
