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

#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <QListView>
#include <QPixmap>
#include "treeview.h"

class QIcon;

class QMenu;
class ListView : public QListView
{
    Q_OBJECT

public:
    ListView(QWidget *parent=0);
    virtual ~ListView();

    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    bool haveSelectedItems() const;
    bool haveUnSelectedItems() const;
    void startDrag(Qt::DropActions supportedActions) { TreeView::drag(supportedActions, this, selectedIndexes()); }
    void mouseReleaseEvent(QMouseEvent *event);
    QModelIndexList selectedIndexes() const { return selectedIndexes(true); }
    QModelIndexList selectedIndexes(bool sorted) const;
    virtual void setModel(QAbstractItemModel *m);
    void addDefaultAction(QAction *act);
    void setBackgroundImage(const QIcon &icon);
    void paintEvent(QPaintEvent *e);
    void installFilter(QObject *f) { eventFilter=f; installEventFilter(f); }
    QObject * filter() const { return eventFilter; }

private Q_SLOTS:
    void correctSelection();
    void showCustomContextMenu(const QPoint &pos);
    void checkDoubleClick(const QModelIndex &idx);

Q_SIGNALS:
    bool itemsSelected(bool);
    void itemDoubleClicked(const QModelIndex &idx);

private:
    QObject *eventFilter;
    QMenu *menu;
    QPixmap bgnd;
};

#endif
