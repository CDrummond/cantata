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

#include <QByteArray>
#include "playlisttableproxymodel.h"
#include "playlisttablemodel.h"

PlaylistTableProxyModel::PlaylistTableProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
}

bool PlaylistTableProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    int columnCount = sourceModel()->columnCount(sourceParent);

    for (int i = 0; i < columnCount; i++) {
        QModelIndex index = sourceModel()->index(sourceRow, i, sourceParent);
        if (sourceModel()->data(index).toString().contains(filterRegExp())) {
            return true;
        }
    }
    return false;
}

/* map proxy to real indexes and redirect to sourceModel */
QMimeData *PlaylistTableProxyModel::mimeData(const QModelIndexList &indexes) const
{
    QModelIndexList sourceIndexes;

    foreach(QModelIndex index, indexes) {
        sourceIndexes.append(mapToSource(index));
    }

    return sourceModel()->mimeData(sourceIndexes);
}

/* map proxy to real indexes and redirect to sourceModel */
bool PlaylistTableProxyModel::dropMimeData(const QMimeData *data,
        Qt::DropAction action, int row, int column, const QModelIndex & parent)
{
    const QModelIndex index = this->index(row, column, parent);
    const QModelIndex sourceIndex = mapToSource(index);

    return sourceModel()->dropMimeData(data, action, sourceIndex.row(), sourceIndex.column(), sourceIndex.parent());
}
