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

#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include "treeview.h"

class TableView : public TreeView
{
    Q_OBJECT

public:
    enum Roles {
        Role_Width = Qt::UserRole+400,
        Role_Hideable
    };

    TableView(const QString &cfgName, QWidget *parent=0, bool menuAlwaysAllowed=false);
    virtual ~TableView() { }
    void initHeader();
    void saveHeader();

private Q_SLOTS:
    void showMenu();
    void toggleHeaderItem(bool visible);
    void stretchToggled(bool e);

protected:
    QMenu *menu;
    QString configName;
};

#endif
