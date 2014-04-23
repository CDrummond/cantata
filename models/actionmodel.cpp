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

#include "actionmodel.h"
#include "stdactions.h"
#include "roles.h"

#ifdef ENABLE_UBUNTU
static const int Role_HasChildren = Qt::UserRole+500;
#endif

QVariant ActionModel::data(const QModelIndex &index, int role) const
{
    QVariant v;
    #ifdef ENABLE_UBUNTU
    if (Role_HasChildren==role) {
        return rowCount(index)>0;
    }
    #else
    Q_UNUSED(index)
    if (Cantata::Role_Actions==role) {
        v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction << StdActions::self()->addToPlayQueueAction);
    }
    #endif
    return v;
}

#ifdef ENABLE_UBUNTU
//Expose role names, so that they can be accessed via QML
QHash<int, QByteArray> ActionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Cantata::Role_MainText] = "mainText";
    roles[Cantata::Role_SubText] = "subText";
    roles[Cantata::Role_TitleText] = "titleText";
    roles[Cantata::Role_Image] = "image";
    roles[Role_HasChildren] = "hasChildren";
    return roles;
}
#endif
