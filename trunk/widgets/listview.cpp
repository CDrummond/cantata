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

#include "listview.h"
#include "itemview.h"
#include "treeview.h"
#include "config.h"
#include "icons.h"
#include "support/utils.h"
#include "support/flickcharm.h"
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>

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
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(showCustomContextMenu(const QPoint &)));
    FlickCharm::self()->activateOn(this);
}

ListView::~ListView()
{
}

void ListView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QListView::selectionChanged(selected, deselected);
    bool haveSelection=haveSelectedItems();

    setContextMenuPolicy(haveSelection ? Qt::ActionsContextMenu : (menu ? Qt::CustomContextMenu : Qt::NoContextMenu));
    emit itemsSelected(haveSelection);
}

bool ListView::haveSelectedItems() const
{
    // Dont need the sorted type of 'selectedIndexes' here...
    return selectionModel() && selectionModel()->selectedIndexes().count()>0;
}

bool ListView::haveUnSelectedItems() const
{
    // Dont need the sorted type of 'selectedIndexes' here...
    return selectionModel() && selectionModel()->selectedIndexes().count()!=model()->rowCount();
}

void ListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (Qt::NoModifier==event->modifiers() && Qt::LeftButton==event->button()) {
        QListView::mouseReleaseEvent(event);
    }
}

QModelIndexList ListView::selectedIndexes(bool sorted) const
{
    QModelIndexList indexes=selectionModel() ? selectionModel()->selectedIndexes() : QModelIndexList();
    if (sorted) {
        qSort(indexes);
    }
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

void ListView::setBackgroundImage(const QIcon &icon)
{
    QPalette pal=parentWidget()->palette();
    if (!icon.isNull()) {
        pal.setColor(QPalette::Base, Qt::transparent);
    }
    setPalette(pal);
    viewport()->setPalette(pal);
    bgnd=TreeView::createBgndPixmap(icon);
}

void ListView::paintEvent(QPaintEvent *e)
{
    if (!bgnd.isNull()) {
        QPainter p(viewport());
        QSize sz=size();
        p.fillRect(0, 0, sz.width(), sz.height(), QApplication::palette().color(QPalette::Base));
        p.drawPixmap((sz.width()-bgnd.width())/2, (sz.height()-bgnd.height())/2, bgnd);
    }
    QListView::paintEvent(e);
}

// Workaround for https://bugreports.qt-project.org/browse/QTBUG-18009
void ListView::correctSelection()
{
    if (!selectionModel()) {
        return;
    }

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
