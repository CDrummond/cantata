/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "itemview.h"
#include "covers.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QStyle>
#include <QtGui/QStyleOptionViewItem>
#include <QtGui/QPainter>
#include <QtGui/QAction>
#include <QtGui/QRadialGradient>

static void drawGlow(QPainter *painter, const QRect &rOrig)
{
    QRect r=rOrig.adjusted(-2, -2, 2, 2);
    QRadialGradient gradient(QPointF(r.x()+r.width()/2.0, r.y()+r.height()/2.0), r.width()/2.0, QPointF(r.x()+r.width()/2.0, r.y()+r.height()/2.0));
    QColor c(Qt::white);
    c.setAlphaF(0.65);
    gradient.setColorAt(0, c);
    gradient.setColorAt(0.5, c);
    c.setAlphaF(0.0);
    gradient.setColorAt(1.0, c);
    painter->fillRect(r, gradient);
}

class ItemDelegate : public QStyledItemDelegate
{
public:
    static const int constBorder = 1;
    static const int constActionBorder = 2;
    static const int constActionIconSize=16;

    ItemDelegate(QObject *p, QAction *a1, QAction *a2)
        : QStyledItemDelegate(p)
        , act1(a1)
        , act2(a2)
    {
    }

    virtual ~ItemDelegate()
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QSize sz(QStyledItemDelegate::sizeHint(option, index));
        int imageSize = index.data(ItemView::Role_IconSize).toInt();
        bool oneLine = index.data(ItemView::Role_SubText).toString().isEmpty();
        int textHeight = QApplication::fontMetrics().height()*(oneLine ? 1 : 2);

        return QSize(qMax(64, imageSize) + (constBorder * 2),
                     qMax(qMax(textHeight, sz.height()), imageSize)  + (constBorder*2));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);

        QString text = index.data(Qt::DisplayRole).toString();
        QRect r(option.rect);
        QString childText = index.data(ItemView::Role_SubText).toString();
        QPixmap pix = index.data(ItemView::Role_Pixmap).value<QPixmap>();
        bool oneLine = childText.isEmpty();

        r.adjust(constBorder, 0, -constBorder, 0);
        if (!pix.isNull()) {
            painter->drawPixmap(r.x(), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
            r.adjust(pix.width()+3, 0, -3, 0);
        }
        QFont textFont(QApplication::font());
        QFontMetrics textMetrics(textFont);
        QRect textRect;
        int textHeight=textMetrics.height();
        bool selected=option.state&QStyle::State_Selected;
        QColor color(QApplication::palette().color(selected ? QPalette::HighlightedText : QPalette::Text));

        if (oneLine) {
            textRect=QRect(r.x(), r.y()+((r.height()-textHeight)/2), r.width(), textHeight);
            text = textMetrics.elidedText(text, Qt::ElideRight, textRect.width(), QPalette::WindowText);
            painter->setPen(color);
            painter->setFont(textFont);
            painter->drawText(textRect, text);
        } else {
            QFont childFont(QApplication::font());
            childFont.setPointSizeF(childFont.pointSizeF()*0.8);
            QFontMetrics childMetrics(childFont);
            QRect childRect;

            int childHeight=childMetrics.height();
            int totalHeight=textHeight+childHeight;
            textRect=QRect(r.x(), r.y()+((r.height()-totalHeight)/2), r.width(), textHeight);
            childRect=QRect(r.x(), r.y()+textHeight+((r.height()-totalHeight)/2), r.width(), textHeight);

            text = textMetrics.elidedText(text, Qt::ElideRight, textRect.width(), QPalette::WindowText);
            painter->setPen(color);
            painter->setFont(textFont);
            painter->drawText(textRect, text);

            childText = childMetrics.elidedText(childText, Qt::ElideRight, childRect.width(), QPalette::WindowText);
            color.setAlphaF(selected ? 0.65 : 0.5);
            painter->setPen(color);
            painter->setFont(childFont);
            painter->drawText(childRect, childText);
        }

        if (option.state & QStyle::State_MouseOver) {
            if (act1 && r.width()>(constActionIconSize+(2*constActionBorder))) {
                pix=act1->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
                if (!pix.isNull()) {
                    QRect ir(r.x()+r.width()-(pix.width()+constActionBorder),
                             r.y()+((r.height()-pix.height())/2),
                             pix.width(), pix.height());
                    drawGlow(painter, ir);
                    painter->drawPixmap(ir, pix);
                    r.adjust(0, 0, -(pix.width()+constActionBorder), 0);
                }
            }

            if (act2 && r.width()>(constActionIconSize+(2*constActionBorder))) {
                pix=act2->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
                if (!pix.isNull()) {
                    QRect ir(r.x()+r.width()-(pix.width()+constActionBorder),
                             r.y()+((r.height()-pix.height())/2),
                             pix.width(), pix.height());
                    drawGlow(painter, ir);
                    painter->drawPixmap(ir, pix);
                }
            }
        }
    }

    QAction *act1;
    QAction *act2;
};

ItemView::ItemView(QWidget *p)
    : QWidget(p)
    , itemModel(0)
    , act1(0)
    , act2(0)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    backAction = new QAction(i18n("Back"), this);
    #else
    backAction = new QAction(tr("Back"), this);
    #endif
    backAction->setIcon(QIcon::fromTheme("go-previous"));
    backButton->setDefaultAction(backAction);
    backButton->setAutoRaise(true);
    treeView->setPageDefaults();
}

ItemView::~ItemView()
{
}

