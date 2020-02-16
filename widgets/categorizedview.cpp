/*
 * Cantata
 *
 * Copyright (c) 2018-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "categorizedview.h"
#include "kcategorizedview/kcategorydrawer.h"
#include "kcategorizedview/kcategorizedsortfilterproxymodel.h"
#include "config.h"
#include "icons.h"
#include "support/utils.h"
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QModelIndex>
#include <QApplication>
#include <QLinearGradient>
#include <algorithm>

class CategoryDrawer : public KCategoryDrawer
{
public:
    CategoryDrawer(KCategorizedView *view)
        : KCategoryDrawer(view) {
    }

    ~CategoryDrawer() override {
    }

    void drawCategory(const QModelIndex &index, int /*sortRole*/, const QStyleOption &option, QPainter *painter) const override
    {
        const QString category = index.model()->data(index, KCategorizedSortFilterProxyModel::CategoryDisplayRole).toString();
        QFont font(QApplication::font());
        font.setBold(true);
        const QFontMetrics fontMetrics = QFontMetrics(font);

        QRect r(option.rect);
        r.setTop(r.top() + 2);
        r.setLeft(r.left() + 2);
        r.setHeight(fontMetrics.height());
        r.setRight(r.right() - 2);

        painter->save();
        painter->setFont(font);
        QColor col(option.palette.text().color());
        painter->setPen(col);
        painter->drawText(r, Qt::AlignLeft | Qt::AlignVCenter, category);

        r.adjust(0, 4, 0, 4);

        double alpha=0.5;
        double fadeSize=64.0;
        double fadePos=fadeSize/r.width();
        QLinearGradient grad(r.bottomLeft(), r.bottomRight());

        col.setAlphaF(Qt::RightToLeft==option.direction ? 0.0 : alpha);
        grad.setColorAt(0, col);
        col.setAlphaF(alpha);
        grad.setColorAt(fadePos, col);
        grad.setColorAt(1.0-fadePos, col);
        col.setAlphaF(Qt::LeftToRight==option.direction ? 0.0 : alpha);
        grad.setColorAt(1, col);
        painter->setPen(QPen(grad, 1));

        painter->drawLine(r.bottomLeft(), r.bottomRight());
        painter->restore();
    }
};

CategorizedView::CategorizedView(QWidget *parent)
    : KCategorizedView(parent)
    , eventFilter(nullptr)
    , menu(nullptr)
    , zoomLevel(1.0)
{
    proxy=new KCategorizedSortFilterProxyModel(this);
    proxy->setCategorizedModel(true);
    setCategoryDrawer(new CategoryDrawer(this));
    setDragEnabled(true);
    setContextMenuPolicy(Qt::NoContextMenu);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAlternatingRowColors(false);
    setUniformItemSizes(true);
    setAttribute(Qt::WA_MouseTracking);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(showCustomContextMenu(const QPoint &)));
    connect(this, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(checkDoubleClick(const QModelIndex &)));
    connect(this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(checkClicked(const QModelIndex &)));
    connect(this, SIGNAL(activated(const QModelIndex &)), this, SLOT(checkActivated(const QModelIndex &)));

    setViewMode(QListView::IconMode);
    setResizeMode(QListView::Adjust);
    setWordWrap(true);
}

CategorizedView::~CategorizedView()
{
}

void CategorizedView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    //KCategorizedView::selectionChanged(selected, deselected);
    bool haveSelection=haveSelectedItems();

    setContextMenuPolicy(haveSelection ? Qt::ActionsContextMenu : (menu ? Qt::CustomContextMenu : Qt::NoContextMenu));
    emit itemsSelected(haveSelection);
}

bool CategorizedView::haveSelectedItems() const
{
    // Dont need the sorted type of 'selectedIndexes' here...
    return selectionModel() && selectionModel()->selectedIndexes().count()>0;
}

bool CategorizedView::haveUnSelectedItems() const
{
    // Dont need the sorted type of 'selectedIndexes' here...
    return selectionModel() && model() && selectionModel()->selectedIndexes().count()!=model()->rowCount();
}

