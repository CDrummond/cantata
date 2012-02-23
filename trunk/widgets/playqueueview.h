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

#ifndef PLAYQUEUEVIEW_H
#define PLAYQUEUEVIEW_H

#include "treeview.h"
class QAbstractItemDelegate;

class PlayQueueView : public TreeView
{
public:
    enum Roles {
        Role_Key = Qt::UserRole+512,
        Role_Artist,
        Role_AlbumArtist,
        Role_Title,
        Role_Track,
        Role_Duration,
        Role_StatusIcon
    };

    PlayQueueView(QWidget *parent=0);
    virtual ~PlayQueueView();

    void setGrouped(bool g);

private:
    bool grouped;
    QAbstractItemDelegate *prev;
    QAbstractItemDelegate *custom;
};

#endif
