/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "proxymodel.h"

void ProxyModel::update(const QString &text, const QString &genre)
{
    if (text.length()<2 && genre.isEmpty()) {
        bool wasEmpty=isEmpty();
        filterEnabled=false;
        filterGenre=genre;
        if (!filterRegExp().isEmpty()) {
             setFilterRegExp(QString());
        } else if (!wasEmpty) {
            invalidate();
        }
    } else {
        filterEnabled=true;
        filterGenre=genre;
        if (text!=filterRegExp().pattern()) {
            setFilterRegExp(text);
        } else {
            invalidate();
        }
    }
}

bool ProxyModel::isChildOfRoot(const QModelIndex &idx) const {
    if (!rootIndex.isValid()) {
        return true;
    }

    QModelIndex i=idx;
    while(i.isValid()) {
        if (i==rootIndex) {
            return true;
        }
        i=i.parent();
    }
    return false;
}
