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
#ifndef PLAYLISTSPROXYMODEL_H
#define PLAYLISTSPROXYMODEL_H

#include "proxymodel.h"

class PlaylistsProxyModel : public ProxyModel
{
    Q_OBJECT

public:
    PlaylistsProxyModel(QObject *parent = 0);
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    static bool compareNames(const QString &l, const QString &r) { return l.localeAwareCompare(r)<0; }
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};

#endif
