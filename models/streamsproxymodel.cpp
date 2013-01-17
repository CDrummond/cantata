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

    if (item->isCategory()) {
        StreamsModel::CategoryItem *cat = static_cast<StreamsModel::CategoryItem *>(item);

        if (!filterGenre.isEmpty() && !cat->genres.contains(filterGenre)) {
            return false;
        }

        if (cat->name.contains(filterRegExp())) {
            return true;
        }

        foreach (StreamsModel::StreamItem *s, cat->streams) {
            if (matchesFilter(QStringList() << cat->name << s->name)) {
                return true;
            }
        }
    } else {
        StreamsModel::StreamItem *s = static_cast<StreamsModel::StreamItem *>(item);

        if (!filterGenre.isEmpty() && s->genre!=filterGenre) {
            return false;
        }
        return matchesFilter(QStringList() << s->name);
    }

    return false;
}
