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

#ifndef GROUPEDVIEW_H
#define GROUPEDVIEW_H

#include <QSet>
#include "treeview.h"

class Song;

class GroupedView : public TreeView
{
    Q_OBJECT

public:
    enum Status {
        State_Default,
        State_Playing,
        State_Paused,
        State_Stopped
    };

    enum Roles {
        Role_Key = Qt::UserRole+512,
        Role_Id,
        Role_Song,
        Role_AlbumDuration,
        Role_Status,
        Role_CurrentStatus,
        Role_SongCount,

        Role_IsCollection,
        Role_CollectionId,
        Role_DropAdjust
    };

    static void setup();

    GroupedView(QWidget *parent=0);
    virtual ~GroupedView();

    void setModel(QAbstractItemModel *model);
    void setFilterActive(bool f);
    bool isFilterActive() const { return filterActive; }
    void setAutoExpand(bool ae) { autoExpand=ae; }
    bool isAutoExpand() const { return autoExpand; }
    void setStartClosed(bool sc);
    bool isStartClosed() const { return startClosed; }
    void setMultiLevel(bool ml) { isMultiLevel=ml; }
    void updateRows(qint32 row, quint16 curAlbum, bool scroll, const QModelIndex &parent=QModelIndex());
    void updateCollectionRows();
    bool isCurrentAlbum(quint16 key) const { return key==currentAlbum; }
    bool isExpanded(quint16 key, quint32 collection) const { return filterActive ||
                                                (autoExpand && currentAlbum==key) ||
                                                (startClosed && controlledAlbums[collection].contains(key)) ||
                                                (!startClosed && !controlledAlbums[collection].contains(key)); }
    void toggle(const QModelIndex &idx);
    QModelIndexList selectedIndexes() const;
    void dropEvent(QDropEvent *event);
    void collectionRemoved(quint32 key);
    void expandAll();
    void expand(const QModelIndex &idx);

public Q_SLOTS:
    void updateRows(const QModelIndex &parent);
    void coverRetrieved(const Song &song);

private Q_SLOTS:
    void itemClicked(const QModelIndex &index);

private:
    bool startClosed;
    bool autoExpand;
    bool filterActive;
    bool isMultiLevel;
    quint16 currentAlbum;
    QMap<quint32, QSet<quint16> > controlledAlbums;
};

#endif
