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

#include "listview.h"
#include "config.h"
#include "icon.h"
#include <QtCore/QMimeData>
#include <QtGui/QDrag>
#include <QtGui/QMouseEvent>

ListView::ListView(QWidget *parent)
        : QListView(parent)
{
    setDragEnabled(true);
    setContextMenuPolicy(Qt::NoContextMenu);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAlternatingRowColors(true);
    setUniformItemSizes(true);
    setAttribute(Qt::WA_MouseTracking);
}

ListView::~ListView()
{
}

void ListView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    bool haveSelection=selectedIndexes().count();

    setContextMenuPolicy(haveSelection ? Qt::ActionsContextMenu : Qt::NoContextMenu);
    emit itemsSelected(haveSelection);
}

bool ListView::haveSelectedItems() const
{
    return selectedIndexes().count()>0;
}

bool ListView::haveUnSelectedItems() const
{
    return selectedIndexes().count()!=model()->rowCount();
}

void ListView::startDrag(Qt::DropActions supportedActions)
{
    if (QListView::IconMode!=viewMode()) {
        QListView::startDrag(supportedActions);
        return;
    }

    QModelIndexList indexes = selectedIndexes();
    if (indexes.count() > 0) {
        QMimeData *data = model()->mimeData(indexes);
        if (!data)
            return;
        QRect rect;
        QDrag *drag = new QDrag(this);
        drag->setMimeData(data);
        if (indexes.count()>1) {
            drag->setPixmap(Icon(DEFAULT_ALBUM_ICON).pixmap(64, 64));
        } else {
            drag->setPixmap(QPixmap::fromImage(model()->data(indexes.first(), Qt::DecorationRole).value<QImage>()
                            .scaled(QSize(64, 64), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
        drag->start(supportedActions);
    }
}

void ListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (Qt::NoModifier==event->modifiers()) {
        QListView::mouseReleaseEvent(event);
    }
}

QModelIndexList ListView::selectedIndexes() const
{
    QModelIndexList indexes=selectionModel()->selectedIndexes();
    qSort(indexes);
    return indexes;
}

void ListView::setModel(QAbstractItemModel *m)
{
    QAbstractItemModel *old=model();
    QListView::setModel(m);

    if (old) {
        disconnect(old, SIGNAL(layoutChanged()), this, SLOT(correctSelection()));
    }

    if (m && old!=m) {
        connect(m, SIGNAL(layoutChanged()), this, SLOT(correctSelection()));
    }
}

// Workaround for https://bugreports.qt-project.org/browse/QTBUG-18009
void ListView::correctSelection()
{
    QItemSelection s = selectionModel()->selection();
    setCurrentIndex(currentIndex());
    selectionModel()->select(s, QItemSelectionModel::SelectCurrent);
}
