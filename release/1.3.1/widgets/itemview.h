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

#ifndef ITEMVIEW_H
#define ITEMVIEW_H

#include "ui_itemview.h"
#include "treeview.h"
#include <QMap>
#include <QList>
#include <QPair>

class ProxyModel;
class Spinner;
class QAction;
class QTimer;
class GroupedView;
class ActionItemDelegate;
class MessageOverlay;
class Icon;
class TableView;

class ViewEventHandler : public QObject
{
    Q_OBJECT
public:
    ViewEventHandler(ActionItemDelegate *d, QAbstractItemView *v);

Q_SIGNALS:
    void escPressed();

protected:
    bool eventFilter(QObject *obj, QEvent *event);

protected:
    ActionItemDelegate *delegate;
    QAbstractItemView *view;
    bool interceptBackspace;
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
        Mode_BasicTree,
        Mode_DetailedTree,

        Mode_GroupedTree,

        Mode_Table,

        Mode_List,
        Mode_IconTop,

        Mode_Count
    };

    enum Role
    {
        Role_ImageSize = Qt::UserRole+100,
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
    void allowTableView(TableView *v);
    void addAction(QAction *act);
    void setMode(Mode m);
    Mode viewMode() const { return mode; }
    void setLevel(int level, bool haveChildren=true);
    QAbstractItemView * view() const;
    void setModel(ProxyModel *m);
    void clearSelection() { view()->selectionModel()->clearSelection(); }
    void setCurrentIndex(const QModelIndex &idx) { view()->setCurrentIndex(idx); }
    void select(const QModelIndex &idx);
    QModelIndexList selectedIndexes(bool sorted=true) const;
    QString searchText() const;
    QString searchCategory() const;
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
    void setSearchLabelText(const QString &text);
    void setSearchVisible(bool v);
    bool isSearchActive() const;
    void setStartClosed(bool sc);
    bool isStartClosed();
    void expandAll(const QModelIndex &index=QModelIndex());
    void expand(const QModelIndex &index, bool singleOnly=false);
    void showMessage(const QString &message, int timeout);
    void setBackgroundImage(const QIcon &icon);
    bool isAnimated() const;
    void setAnimated(bool a);
    void setPermanentSearch();
    void setSearchCategories(const QList<QPair<QString, QString> > &categories);
    void setSearchCategory(const QString &id);
    void setSearchResetLevel(int l) { searchResetLevel=l; }
    void setSearchIndex(const QModelIndex &idx) { searchIndex=idx; }
    void hideBackAction();

private:
    bool usingTreeView() const { return mode<=Mode_DetailedTree; }
    bool usingListView() const { return mode>=Mode_List; }
    bool eventFilter(QObject *o, QEvent *e);

public Q_SLOTS:
    void focusSearch();
    void focusView();
    void showSpinner(bool v=true);
    void hideSpinner();
    void collectionRemoved(quint32 key);
    void updateRows();
    void updateRows(const QModelIndex &idx);
    void backActivated();
    void homeActivated();
    void setExpanded(const QModelIndex &idx, bool exp=true);
    void closeSearch();

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
    void doSearch();
    void searchActive(bool a);
    void activateItem(const QModelIndex &index, bool emitRootSet=true);

private:
    void collapseToLevel();
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
    TableView *tableView;
    Spinner *spinner;
    MessageOverlay *msgOverlay;
    QIcon bgndIcon;
    bool performedSearch;
    int searchResetLevel;
    QModelIndex searchIndex;
};

#endif
