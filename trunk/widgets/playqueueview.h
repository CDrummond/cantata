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

#ifndef PLAYQUEUEVIEW_H
#define PLAYQUEUEVIEW_H

#include <QStackedWidget>
#include <QAbstractItemView>
#include <QSet>
#include <QPixmap>
#include <QImage>
#include <QPropertyAnimation>
#include "tableview.h"
#include "listview.h"
#include "groupedview.h"
#include "song.h"
#include "itemview.h"

class QAbstractItemModel;
class QAction;
class Action;
class QItemSelectionModel;
class QModelIndex;
class QMenu;
class Spinner;
class PlayQueueView;
class MessageOverlay;

class PlayQueueListView : public ListView
{
    Q_OBJECT
public:
    PlayQueueListView(PlayQueueView *p);
    virtual ~PlayQueueListView() { }
    void paintEvent(QPaintEvent *e);
    void setModel(QAbstractItemModel *model);
public Q_SLOTS:
    void coverLoaded(const Song &song, int size);
private:
    PlayQueueView *view;
};

class PlayQueueTreeView : public TableView
{
public:
    PlayQueueTreeView(PlayQueueView *p);
    virtual ~PlayQueueTreeView() { }
    void paintEvent(QPaintEvent *e);
private:
    PlayQueueView *view;
};

class PlayQueueGroupedView : public GroupedView
{
public:
    PlayQueueGroupedView(PlayQueueView *p);
    virtual ~PlayQueueGroupedView();
    void paintEvent(QPaintEvent *e);
private:
    PlayQueueView *view;
};

class PlayQueueView : public QStackedWidget
{
    Q_OBJECT
    Q_PROPERTY(float fade READ fade WRITE setFade)

public:
    enum BackgroundImage {
        BI_None,
        BI_Cover,
        BI_Custom
    };

    PlayQueueView(QWidget *parent=0);
    virtual ~PlayQueueView();

    void readConfig();
    void saveConfig();
    void saveHeader();
    void setMode(ItemView::Mode m);
    bool isGrouped() const { return ItemView::Mode_GroupedTree==mode; }
    void setAutoExpand(bool ae);
    bool isAutoExpand() const;
    void setStartClosed(bool sc);
    bool isStartClosed() const;
    void setFilterActive(bool f);
    void updateRows(qint32 row, quint16 curAlbum, bool scroll, bool forceScroll=false);
    void scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint);
    QModelIndex indexAt(const QPoint &point);
    void setModel(QAbstractItemModel *m) { view()->setModel(m); }
    void addAction(QAction *a);
    void setFocus();
    bool hasFocus();
    QAbstractItemModel * model() { return view()->model(); }
    bool haveSelectedItems();
    bool haveUnSelectedItems();
    void setCurrentIndex(const QModelIndex &idx) { view()->setCurrentIndex(idx); }
    void clearSelection();
    QAbstractItemView * view() const;
    bool hasFocus() const;
    QModelIndexList selectedIndexes(bool sorted=true) const;
    QList<Song> selectedSongs() const;
    float fade() { return fadeValue; }
    void setFade(float value);
    void updatePalette();
    Action * removeFromAct() { return removeFromAction; }

public Q_SLOTS:
    void showSpinner();
    void hideSpinner();
    void setImage(const QImage &img);
    void streamFetchStatus(const QString &msg);

Q_SIGNALS:
    void itemsSelected(bool);
    void doubleClicked(const QModelIndex &);
    void cancelStreamFetch();

private:
    void drawBackdrop(QWidget *widget, const QSize &size);

private:
    Action *removeFromAction;
    ItemView::Mode mode;
    PlayQueueListView *listView;
    PlayQueueGroupedView *groupedView;
    PlayQueueTreeView *treeView;
    Spinner *spinner;
    MessageOverlay *msgOverlay;

    BackgroundImage backgroundImageType;
    QPropertyAnimation animator;
    QImage curentCover;
    QPixmap curentBackground;
    QPixmap previousBackground;
    QSize lastBgndSize;
    double fadeValue;
    int backgroundOpacity;
    int backgroundBlur;
    QString customBackgroundFile;
    friend class PlayQueueListView;
    friend class PlayQueueGroupedView;
    friend class PlayQueueTreeView;
};

#endif