void CategorizedView::mouseReleaseEvent(QMouseEvent *event)
{
    if (Qt::NoModifier==event->modifiers() && Qt::LeftButton==event->button()) {
        KCategorizedView::mouseReleaseEvent(event);
    }
}

QModelIndexList CategorizedView::selectedIndexes(bool sorted) const
{
    QModelIndexList indexes=selectionModel() ? selectionModel()->selectedIndexes() : QModelIndexList();
    QModelIndexList actual;

    for (const auto &idx: indexes) {
        actual.append(proxy->mapToSource(idx));
    }

    if (sorted) {
        std::sort(actual.begin(), actual.end());
    }
    return actual;
}

void CategorizedView::setModel(QAbstractItemModel *m)
{
    QAbstractItemModel *old=proxy->sourceModel();
    proxy->setSourceModel(m);

    if (old) {
        disconnect(old, SIGNAL(layoutChanged()), this, SLOT(correctSelection()));
    }

    if (m && old!=m) {
        connect(m, SIGNAL(layoutChanged()), this, SLOT(correctSelection()));
    }
    if (m) {
        KCategorizedView::setModel(proxy);
    } else {
        KCategorizedView::setModel(nullptr);
    }
}

void CategorizedView::addDefaultAction(QAction *act)
{
    if (!menu) {
        menu=new QMenu(this);
    }
    menu->addAction(act);
}

void CategorizedView::setBackgroundImage(const QIcon &icon)
{
    QPalette pal=parentWidget()->palette();
//    if (!icon.isNull()) {
//        pal.setColor(QPalette::Base, Qt::transparent);
//    }
    #ifndef Q_OS_MAC
    setPalette(pal);
    #endif
    viewport()->setPalette(pal);
    bgnd=TreeView::createBgndPixmap(icon);
}

void CategorizedView::paintEvent(QPaintEvent *e)
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
    KCategorizedView::paintEvent(e);
}

void CategorizedView::setRootIndex(const QModelIndex &idx)
{
    KCategorizedView::setRootIndex(idx.model()==proxy->sourceModel() ? proxy->mapFromSource(idx) : idx);
}

QModelIndex CategorizedView::rootIndex() const
{
    QModelIndex idx=KCategorizedView::rootIndex();
    return idx.model()==proxy ? proxy->mapToSource(idx) : idx;
}

QModelIndex CategorizedView::indexAt(const QPoint &point, bool ensureFromSource) const
{
    QModelIndex idx=KCategorizedView::indexAt(point);
    return ensureFromSource && idx.model()==proxy ? proxy->mapToSource(idx) : idx;
}

QModelIndex CategorizedView::mapFromSource(const QModelIndex &idx) const
{
    return idx.model()==proxy->sourceModel() ? proxy->mapFromSource(idx) : idx;
}

void CategorizedView::setPlain(bool plain)
{
    proxy->setCategorizedModel(!plain);
    setViewMode(plain ? QListView::ListMode : QListView::IconMode);
}

// Workaround for https://bugreports.qt-project.org/browse/QTBUG-18009
void CategorizedView::correctSelection()
{
    if (!selectionModel()) {
        return;
    }

    QItemSelection s = selectionModel()->selection();
    setCurrentIndex(currentIndex());
    selectionModel()->select(s, QItemSelectionModel::SelectCurrent);
}

void CategorizedView::showCustomContextMenu(const QPoint &pos)
{
    if (menu) {
        menu->popup(mapToGlobal(pos));
    }
}

void CategorizedView::checkDoubleClick(const QModelIndex &idx)
{
    if (!TreeView::getForceSingleClick() && idx.model() && idx.model()->rowCount(idx)) {
        return;
    }
    emit itemDoubleClicked(idx.model()==proxy ? proxy->mapToSource(idx) : idx);
}

void CategorizedView::checkClicked(const QModelIndex &idx)
{
    emit itemClicked(idx.model()==proxy ? proxy->mapToSource(idx) : idx);
}

void CategorizedView::checkActivated(const QModelIndex &idx)
{
    emit itemActivated(idx.model()==proxy ? proxy->mapToSource(idx) : idx);
}

void CategorizedView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    if (parent==KCategorizedView::rootIndex()) {
        KCategorizedView::rowsInserted(parent, start, end);
    }
}
