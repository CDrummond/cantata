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

#include <QtGui/QStackedWidget>
#include <QtGui/QAbstractItemView>
#include <QtCore/QSet>
#include "treeview.h"
#include "song.h"

class GroupedView;
class QAbstractItemModel;
class QAction;
class QItemSelectionModel;
class QHeaderView;
class QModelIndex;
class QMenu;

class PlayQueueTreeView : public TreeView
{
    Q_OBJECT

public:
    PlayQueueTreeView(QWidget *p);
    virtual ~PlayQueueTreeView();

    void initHeader();
    void saveHeader();

private Q_SLOTS:
    void showMenu();
    void toggleHeaderItem(bool visible);

private:
    QMenu *menu;
};

class PlayQueueView : public QStackedWidget
{
    Q_OBJECT

public:
    PlayQueueView(QWidget *parent=0);
    virtual ~PlayQueueView();

    void initHeader() {
        treeView->initHeader();
    }
    void saveHeader();
    void setGrouped(bool g);
    bool isGrouped() const { return currentWidget()==(QWidget *)groupedView; }
    void setAutoExpand(bool ae);
    bool isAutoExpand() const;
    void setStartClosed(bool sc);
    bool isStartClosed() const;
    void setFilterActive(bool f);
    void updateRows(qint32 row, bool scroll);
    void scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint);
    void setModel(QAbstractItemModel *m);
    void addAction(QAction *a);
    void setFocus();
    bool hasFocus();
    QAbstractItemModel * model();
    void setContextMenuPolicy(Qt::ContextMenuPolicy policy);
    bool haveUnSelectedItems();
    QItemSelectionModel * selectionModel() const;
    void setCurrentIndex(const QModelIndex &index);
    QHeaderView * header();
    QAbstractItemView * tree();
    QAbstractItemView * list();
    bool hasFocus() const;
    QModelIndexList selectedIndexes() const;
    QList<Song> selectedSongs() const;

Q_SIGNALS:
    void itemsSelected(bool);
    void doubleClicked(const QModelIndex &);

private:
    GroupedView *groupedView;
    PlayQueueTreeView *treeView;
};

#endif
