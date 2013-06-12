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

#include "listview.h"
#include "itemview.h"
#include "config.h"
#include "icons.h"
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QMenu>

ListView::ListView(QWidget *parent)
        : QListView(parent)
        , menu(0)
{
    setDragEnabled(true);
    setContextMenuPolicy(Qt::NoContextMenu);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAlternatingRowColors(false);
    setUniformItemSizes(true);
    setAttribute(Qt::WA_MouseTracking);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(showCustomContextMenu(const QPoint &)));
}

ListView::~ListView()
{
}

void ListView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    bool haveSelection=selectedIndexes().count();

    setContextMenuPolicy(haveSelection ? Qt::ActionsContextMenu : (menu ? Qt::CustomContextMenu : Qt::NoContextMenu));
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
            drag->setPixmap(Icons::self()->albumIcon.pixmap(64, 64));
        } else {
            drag->setPixmap(pix.width()<64 ? pix : pix.scaled(QSize(64, 64), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        drag->start(supportedActions);
    }
}

void ListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (Qt::NoModifier==event->modifiers() && Qt::LeftButton==event->button()) {
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

void ListView::addDefaultAction(QAction *act)
{
    if (!menu) {
        menu=new QMenu(this);
    }
    menu->addAction(act);
}

// Workaround for https://bugreports.qt-project.org/browse/QTBUG-18009
void ListView::correctSelection()
{
    QItemSelection s = selectionModel()->selection();
    setCurrentIndex(currentIndex());
    selectionModel()->select(s, QItemSelectionModel::SelectCurrent);
}

void ListView::showCustomContextMenu(const QPoint &pos)
{
    if (menu) {
        menu->popup(mapToGlobal(pos));
    }
}
