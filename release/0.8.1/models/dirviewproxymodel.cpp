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

#include "dirviewitem.h"
#include "dirviewitemfile.h"
#include "dirviewproxymodel.h"

DirViewProxyModel::DirViewProxyModel(QObject *parent)
    : ProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
}

bool DirViewProxyModel::filterAcceptsDirViewItem(const DirViewItem * const item, QStringList strings) const
{
    strings << item->data();
    if (matchesFilter(strings)) {
        return true;
    }

    for (int i = 0; i < item->childCount(); i++) {
        if (filterAcceptsDirViewItem(item->child(i), strings)) {
            return true;
        }
    }

    return false;
}

bool DirViewProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!filterEnabled) {
        return true;
    }
    if (!isChildOfRoot(sourceParent)) {
        return true;
    }

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    DirViewItem *item = static_cast<DirViewItem *>(index.internalPointer());
    QModelIndex idx=index.parent();
    QStringList strings;

    // Traverse back up tree, so we get parent strings...
    while (idx.isValid()) {
        DirViewItem *i = static_cast<DirViewItem *>(idx.internalPointer());
        if (DirViewItem::Type_Dir!=i->type()) {
            break;
        }
        strings << i->data();
        idx=idx.parent();
    }

    if (DirViewItem::Type_Dir==item->type()) {
        // Check *all* children...
        if (filterAcceptsDirViewItem(item, strings)) {
            return true;
        }
    } else {
        strings << item->data();
        return matchesFilter(strings);
    }

    return false;
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
