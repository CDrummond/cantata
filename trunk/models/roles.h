/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifndef ROLES_H
#define ROLES_H

#include <QAbstractItemModel>

namespace Cantata {
    enum Roles
    {
        // ItemView...
        Role_ImageSize = Qt::UserRole+100,
        Role_MainText,
        Role_BriefMainText,
        Role_SubText,
        Role_TitleText,
        Role_Image,
        Role_Capacity,
        Role_CapacityText,
        Role_Actions,

        // GroupedView...
        Role_Key,
        Role_Id,
        Role_Song,
        Role_AlbumDuration,
        Role_Status,
        Role_CurrentStatus,
        Role_SongCount,

        Role_IsCollection,
        Role_CollectionId,
        Role_DropAdjust,

        // PlayQueueView ...
        Role_Decoration,

        // TableView...
        Role_Width,
        Role_Hideable,

        // PlayQueueModel...
        Role_Time
    };
}

#endif
