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

#include "treeview.h"
#include "models/roles.h"
#include "icons.h"
#include "config.h"
#include "basicitemdelegate.h"
#include "support/utils.h"
#include "mpd-interface/song.h"
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QDrag>
#include <QMimeData>
#include <QMap>
#include <QStyle>
#include <QList>
#include <QApplication>
#include <QHeaderView>
#include <algorithm>

#define SINGLE_CLICK style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, 0, this)

QImage TreeView::setOpacity(const QImage &orig, double opacity)
{
    QImage img=QImage::Format_ARGB32==orig.format() ? orig : orig.convertToFormat(QImage::Format_ARGB32);
    uchar *bits = img.bits();
    for (int i = 0; i < img.height()*img.bytesPerLine(); i+=4) {
        if (0!=bits[i+3]) {
            bits[i+3]*=opacity;
        }
    }
    return img;
}

QPixmap TreeView::createBgndPixmap(const QIcon &icon)
{
    if (icon.isNull()) {
        return QPixmap();
    }
    static int bgndSize=0;
    if (0==bgndSize) {
        bgndSize=QApplication::fontMetrics().height()*16;
    }

    QImage img=icon.pixmap(bgndSize, bgndSize).toImage();
    if (img.width()!=bgndSize && img.height()!=bgndSize) {
        img=img.scaled(bgndSize, bgndSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return QPixmap::fromImage(setOpacity(img, 0.075));
}

static bool forceSingleClick=true;

void TreeView::setForceSingleClick(bool v)
{
    forceSingleClick=v;
}

bool TreeView::getForceSingleClick()
{
    return forceSingleClick;
}

TreeView::TreeView(QWidget *parent, bool menuAlwaysAllowed)
    : QTreeView(parent)
    , eventFilter(nullptr)
    , forceSingleColumn(false)
    , alwaysAllowMenu(menuAlwaysAllowed)
{
    setDragEnabled(true);
    setContextMenuPolicy(Qt::NoContextMenu);
//     setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
    setAlternatingRowColors(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Treeview does not seem to need WA_MouseTracking set, even with QGtkStyle items still
    // highlight under mouse. And enabling WA_MouseTracking here seems to cause drag-n-drop
    // errors if an item is dragged onto playqueue whilst playqueue has a selected item!
    // BUG:145
    //setAttribute(Qt::WA_MouseTracking);

    if (SINGLE_CLICK) {
        connect(this, SIGNAL(activated(const QModelIndex &)), this, SIGNAL(itemActivated(const QModelIndex &)));
    }
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
    forceSingleColumn=true;
}

void TreeView::setExpandOnClick()
{
    connect(this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(itemWasClicked(const QModelIndex &)));
}

void TreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selected, deselected);
    bool haveSelection=haveSelectedItems();

    if (!alwaysAllowMenu) {
        setContextMenuPolicy(haveSelection ? Qt::ActionsContextMenu : Qt::NoContextMenu);
    }
    emit itemsSelected(haveSelection);
}

bool TreeView::haveSelectedItems() const
{
    // Dont need the sorted type of 'selectedIndexes' here...
    return selectionModel() && selectionModel()->selectedIndexes().count()>0;
}

bool TreeView::haveUnSelectedItems() const
{
    // Dont need the sorted type of 'selectedIndexes' here...
    return selectionModel() && model() && selectionModel()->selectedIndexes().count()!=model()->rowCount();
}

void TreeView::drag(Qt::DropActions supportedActions, QAbstractItemView *view, const QModelIndexList &items)
{
    if (items.count() > 0) {
        QMimeData *data = view->model()->mimeData(items);
        if (!data) {
            return;
        }
        QDrag *drag = new QDrag(view);
        drag->setMimeData(data);
        int pixSize=Icon::stdSize(Utils::scaleForDpi(32));
        drag->setPixmap(Icons::self()->audioListIcon.pixmap(pixSize, pixSize));
        drag->exec(supportedActions);
    }
}

void TreeView::mouseReleaseEvent(QMouseEvent *event)
{
    if (Qt::NoModifier==event->modifiers() && Qt::LeftButton==event->button()) {
        QTreeView::mouseReleaseEvent(event);
    }
}

QModelIndexList TreeView::selectedIndexes(bool sorted) const
{
    if (!selectionModel()) {
        return QModelIndexList();
    }

    if (sorted) {
        return sortIndexes(selectionModel()->selectedIndexes());
    } else if (model() && model()->columnCount()>1) {
        QModelIndexList list=selectionModel()->selectedIndexes();
        QModelIndexList sel;
        for (const QModelIndex &idx: list) {
            if (0==idx.column()) {
                sel.append(idx);
            }
        }
        return sel;
    }
    return selectionModel()->selectedIndexes();
}

struct Index : public QModelIndex
{
    Index(const QModelIndex &i)
        : QModelIndex(i)
    {
        QModelIndex idx=i;
        while (idx.isValid()) {
            rows.prepend(idx.row());
            idx=idx.parent();
        }
        count=rows.count();
    }

    bool operator<(const Index &rhs) const
    {
        int toCompare=qMax(count, rhs.count);
        for (int i=0; i<toCompare; ++i) {
            qint32 left=i<count ? rows.at(i) : -1;
            qint32 right=i<rhs.count ? rhs.rows.at(i) : -1;
            if (left<right) {
                return true;
            } else if (left>right) {
                return false;
            }
        }
        return false;
    }

    QList<qint32> rows;
    int count;
};

QModelIndexList TreeView::sortIndexes(const QModelIndexList &list)
{
    if (list.isEmpty()) {
        return list;
    }

    // QModelIndex::operator< sorts on row first - but this messes things up if rows
    // have different parents. Therefore, we use the sort above - so that the hierarchy is preserved.
    // First, create the list of 'Index' items to be sorted...
    QList<Index> toSort;
    for (const QModelIndex &i: list) {
        if (0==i.column()) {
            toSort.append(Index(i));
        }
    }
    // Call std::sort on these - this will use operator<
    std::sort(toSort.begin(), toSort.end());

    // Now convert the QList<Index> into a QModelIndexList
    QModelIndexList sorted;
    for (const Index &i: toSort) {
        sorted.append(i);
    }
    return sorted;
}

void TreeView::expandAll(const QModelIndex &idx, bool singleLevelOnly)
{
    quint32 count=model()->rowCount(idx);
    for (quint32 i=0; i<count; ++i) {
        expand(model()->index(i, 0, idx), singleLevelOnly);
    }
}

void TreeView::collapseToLevel(int level, const QModelIndex &idx)
{
    quint32 count=model()->rowCount(idx);
    if (level) {
        for (quint32 i=0; i<count; ++i) {
            collapseToLevel(level-1, model()->index(i, 0, idx));
        }
    } else {
        for (quint32 i=0; i<count; ++i) {
            collapse(model()->index(i, 0, idx));
        }
    }
}

void TreeView::expand(const QModelIndex &idx, bool singleOnly)
{
    if (idx.isValid()) {
        setExpanded(idx, true);
        if (!singleOnly) {
            quint32 count=model()->rowCount(idx);
            for (quint32 i=0; i<count; ++i) {
                expand(model()->index(i, 0, idx));
            }
        }
    }
}

void TreeView::collapse(const QModelIndex &idx, bool singleOnly)
{
    if (idx.isValid()) {
        setExpanded(idx, false);
        if (!singleOnly) {
            quint32 count=model()->rowCount(idx);
            for (quint32 i=0; i<count; ++i) {
                collapse(model()->index(i, 0, idx));
            }
        }
    }
}

bool TreeView::checkBoxClicked(const QModelIndex &idx) const
{
    QRect rect = visualRect(idx);
    rect.moveTo(viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
    int itemIndentation = rect.x() - visualRect(rootIndex()).x();
    rect = QRect(header()->sectionViewportPosition(0) + itemIndentation, rect.y(), style()->pixelMetric(QStyle::PM_IndicatorWidth), rect.height());
    return rect.contains(QCursor::pos());
}

void TreeView::setUseSimpleDelegate()
{
    setItemDelegate(new BasicItemDelegate(this));
}

void TreeView::setBackgroundImage(const QIcon &icon)
{
    QPalette pal=parentWidget()->palette();
//    if (!icon.isNull()) {
//        pal.setColor(QPalette::Base, Qt::transparent);
//    }
    #ifndef Q_OS_MAC
    setPalette(pal);
    #endif
    viewport()->setPalette(pal);
    bgnd=createBgndPixmap(icon);
}

void TreeView::paintEvent(QPaintEvent *e)
{
    if (!bgnd.isNull()) {
        QPainter p(viewport());
        QSize sz=size();
        p.fillRect(0, 0, sz.width(), sz.height(), QApplication::palette().color(QPalette::Base));
        p.drawPixmap((sz.width()-bgnd.width())/2, (sz.height()-bgnd.height())/2, bgnd);
    }
    if (!info.isEmpty() && model() && 0==model()->rowCount()) {
        QPainter p(viewport());
        QColor col(palette().text().color());
        col.setAlphaF(0.5);
        QFont f(font());
        f.setItalic(true);
        p.setPen(col);
        p.setFont(f);
        p.drawText(rect().adjusted(8, 8, -16, -16), Qt::AlignCenter|Qt::TextWordWrap, info);
    }
    QTreeView::paintEvent(e);
}

void TreeView::setModel(QAbstractItemModel *m)
{
    QAbstractItemModel *old=model();
    QTreeView::setModel(m);

    if (forceSingleColumn && m) {
        int columnCount=m->columnCount();
        if (columnCount>1) {
            QHeaderView *hdr=header();
            for (int i=1; i<columnCount; ++i) {
                hdr->setSectionHidden(i, true);
            }
        }
    }

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
    if (!selectionModel()) {
        return;
    }

    QItemSelection s = selectionModel()->selection();
    setCurrentIndex(currentIndex());
    selectionModel()->select(s, QItemSelectionModel::SelectCurrent);
}

//void TreeView::itemWasActivated(const QModelIndex &index)
//{
//    if (!forceSingleClick) {
//        setExpanded(index, !isExpanded(index));
//    }
//}

void TreeView::itemWasClicked(const QModelIndex &index)
{
    if (forceSingleClick) {
        setExpanded(index, !isExpanded(index));
    }
}

#include "moc_treeview.cpp"
