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

#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include "treeview.h"

class TableView : public TreeView
{
    Q_OBJECT

public:
    TableView(const QString &cfgGroup, QWidget *parent=nullptr, bool menuAlwaysAllowed=false);
    ~TableView() override { }
    void setModel(QAbstractItemModel *m) override;
    void initHeader();
    void saveHeader();
    void scrollTo(const QModelIndex &index, ScrollHint hint) override;

private Q_SLOTS:
    void showMenu(const QPoint &pos);
    void toggleHeaderItem(bool visible);
    void stretchToggled(bool e);
    void alignmentChanged();

protected:
    QMenu *menu;
    QString configGroup;
    int menuIsForCol;
    QAction *alignAction;
    QAction *alignLeftAction;
    QAction *alignCenterAction;
    QAction *alignRightAction;
};

#endif
