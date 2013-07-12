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

#ifndef ITEMVIEW_H
#define ITEMVIEW_H

#include "ui_itemview.h"
#include <QMap>

class ProxyModel;
class Spinner;
class QAction;
class QTimer;
class GroupedView;
class ActionItemDelegate;
class MessageOverlay;

class ViewEventHandler : public QObject
{
public:
    ViewEventHandler(ActionItemDelegate *d, QAbstractItemView *v);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

protected:
    ActionItemDelegate *delegate;
    QAbstractItemView *view;
};

class ListViewEventHandler : public ViewEventHandler
{
public:
    ListViewEventHandler(ActionItemDelegate *d, QAbstractItemView *v, QAction *a);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    QAction *act;
};

class ItemView : public QWidget, public Ui::ItemView
{
    Q_OBJECT
public:

    enum Mode
    {
        Mode_SimpleTree,
        Mode_DetailedTree,
        Mode_List,
        Mode_IconTop,
        Mode_GroupedTree,

        Mode_Count
    };

    enum Role
    {
        Role_ImageSize = Qt::UserRole+256,
        Role_MainText,
        Role_SubText,
        Role_TitleText,
        Role_Image,
        Role_Capacity,
        Role_CapacityText,
        Role_Actions
    };

    static Mode toMode(const QString &str);
    static QString modeStr(Mode m);

    static void setup();

    ItemView(QWidget *p=0);
    virtual ~ItemView();

    void allowGroupedView();
    void addAction(QAction *act);
    void setMode(Mode m);
    Mode viewMode() const { return mode; }
    void setLevel(int level, bool haveChildren=true);
    QAbstractItemView * view() const;
    void setModel(ProxyModel *m);
    void clearSelection() { selectionModel()->clearSelection(); }
    QItemSelectionModel * selectionModel() const { return view()->selectionModel(); }
    void setCurrentIndex(const QModelIndex &idx) { view()->setCurrentIndex(idx); }
    void select(const QModelIndex &idx);
    QModelIndexList selectedIndexes() const;
    QString searchText() const;
    void clearSearchText();
    void setUniformRowHeights(bool v);
    void setAcceptDrops(bool v);
    void setDragDropOverwriteMode(bool v);
    void setDragDropMode(QAbstractItemView::DragDropMode v);
    void setGridSize(const QSize &sz);
    QSize gridSize() const { return listView->gridSize(); }
    void update();
    void setDeleteAction(QAction *act);
    void setRootIsDecorated(bool v) { treeView->setRootIsDecorated(v); }
    void showIndex(const QModelIndex &idx, bool scrollTo);
    void focusSearch();
    void setSearchLabelText(const QString &text);
    void setSearchVisible(bool v);
    bool isSearchActive() const;
    void setStartClosed(bool sc);
    bool isStartClosed();
    void expandAll();
    void expand(const QModelIndex &index);
    void showMessage(const QString &message, int timeout);
    void setBackgroundImage(const QIcon &icon);

public Q_SLOTS:
    void showSpinner(bool v=true);
    void hideSpinner();
    void collectionRemoved(quint32 key);
    void updateRows();
    void updateRows(const QModelIndex &idx);
    void backActivated();
    void homeActivated();
    void setExpanded(const QModelIndex &idx, bool exp=true);

Q_SIGNALS:
    void searchItems();
    void searchIsActive(bool);
    void itemsSelected(bool);
    void doubleClicked(const QModelIndex &);
    void rootIndexSet(const QModelIndex &);

private Q_SLOTS:
    void itemClicked(const QModelIndex &index);
    void itemActivated(const QModelIndex &index);
    void delaySearchItems();
    void activateItem(const QModelIndex &index, bool emitRootSet=true);

private:
    QAction * getAction(const QModelIndex &index);

private:
    QTimer *searchTimer;
    ProxyModel *itemModel;
    QAction *backAction;
    QAction *homeAction;
    int currentLevel;
    Mode mode;
    QMap<int, QString> prevText;
    QModelIndex prevTopIndex;
    QSize iconGridSize;
    QSize listGridSize;
    GroupedView *groupedView;
    Spinner *spinner;
    MessageOverlay *msgOverlay;
    QIcon bgndIcon;
};

#endif
