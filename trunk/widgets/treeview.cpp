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
#include <QStyle>
#include <QStyledItemDelegate>

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
#endif

#ifdef ENABLE_KDE_SUPPORT
#define SINGLE_CLICK KGlobalSettings::singleClick()
#else
#define SINGLE_CLICK style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, 0, this)
#endif


QImage TreeView::setOpacity(const QImage &orig, double opacity)
{
    QImage img=QImage::Format_ARGB32==orig.format() ? orig : orig.convertToFormat(QImage::Format_ARGB32);
    uchar *bits = img.bits();
    for (int i = 0; i < img.height()*img.bytesPerLine(); i+=4) {
        if (0!=bits[i+3]) {
            bits[i+3] = opacity*255;
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

static void drawLine(QPainter *p, const QRect &r, const QColor &color, bool fadeStart, bool fadeEnd)
{
    static const double constAlpha=0.1;
    QColor col(color);
    QLinearGradient grad(r.bottomLeft(), r.bottomRight());

    if (fadeStart || fadeEnd) {
        double fadeSize=(fadeStart && fadeEnd ? 64.0 : 32.0);
        if (r.width()<(2.2*fadeSize)) {
            fadeSize=r.width()/3.0;
        }
        double fadePos=fadeSize/r.width();
        col.setAlphaF(fadeStart ? 0.0 : constAlpha);
        grad.setColorAt(0, col);
        col.setAlphaF(constAlpha);
        grad.setColorAt(fadePos, col);
        grad.setColorAt(1.0-fadePos, col);
        col.setAlphaF(fadeEnd ? 0.0 : constAlpha);
        grad.setColorAt(1, col);
        p->setPen(QPen(grad, 1));
    } else {
        col.setAlphaF(0.1);
        p->setPen(QPen(col, 1));
    }
    p->drawLine(r.bottomLeft(), r.bottomRight());
}

SimpleTreeViewDelegate::SimpleTreeViewDelegate(QObject *p)
    : QStyledItemDelegate(p)
{
}

SimpleTreeViewDelegate::~SimpleTreeViewDelegate()
{
}

void SimpleTreeViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return;
    }

    bool selected=option.state&QStyle::State_Selected;
    bool active=option.state&QStyle::State_Active;
    QStyledItemDelegate::paint(painter, option, index);
    QColor col(option.palette.color(active ? QPalette::Active : QPalette::Inactive,
                                    selected ? QPalette::HighlightedText : QPalette::Text));

    col.setAlphaF(0.1);
    painter->setPen(QPen(col, 1));
    if (4==option.version) {
        const QStyleOptionViewItemV4 &v4=(QStyleOptionViewItemV4 &)option;

        switch (v4.viewItemPosition) {
        case QStyleOptionViewItemV4::Beginning:
            drawLine(painter, option.rect, col, true, false);
            break;
        case QStyleOptionViewItemV4::Middle:
            drawLine(painter, option.rect, col, false, false);
            break;
        case QStyleOptionViewItemV4::End:
            drawLine(painter, option.rect, col, false, true);
            break;
        case QStyleOptionViewItemV4::Invalid:
        case QStyleOptionViewItemV4::OnlyOne:
            drawLine(painter, option.rect, col, true, true);
        }
    } else {
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    }
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
        , alwaysAllowMenu(menuAlwaysAllowed)
{
    setDragEnabled(true);
    setContextMenuPolicy(Qt::NoContextMenu);
//     setRootIsDecorated(false);
    setAllColumnsShowFocus(true);
    setAlternatingRowColors(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
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
}

void TreeView::setExpandOnClick()
{
    connect(this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(itemWasClicked(const QModelIndex &)));
}

void TreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selected, deselected);
    bool haveSelection=selectedIndexes().count();

    if (!alwaysAllowMenu) {
        setContextMenuPolicy(haveSelection ? Qt::ActionsContextMenu : Qt::NoContextMenu);
    }
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
            drag->setPixmap(Icons::self()->albumIcon.pixmap(32, 32));
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

void TreeView::expandAll(const QModelIndex &idx)
{
    quint32 count=model()->rowCount(idx);
    for (quint32 i=0; i<count; ++i) {
        expand(model()->index(i, 0, idx));
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
    setItemDelegate(new SimpleTreeViewDelegate(this));
}

void TreeView::setBackgroundImage(const QIcon &icon)
{
    QPalette pal=parentWidget()->palette();
    if (!icon.isNull()) {
        pal.setColor(QPalette::Base, Qt::transparent);
    }
    setPalette(pal);
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
    QTreeView::paintEvent(e);
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

void TreeView::itemWasActivated(const QModelIndex &index)
{
    if (!forceSingleClick) {
        setExpanded(index, !isExpanded(index));
    }
}

void TreeView::itemWasClicked(const QModelIndex &index)
{
    if (forceSingleClick) {
        setExpanded(index, !isExpanded(index));
    }
}
