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

#include "smartplaylists.h"
#include "support/monoicon.h"
#include "support/globalstatic.h"
#include "models/roles.h"

GLOBAL_STATIC(SmartPlaylists, instance)

SmartPlaylists::SmartPlaylists()
    : RulesPlaylists(FontAwesome::graduationcap, "smart")
{
}

QString SmartPlaylists::name() const
{
    return QLatin1String("smart");
}

QString SmartPlaylists::title() const
{
    return tr("Smart Playlists");
}

QString SmartPlaylists::descr() const
{
    return tr("Rules based playlists");
}

QVariant SmartPlaylists::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return RulesPlaylists::data(index, role);
    }

    if (index.parent().isValid() || index.row()>=entryList.count()) {
        return QVariant();
    }

    switch (role) {
    case Qt::DecorationRole:
        return icn;
    case Cantata::Role_Actions:
        return ActionModel::data(index, role);
    default:
        return RulesPlaylists::data(index, role);
    }
}

#include "moc_smartplaylists.cpp"
