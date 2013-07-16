/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "playlistsproxymodel.h"
#include "playlistsmodel.h"

PlaylistsProxyModel::PlaylistsProxyModel(QObject *parent)
    : ProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
}

bool PlaylistsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!filterEnabled) {
        return true;
    }
    if (!isChildOfRoot(sourceParent)) {
        return true;
    }

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    PlaylistsModel::Item *item = static_cast<PlaylistsModel::Item *>(index.internalPointer());

    if (item->isPlaylist()) {
        PlaylistsModel::PlaylistItem *pl = static_cast<PlaylistsModel::PlaylistItem *>(item);

        if (!filterGenre.isEmpty() && !pl->genres.contains(filterGenre)) {
            return false;
        }

        if (matchesFilter(QStringList() << pl->name)) {
            return true;
        }

        foreach (PlaylistsModel::SongItem *s, pl->songs) {
            if (matchesFilter(*s)) {
                return true;
            }
        }
    } else {
        PlaylistsModel::SongItem *s = static_cast<PlaylistsModel::SongItem *>(item);

        if (!filterGenre.isEmpty() && s->genre!=filterGenre) {
            return false;
        }
        return matchesFilter(*s);
    }

    return false;
}

bool PlaylistsProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    PlaylistsModel::Item *l=static_cast<PlaylistsModel::Item *>(left.internalPointer());
    PlaylistsModel::Item *r=static_cast<PlaylistsModel::Item *>(right.internalPointer());

    if (l->isPlaylist() && r->isPlaylist()) {
        return compareNames(static_cast<PlaylistsModel::PlaylistItem *>(l)->name, static_cast<PlaylistsModel::PlaylistItem *>(r)->name);
    } else if(!l->isPlaylist() && !r->isPlaylist()) {
        return left.row()<right.row();
    }

    return false;
}