void ItemView::init(QAction *a1, QAction *a2)
{
    if (act1 || act2) {
        return;
    }

    act1=a1;
    act2=a2;
    listView->setItemDelegate(new ItemDelegate(this, a1, a2));
    connect(treeSearch, SIGNAL(returnPressed()), this, SIGNAL(searchItems()));
    connect(treeSearch, SIGNAL(textChanged(const QString)), this, SIGNAL(searchItems()));
    connect(listSearch, SIGNAL(returnPressed()), this, SIGNAL(searchItems()));
    connect(listSearch, SIGNAL(textChanged(const QString)), this, SIGNAL(searchItems()));
    connect(treeView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
    connect(treeView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
    connect(listView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
    connect(listView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(listView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
    connect(listView, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(backAction, SIGNAL(triggered(bool)), this, SLOT(backActivated()));
    usingTreeView=false;
    setView(true);
}

void ItemView::addAction(QAction *act)
{
    treeView->addAction(act);
    listView->addAction(act);
}

void ItemView::setView(bool tree)
{
    if (tree==usingTreeView) {
        return;
    }

    usingTreeView=tree;
    treeSearch->setText(QString());
    listSearch->setText(QString());
    if (tree) {
        treeView->setModel(itemModel);
        listView->setModel(0);
    } else {
        treeView->setModel(0);
        listView->setModel(itemModel);
        setLevel(0);
        listView->setRootIndex(QModelIndex());
    }
    stackedWidget->setCurrentIndex(tree ? 0 : 1);
}

void ItemView::setLevel(int l)
{
    int prev=currentLevel;
    currentLevel=l;
    backAction->setEnabled(0!=l);

    if (view()->selectionModel()) {
        view()->selectionModel()->clearSelection();
    }
    #ifdef ENABLE_KDE_SUPPORT
    if (0==currentLevel) {
        listSearch->setPlaceholderText(i18n("Search %1...", topText));
    } else if (prev>currentLevel) {
        listSearch->setPlaceholderText(prevText);
    } else {
        prevText=listSearch->placeholderText();
    }
    #else
    if (0==currentLevel) {
        listSearch->setPlaceholderText(tr("Search %1...").arg(topText));
    } else if (prev>currentLevel) {
        listSearch->setPlaceholderText(prevText);
    } else {
        prevText=listSearch->placeholderText();
    }
    #endif
}

QAbstractItemView * ItemView::view() const
{
    return usingTreeView ? (QAbstractItemView *)treeView : (QAbstractItemView *)listView;
}

void ItemView::setModel(QAbstractItemModel *m)
{
    itemModel=m;
    view()->setModel(m);
}

QString ItemView::searchText() const
{
    return usingTreeView ? treeSearch->text() : listSearch->text();
}

void ItemView::setTopText(const QString &text)
{
    topText=text;
    if (0==currentLevel) {
        #ifdef ENABLE_KDE_SUPPORT
        listSearch->setPlaceholderText(i18n("Search %1...", topText));
        #else
        listSearch->setPlaceholderText(tr("Search %1...").arg(topText));
        #endif
    }
    #ifdef ENABLE_KDE_SUPPORT
    treeSearch->setPlaceholderText(i18n("Search %1...", topText));
    #else
    treeSearch->setPlaceholderText(tr("Search %1...").arg(topText));
    #endif
}

void ItemView::setUniformRowHeights(bool v)
{
    treeView->setUniformRowHeights(v);
}

void ItemView::setAcceptDrops(bool v)
{
    listView->setAcceptDrops(v);
    treeView->setAcceptDrops(v);
}

void ItemView::setDragDropOverwriteMode(bool v)
{
    listView->setDragDropOverwriteMode(v);
    treeView->setDragDropOverwriteMode(v);
}

void ItemView::setDragDropMode(QAbstractItemView::DragDropMode v)
{
    listView->setDragDropMode(v);
    treeView->setDragDropMode(v);
}

void ItemView::backActivated()
{
    if (usingTreeView) {
        return;
    }
    setLevel(currentLevel-1);
    listView->setRootIndex(listView->rootIndex().parent());
    listView->scrollTo(prevTopIndex, QAbstractItemView::PositionAtTop);
}

QAction * ItemView::getAction(const QModelIndex &index)
{
    QRect rect(listView->visualRect(index));
    rect.moveTo(listView->viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
    QRect actionRect(rect.x()+rect.width()-(ItemDelegate::constActionIconSize+ItemDelegate::constActionBorder),
                     rect.y()+((rect.height()-ItemDelegate::constActionIconSize)/2),
                     ItemDelegate::constActionIconSize, ItemDelegate::constActionIconSize);

    if (act1 && actionRect.contains(QCursor::pos())) {
        return act1;
    }
    actionRect.adjust(-(ItemDelegate::constActionIconSize+ItemDelegate::constActionBorder), 0, 0, 0);
    if (act2 && actionRect.contains(QCursor::pos())) {
        return act2;
    }
    return 0;
}

void ItemView::itemClicked(const QModelIndex &index)
{
    QAction *act=getAction(index);
    if (act) {
        act->trigger();
    }
}

void ItemView::itemActivated(const QModelIndex &index)
{
    QAction *act=getAction(index);
    if (act) {
        return;
    }

    if (usingTreeView) {
        treeView->setExpanded(index, !treeView->isExpanded(index));
    } else if (index.isValid() && index.child(0, 0).isValid()) {
        prevTopIndex=listView->indexAt(QPoint(0, 0));
        setLevel(currentLevel+1);
        #ifdef ENABLE_KDE_SUPPORT
        listSearch->setPlaceholderText(i18n("Search %1...", index.data(Qt::DisplayRole).toString()));
        #else
        listSearch->setPlaceholderText(tr("Search %1...").arg(index.data(Qt::DisplayRole).toString()));
        #endif
        listView->setRootIndex(index);
        listView->scrollToTop();
    }
}
