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

#ifndef TREEVIEW_H
#define TREEVIEW_H

#include <QtGui/QTreeView>
#include <QtGui/QPixmap>
#include "touchscroll.h"

class TreeView : public QTreeView
{
    Q_OBJECT

public:
    TreeView(QWidget *parent=0);

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
    void setPixmap(const QPixmap &pix) {
        pixmap=pix;
    }
    void paintEvent(QPaintEvent *e);

private Q_SLOTS:
    void correctSelection();
    void showTouchContextMenu();

Q_SIGNALS:
    bool itemsSelected(bool);
    void goBack();

private:
    QPixmap pixmap;

    TOUCH_MEMBERS
};

#endif
