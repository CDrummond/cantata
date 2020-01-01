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

#ifndef ITEMVIEW_H
#define ITEMVIEW_H

#include "config.h"
#include "ui_itemview.h"
#include "treeview.h"
#include <QMap>
#include <QList>
#include <QPair>

class Spinner;
class QTimer;
class GroupedView;
class ActionItemDelegate;
class MessageOverlay;
class TableView;
class QMenu;
class Configuration;
class CategorizedView;

class KeyEventHandler : public QObject
{
    Q_OBJECT
public:
    KeyEventHandler(QAbstractItemView *v, QAction *a=nullptr);
    void setDeleteAction(QAction *a) { deleteAct=a; }
Q_SIGNALS:
    void backspacePressed();
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
protected:
    QAbstractItemView *view;
    QAction *deleteAct;
    bool interceptBackspace;
};

class ViewEventHandler : public KeyEventHandler
{
public:
    ViewEventHandler(ActionItemDelegate *d, QAbstractItemView *v);
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
private:
    ActionItemDelegate *delegate;
};

class ItemView : public QWidget, public Ui::ItemView
{
    Q_OBJECT
public:

    enum Mode
    {
        Mode_BasicTree,
        Mode_SimpleTree,
        Mode_DetailedTree,

        Mode_GroupedTree,

        Mode_Table,
        Mode_Categorized,

        Mode_List,
        Mode_IconTop,

        Mode_Count
    };

    static Mode toMode(const QString &str);
    static QString modeStr(Mode m);
    static void setup();
    static const QLatin1String constSearchActiveKey;
    static const QLatin1String constViewModeKey;
    static const QLatin1String constStartClosedKey;
    static const QLatin1String constSearchCategoryKey;

    ItemView(QWidget *p=nullptr);
    ~ItemView() override;

    void alwaysShowHeader();
    void load(Configuration &config);
    void save(Configuration &config);
    void allowGroupedView();
    void allowTableView(TableView *v);
    void allowCategorized();
    void addAction(QAction *act);
    void addSeparator();
    void setMode(Mode m);
    Mode viewMode() const { return mode; }
    QAbstractItemView * view() const;
    void setModel(QAbstractItemModel *m);
    void clearSelection() { view()->selectionModel()->clearSelection(); }
    void setCurrentIndex(const QModelIndex &idx) { view()->setCurrentIndex(idx); }
    void select(const QModelIndex &idx);
    QModelIndexList selectedIndexes(bool sorted=true) const;
    bool searchVisible() const;
    QString searchText() const;
    QString searchCategory() const;
    void clearSearchText();
    void setUniformRowHeights(bool v);
    void setAcceptDrops(bool v);
    void setDragDropOverwriteMode(bool v);
    void setDragDropMode(QAbstractItemView::DragDropMode v);
    void update();
    void setDeleteAction(QAction *act);
    void setRootIsDecorated(bool v) { treeView->setRootIsDecorated(v); }
    void showIndex(const QModelIndex &idx, bool scrollTo);
    void setSearchVisible(bool v);
    bool isSearchActive() const;
    void setSearchToolTip(const QString &str);
    void setStartClosed(bool sc);
    bool isStartClosed();
    void expandAll(const QModelIndex &index=QModelIndex());
    void expand(const QModelIndex &index, bool singleOnly=false);
    void showMessage(const QString &message, int timeout);
    void setBackgroundImage(const QIcon &icon);
    bool isAnimated() const;
    void setAnimated(bool a);
    void setPermanentSearch();
    void hideSearch();
    void setSearchCategories(const QList<SearchWidget::Category> &categories);
    void setSearchCategory(const QString &id);
    void setSearchResetLevel(int l) { searchResetLevel=l; }
    void goToTop();
    void setOpenAfterSearch(bool o) { openFirstLevelAfterSearch=o; }
    void setEnabled(bool en);
    void setMinSearchDebounce(unsigned int val) { minSearchDebounce = val; }
    void setInfoText(const QString &info);

private:
    void setLevel(int level, bool haveChildren=true);
    bool usingTreeView() const { return mode<=Mode_DetailedTree; }
    bool usingListView() const { return mode>=Mode_List; }

public Q_SLOTS:
    void focusSearch(const QString &text=QString());
    void focusView();
    void showSpinner(bool v=true);
    void hideSpinner();
    void updating();
    void updated();
    void collectionRemoved(quint32 key);
    void updateRows();
    void updateRows(const QModelIndex &idx);
    void backActivated();
    void setExpanded(const QModelIndex &idx, bool exp=true);
    void closeSearch();

Q_SIGNALS:
    void searchItems();
    void searchIsActive(bool);
    void itemsSelected(bool);
    void doubleClicked(const QModelIndex &);
    void rootIndexSet(const QModelIndex &);
    void headerClicked(int level);
    void updateToPlayQueue(const QModelIndex &idx, bool replace);

private Q_SLOTS:
    void itemClicked(const QModelIndex &index);
    void itemActivated(const QModelIndex &index);
    void delaySearchItems();
    void doSearch();
    void searchActive(bool a);
    void activateItem(const QModelIndex &index, bool emitRootSet=true);
    void modelReset();
    void dataChanged(const QModelIndex &tl, const QModelIndex &br);
    void addTitleButtonClicked();
    void replaceTitleButtonClicked();
    void coverLoaded(const Song &song, int size);
    void zoomIn();
    void zoomOut();

private:
    QAction * getAction(const QModelIndex &index);
    void setTitle();
    void controlViewFrame();

private:
    QTimer *searchTimer;
    QAbstractItemModel *itemModel;
    int currentLevel;
    Mode mode;
    QModelIndexList prevTopIndex;
    QSize iconGridSize;
    QSize listGridSize;
    GroupedView *groupedView;
    TableView *tableView;
    #ifdef ENABLE_CATEGORIZED_VIEW
    CategorizedView *categorizedView;
    #endif
    Spinner *spinner;
    MessageOverlay *msgOverlay;
    QIcon bgndIcon;
    bool performedSearch;
    int searchResetLevel;
    bool openFirstLevelAfterSearch;
    bool initialised;
    unsigned int minSearchDebounce;
};

#endif
