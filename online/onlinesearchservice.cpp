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

#include "onlinesearchservice.h"
#include "models/roles.h"
#include "network/networkaccessmanager.h"

OnlineSearchService::OnlineSearchService(QObject *p)
    : SearchModel(p)
    , job(nullptr)
{
}

QVariant OnlineSearchService::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        switch (role) {
        case Cantata::Role_TitleText:
            return title();
        case Cantata::Role_SubText:
            return job ? tr("Searching...") : descr();
        case Qt::DecorationRole:
            return icon();
        default:
            break;
        }
    }
    switch (role) {
    case Cantata::Role_ListImage:
        return false;
    case Cantata::Role_CoverSong:
        return QVariant();
    default:
        break;
    }
    return SearchModel::data(index, role);
}

Song & OnlineSearchService::fixPath(Song &s) const
{
    s.setIsFromOnlineService(name());
    s.album=title();
    return encode(s);
}

void OnlineSearchService::cancel()
{
    if (job) {
        job->cancelAndDelete();
        job=nullptr;
    }
}

#include "moc_onlinesearchservice.cpp"
