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

#ifndef GROUPEDVIEW_H
#define GROUPEDVIEW_H

#include <QSet>
#include "treeview.h"
#include "actionitemdelegate.h"

struct Song;
class RatingPainter;
class GroupedView;

class GroupedViewDelegate : public ActionItemDelegate
{
    Q_OBJECT
public:
    GroupedViewDelegate(GroupedView *p);
    ~GroupedViewDelegate() override;

    QSize sizeHint(int type, bool isCollection) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    int drawRatings(QPainter *painter, const Song &song, const QRect &r, const QFontMetrics &fm, const QColor &col) const;

private:
    GroupedView *view;
    mutable RatingPainter *ratingPainter;
};

class GroupedView : public TreeView
{
    Q_OBJECT

public:
    enum Status {
        State_Default,
        State_Playing,
        State_StopAfter,
        State_StopAfterTrack,
        State_Paused,
        State_Stopped
    };

    static void setup();
    static int coverSize();
    static int borderSize();
    static int iconSize();
    static void drawPlayState(QPainter *painter, const QStyleOptionViewItem &option, const QRect &r, int state);

    GroupedView(QWidget *parent=nullptr, bool isPlayQueue=false);
    ~GroupedView() override;

    void setModel(QAbstractItemModel *model) override;
    void setFilterActive(bool f);
    bool isFilterActive() const { return filterActive; }
    void setAutoExpand(bool ae) { autoExpand=ae; }
    bool isAutoExpand() const { return autoExpand; }
    void setStartClosed(bool sc);
    bool isStartClosed() const { return startClosed; }
    void setMultiLevel(bool ml) { isMultiLevel=ml; }
    void updateRows(qint32 row, quint16 curAlbum, bool scroll, const QModelIndex &parent=QModelIndex(), bool forceScroll=false);
    void updateCollectionRows();
    bool isCurrentAlbum(quint16 key) const { return key==currentAlbum; }
    bool isExpanded(quint16 key, quint32 collection) const { return filterActive ||
                                                (autoExpand && currentAlbum==key) ||
                                                (startClosed && controlledAlbums[collection].contains(key)) ||
                                                (!startClosed && !controlledAlbums[collection].contains(key)); }
    void toggle(const QModelIndex &idx);
    QModelIndexList selectedIndexes() const override { return selectedIndexes(true); }
    QModelIndexList selectedIndexes(bool sorted) const;
    void dropEvent(QDropEvent *event) override;
    void collectionRemoved(quint32 key);
    void expand(const QModelIndex &idx, bool singleOnly=false) override;
    void collapse(const QModelIndex &idx, bool singleOnly=false) override;

 private:
    void drawBranches(QPainter *painter, const QRect &, const QModelIndex &) const override;

public Q_SLOTS:
    void updateRows(const QModelIndex &parent);
    void coverLoaded(const Song &song, int size);

private Q_SLOTS:
    void itemClicked(const QModelIndex &index);

private:
    bool allowClose;
    bool startClosed;
    bool autoExpand;
    bool filterActive;
    bool isMultiLevel;
    quint16 currentAlbum;
    QMap<quint32, QSet<quint16> > controlledAlbums;
};

#endif
