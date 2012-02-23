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

#include "playqueueview.h"
#include "playqueuemodel.h"
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QApplication>
#include <QtGui/QFontMetrics>
#include <QtCore/QDebug>

static const int constIconSize=16;
static const int constBorder=1;

class PlayQueueDelegate : public QStyledItemDelegate
{
public:
    enum Type {
        AlbumHeader,
        AlbumTrack,
        SingleTrack
    };

    PlayQueueDelegate(QObject *p)
        : QStyledItemDelegate(p)
    {
    }

    virtual ~PlayQueueDelegate()
    {
    }

    Type getType(const QModelIndex &index) const
    {
        QModelIndex prev=index.row()>0 ? index.sibling(index.row()-1, 0) : QModelIndex();
        unsigned long thisKey=index.data(PlayQueueView::Role_Key).toUInt();
        unsigned long prevKey=prev.isValid() ? prev.data(PlayQueueView::Role_Key).toUInt() : 0xFFFFFFFF;

        if (thisKey==prevKey) {
            // Album Track
            return AlbumTrack;
        }
        QModelIndex next=index.sibling(index.row()+1, 0);
        unsigned long nextKey=next.isValid() ? next.data(PlayQueueView::Role_Key).toUInt() : 0xFFFFFFFF;
        if (thisKey==nextKey) {
            // Album header...
            return AlbumHeader;
        }
        // Single track
        return SingleTrack;
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QSize sz=QStyledItemDelegate::sizeHint(option, index);

        if (0==index.column()) {
            if (AlbumHeader==getType(index)) {
                int textHeight = QApplication::fontMetrics().height()*2;
                return QSize(sz.width(), textHeight+(2*constBorder));
            }
            return QSize(sz.width(), qMax(sz.height(), constIconSize+(2*constBorder)));
        }
        return sz;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);

        Type type=getType(index);
    }
};

PlayQueueView::PlayQueueView(QWidget *parent)
        : TreeView(parent)
        , grouped(true)
        , prev(0)
        , custom(0)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropOverwriteMode(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setGrouped(false);
}

PlayQueueView::~PlayQueueView()
{
}

void PlayQueueView::setGrouped(bool g)
{
    if (g==grouped) {
        return;
    }
    grouped=g;

    if (grouped) {
        if (!custom) {
            custom=new PlayQueueDelegate(this);
        }
        if (!prev) {
            prev=itemDelegate();
        }
        setItemDelegate(custom);
    } else if (prev) {
        setItemDelegate(prev);
    }

    setUniformRowHeights(!grouped);
}

