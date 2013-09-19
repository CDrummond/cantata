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

#include "pagewidget.h"
#include "icon.h"
#include "gtkstyle.h"
#include <QListWidget>
#include <QStackedWidget>
#include <QBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QApplication>
#include <QFontMetrics>
#include <QTextLayout>
#include <QPainter>

static int layoutText(QTextLayout *layout, int maxWidth)
{
    qreal height = 0;
    int textWidth = 0;
    layout->beginLayout();
    while (true) {
        QTextLine line = layout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(maxWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        textWidth = qMax(textWidth, qRound(line.naturalTextWidth() + 0.5));
    }
    layout->endLayout();
    return textWidth;
}

class PageWidgetItemDelegate : public QAbstractItemDelegate
{
public:
    PageWidgetItemDelegate(QObject *parent)
        : QAbstractItemDelegate(parent)
    {
        int height=QApplication::fontMetrics().height();
        iconSize=height>22 ? Icon::stdSize(height*2.5) : 32;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }
        bool mouseOver=option.state&QStyle::State_MouseOver;
        bool gtk=mouseOver && GtkStyle::isActive();
        bool selected=option.state&QStyle::State_Selected;
        bool drawBgnd=true;

        if (!underMouse) {
            if (mouseOver && !selected) {
                drawBgnd=false;
            }
            mouseOver=false;
        }

        const QString text = index.model()->data(index, Qt::DisplayRole).toString();
        const QIcon icon = index.model()->data(index, Qt::DecorationRole).value<QIcon>();
        const QPixmap pixmap = icon.pixmap(iconSize, iconSize);

        QFontMetrics fm = painter->fontMetrics();
        int wp = pixmap.width();
        int hp = pixmap.height();

        QTextLayout iconTextLayout(text, option.font);
        QTextOption textOption(Qt::AlignHCenter);
        iconTextLayout.setTextOption(textOption);
        int maxWidth = qMax(3 * wp, 8 * fm.height());
        layoutText(&iconTextLayout, maxWidth);

        QPen pen = painter->pen();
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active)) {
            cg = QPalette::Inactive;
        }

        QStyleOptionViewItemV4 opt(option);
        opt.showDecorationSelected = true;
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

        if (drawBgnd) {
            if (mouseOver && gtk) {
                GtkStyle::drawSelection(opt, painter, selected ? 0.75 : 0.25);
            } else {
                style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
            }
        }

        if (selected) {
            painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
        } else {
            painter->setPen(option.palette.color(cg, QPalette::Text));
        }

        painter->drawPixmap(option.rect.x() + (option.rect.width()/2)-(wp/2), option.rect.y() + 5, pixmap);
        if (!text.isEmpty()) {
            iconTextLayout.draw(painter, QPoint(option.rect.x() + (option.rect.width()/2)-(maxWidth/2), option.rect.y() + hp+7));
        }
        painter->setPen(pen);
        drawFocus(painter, option, option.rect);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return QSize(0, 0);
        }

        const QString text = index.model()->data(index, Qt::DisplayRole).toString();
        const QIcon icon = index.model()->data(index, Qt::DecorationRole).value<QIcon>();
        const QPixmap pixmap = icon.pixmap(iconSize, iconSize);

        QFontMetrics fm = option.fontMetrics;
        int gap = fm.height();
        int wp = pixmap.width();
        int hp = pixmap.height();

        if (hp == 0) {
            /**
            * No pixmap loaded yet, we'll use the default icon size in this case.
            */
            hp = iconSize;
            wp = iconSize;
        }

        QTextLayout iconTextLayout(text, option.font);
        int wt = layoutText(&iconTextLayout, qMax(3 * wp, 8 * fm.height()));
        int ht = iconTextLayout.boundingRect().height();

        int width, height;
        if (text.isEmpty()) {
            height = hp;
        } else {
            height = hp + ht + 10;

        }

        width = qMax(wt, wp) + gap;
        return QSize(width, height);
    }

    void drawFocus(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect) const
    {
        if (option.state & QStyle::State_HasFocus) {
            QStyleOptionFocusRect o;
            o.QStyleOption::operator=(option);
            o.rect = rect;
            o.state |= QStyle::State_KeyboardFocusChange;
            QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled)
                    ? QPalette::Normal : QPalette::Disabled;
            o.backgroundColor = option.palette.color(cg, (option.state & QStyle::State_Selected)
                                                      ? QPalette::Highlight : QPalette::Background);
            QApplication::style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter);
        }
    }

    void setUnderMouse(bool u) { underMouse=u; }

