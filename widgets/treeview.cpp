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

#include "treeview.h"
#include "itemview.h"
#include "icons.h"
#include "config.h"
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QDrag>
#include <QMimeData>
#include <QMap>

TreeView::TreeView(QWidget *parent)
        : QTreeView(parent)
{
    setDragEnabled(true);
    setContextMenuPolicy(Qt::NoContextMenu);
//     setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    // Treeview does not seem to need WA_MouseTracking set, even with QGtkStyle items still
    // highlight under mouse. And enabling WA_MouseTracking here seems to cause drag-n-drop
    // errors if an item is dragged onto playqueue whilst playqueue has a selected item!
    // BUG:145
    //setAttribute(Qt::WA_MouseTracking);
}

TreeView::~TreeView()
{
}

void TreeView::setPageDefaults()
{
    sortByColumn(0, Qt::AscendingOrder);
    setHeaderHidden(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSortingEnabled(true);
    setAnimated(true);
}

void TreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selected, deselected);
    bool haveSelection=selectedIndexes().count();

    setContextMenuPolicy(haveSelection ? Qt::ActionsContextMenu : Qt::NoContextMenu);
    emit itemsSelected(haveSelection);
}

bool TreeView::haveSelectedItems() const
{
    return selectedIndexes().count()>0;
}

bool TreeView::haveUnSelectedItems() const
{
    return selectedIndexes().count()!=model()->rowCount();
}

void TreeView::startDrag(Qt::DropActions supportedActions)
{
    QModelIndexList indexes = selectedIndexes();
    if (indexes.count() > 0) {
        qSort(indexes);

        QMimeData *data = model()->mimeData(indexes);
        if (!data) {
            return;
        }
        QDrag *drag = new QDrag(this);
        drag->setMimeData(data);
        QPixmap pix;

        if (1==indexes.count()) {
            QVariant var=model()->data(indexes.first(), ItemView::Role_Image);
            QImage img=var.value<QImage>();
            if (img.isNull()) {
                pix=var.value<QPixmap>();
            } else {
                pix=QPixmap::fromImage(img);
            }
        }
        if (pix.isNull()) {
            drag->setPixmap(Icons::albumIcon.pixmap(32, 32));
        } else {
            drag->setPixmap(pix.width()<32 ? pix : pix.scaled(QSize(32, 32), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        drag->start(supportedActions);
    }
}

void TreeView::mouseReleaseEvent(QMouseEvent *event)
{
    if (Qt::NoModifier==event->modifiers() && Qt::LeftButton==event->button()) {
        QTreeView::mouseReleaseEvent(event);
    }
}

QModelIndexList TreeView::selectedIndexes() const
{
    QModelIndexList indexes=selectionModel()->selectedIndexes();
    QMap<int, QModelIndex> sort;

    foreach (const QModelIndex &idx, indexes) {
        sort.insertMulti(visualRect(idx).y(), idx);
    }
    return sort.values();
}

void TreeView::expandAll()
{
    quint32 count=model()->rowCount();
    for (quint32 i=0; i<count; ++i) {
        expand(model()->index(i, 0));
    }
}

void TreeView::expand(const QModelIndex &idx)
{
    if (idx.isValid()) {
        setExpanded(idx, true);
        quint32 count=model()->rowCount(idx);
        for (quint32 i=0; i<count; ++i) {
            expand(idx.child(i, 0));
        }
    }
}

void TreeView::setModel(QAbstractItemModel *m)
{
    QAbstractItemModel *old=model();
    QTreeView::setModel(m);

    if (old) {
        disconnect(old, SIGNAL(layoutChanged()), this, SLOT(correctSelection()));
    }

    if (m && old!=m) {
        connect(m, SIGNAL(layoutChanged()), this, SLOT(correctSelection()));
    }
}

// Workaround for https://bugreports.qt-project.org/browse/QTBUG-18009
void TreeView::correctSelection()
{
    QItemSelection s = selectionModel()->selection();
    setCurrentIndex(currentIndex());
    selectionModel()->select(s, QItemSelectionModel::SelectCurrent);
}
