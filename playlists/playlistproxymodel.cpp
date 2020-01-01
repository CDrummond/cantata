/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "playlistproxymodel.h"
#include "dynamicplaylists.h"

PlaylistProxyModel::PlaylistProxyModel(QObject *parent)
    : ProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
    sort(0);
}

bool PlaylistProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!filterEnabled) {
        return true;
    }
    if (!isChildOfRoot(sourceParent)) {
        return true;
    }
    if (matchesFilter(QStringList() << sourceModel()->data(sourceModel()->index(sourceRow, 0, sourceParent), Qt::DisplayRole).toString())) {
        return true;
    }

    RulesPlaylists *rules = qobject_cast<RulesPlaylists *>(sourceModel());

    if (rules) {
        RulesPlaylists::Entry item = rules->entry(sourceRow);
        for (const RulesPlaylists::Rule &r: item.rules) {
            RulesPlaylists::Rule::ConstIterator it=r.constBegin();
            RulesPlaylists::Rule::ConstIterator end=r.constEnd();
            for (; it!=end; ++it) {
                if (matchesFilter(QStringList() << it.value())) {
                    return true;
                }
            }
        }
    }
    return false;
}