private:
    int iconSize;
    bool underMouse;
};

class PageWidgetViewEventHandler : public QObject
{
public:
    PageWidgetViewEventHandler(PageWidgetItemDelegate *d, QListWidget *v)
        : QObject(v)
        , delegate(d)
        , view(v)
    {
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event)
    {
        if (delegate) {
            if (QEvent::Enter==event->type()) {
                delegate->setUnderMouse(true);
                view->viewport()->update();
            } else if (QEvent::Leave==event->type()) {
                delegate->setUnderMouse(false);
                view->viewport()->update();
            }
        }
        return QObject::eventFilter(obj, event);
    }

protected:
    PageWidgetItemDelegate *delegate;
    QListWidget *view;
};

PageWidgetItem::PageWidgetItem(QWidget *p, const QString &header, const Icon &icon, QWidget *cfg)
    : QWidget(p)
    , wid(cfg)
{
    static int size=-1;

    if (-1==size) {
        size=QApplication::fontMetrics().height();
        if (size>20) {
            size=Icon::stdSize(size*1.25);
        } else {
            size=22;
        }
    }

    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    QBoxLayout *titleLayout=new QBoxLayout(QBoxLayout::LeftToRight, 0);
    titleLayout->addWidget(new QLabel("<b>"+header+"</b>", this));
    titleLayout->addItem(new QSpacerItem(16, 16, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QLabel *icn=new QLabel(this);
    icn->setPixmap(icon.pixmap(size, size));
    titleLayout->addWidget(icn);
    layout->addLayout(titleLayout);
    layout->addItem(new QSpacerItem(8, 8, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(cfg);
    cfg->setParent(this);
    cfg->adjustSize();
    adjustSize();
}

PageWidget::PageWidget(QWidget *p)
    : QWidget(p)
{
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::LeftToRight, this);
    list = new QListWidget(p);
    stack = new QStackedWidget(p);
    connect(list, SIGNAL(currentRowChanged(int)), stack, SLOT(setCurrentIndex(int)));
    layout->addWidget(list);
    layout->addWidget(stack);
    layout->setMargin(0);
    list->setViewMode(QListView::ListMode);
    list->setVerticalScrollMode(QListView::ScrollPerPixel);
    list->setMovement(QListView::Static);
    PageWidgetItemDelegate *delegate=new PageWidgetItemDelegate(list);
    list->setItemDelegate(delegate);
    list->setSelectionBehavior(QAbstractItemView::SelectItems);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setAttribute(Qt::WA_MouseTracking);
    list->installEventFilter(new PageWidgetViewEventHandler(delegate, list));
}

PageWidgetItem * PageWidget::addPage(QWidget *widget, const QString &name, const Icon &icon, const QString &header)
{
    PageWidgetItem *page=new PageWidgetItem(stack, header, icon, widget);
    QListWidgetItem *listItem=new QListWidgetItem(name, list);
    listItem->setIcon(icon);
    stack->addWidget(page);
    list->addItem(listItem);

    int rows = list->model()->rowCount();
    int width = 0;
    int height = 0;
    for (int i = 0; i < rows; ++i) {
        QSize rowSize=list->sizeHintForIndex(list->model()->index(i, 0));
        width = qMax(width, rowSize.width());
        height += rowSize.height();
    }
    width+=25;
    list->setFixedWidth(width);
    list->setMinimumHeight(height);

    QSize stackSize = stack->size();
    for (int i = 0; i < stack->count(); ++i) {
        const QWidget *widget = stack->widget(i);
        if (widget) {
            stackSize = stackSize.expandedTo(widget->minimumSizeHint());
        }
    }
    stack->setMinimumSize(stackSize);

    QSize sz=size();
    sz=sz.expandedTo(stackSize);
    sz=sz.expandedTo(QSize(width, height));
    setMinimumSize(sz);
    list->setCurrentRow(0);
    stack->setCurrentIndex(0);
    return page;
}

int PageWidget::count()
{
    return list->count();
}

PageWidgetItem * PageWidget::currentPage() const
{
    return static_cast<PageWidgetItem *>(stack->currentWidget());
}
