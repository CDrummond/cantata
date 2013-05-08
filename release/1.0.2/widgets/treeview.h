/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef TREEVIEW_H
#define TREEVIEW_H

#include <QTreeView>

class TreeView : public QTreeView
{
    Q_OBJECT

public:
    TreeView(QWidget *parent=0, bool menuAlwaysAllowed=false);
    virtual ~TreeView();

    void setPageDefaults();
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    bool haveSelectedItems() const;
    bool haveUnSelectedItems() const;
    void startDrag(Qt::DropActions supportedActions);
    void mouseReleaseEvent(QMouseEvent *event);
    QModelIndexList selectedIndexes() const;
    void expandAll();
    void expand(const QModelIndex &idx);
    virtual void setModel(QAbstractItemModel *m);

private Q_SLOTS:
    void correctSelection();

Q_SIGNALS:
    bool itemsSelected(bool);
private:
    bool alwaysAllowMenu;
};

#endif
