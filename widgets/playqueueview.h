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

#ifndef PLAYQUEUEVIEW_H
#define PLAYQUEUEVIEW_H

#include <QStackedWidget>
#include <QAbstractItemView>
#include <QSet>
#include <QPixmap>
#include <QImage>
#include <QPropertyAnimation>
#include "treeview.h"
#include "groupedview.h"
#include "song.h"

class QAbstractItemModel;
class QAction;
class QItemSelectionModel;
class QHeaderView;
class QModelIndex;
class QMenu;
class Spinner;
class PlayQueueView;
class MessageOverlay;

class PlayQueueTreeView : public TreeView
{
    Q_OBJECT

public:
    PlayQueueTreeView(PlayQueueView *p);
    virtual ~PlayQueueTreeView();

    void initHeader();
    void saveHeader();

private Q_SLOTS:
    void showMenu();
    void toggleHeaderItem(bool visible);
    void paintEvent(QPaintEvent *e);

private:
    PlayQueueView *view;
    QMenu *menu;
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
    PlayQueueView(QWidget *parent=0);
    virtual ~PlayQueueView();

    void initHeader() { treeView->initHeader(); }
    void saveHeader();
    void setGrouped(bool g);
    bool isGrouped() const { return currentWidget()==(QWidget *)groupedView; }
    void setAutoExpand(bool ae);
    bool isAutoExpand() const;
    void setStartClosed(bool sc);
    bool isStartClosed() const;
    void setFilterActive(bool f);
    void updateRows(qint32 row, quint16 curAlbum, bool scroll);
    void scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint);
    void setModel(QAbstractItemModel *m) { view()->setModel(m); }
    void addAction(QAction *a);
    void setFocus();
    bool hasFocus();
    QAbstractItemModel * model() { return view()->model(); }
    void setContextMenuPolicy(Qt::ContextMenuPolicy policy);
    bool haveSelectedItems();
    bool haveUnSelectedItems();
    QItemSelectionModel * selectionModel() const { return view()->selectionModel(); }
    void setCurrentIndex(const QModelIndex &idx) { view()->setCurrentIndex(idx); }
    QHeaderView * header();
    QAbstractItemView * tree() const;
    QAbstractItemView * list() const;
    QAbstractItemView * view() const;
    bool hasFocus() const;
    QModelIndexList selectedIndexes() const;
    QList<Song> selectedSongs() const;
    float fade() { return fadeValue; }
    void setFade(float value);
    void setUseCoverAsBackgrond(bool u);
    bool useCoverAsBackground() const { return useCoverAsBgnd; }
    void updatePalette();

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
    PlayQueueGroupedView *groupedView;
    PlayQueueTreeView *treeView;
    Spinner *spinner;
    MessageOverlay *msgOverlay;

    bool useCoverAsBgnd;
    QPropertyAnimation animator;
    QImage curentCover;
    QPixmap curentBackground;
    QPixmap previousBackground;
    QSize lastBgndSize;
    double fadeValue;
    friend class PlayQueueGroupedView;
    friend class PlayQueueTreeView;
};

#endif
