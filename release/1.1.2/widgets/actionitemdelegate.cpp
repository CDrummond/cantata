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

#include "actionitemdelegate.h"
#include "itemview.h"
#include "icon.h"
#include "config.h"
#include "groupedview.h"
#include "actionmodel.h"
#include <QPainter>
#include <QPixmap>
#include <QListView>
#include <QHelpEvent>
#include <QToolTip>
#include <QPointer>

int ActionItemDelegate::constBorder = 1;
int ActionItemDelegate::constActionBorder = 4;
int ActionItemDelegate::constActionIconSize = 16;
int ActionItemDelegate::constLargeActionIconSize = 22;

void ActionItemDelegate::setup()
{
    int height=QApplication::fontMetrics().height();

    if (height>17) {
        constActionIconSize=Icon::stdSize(((int)(height/4))*4);
        constLargeActionIconSize=Icon::stdSize(((int)(height/3))*3);
        constBorder=constActionIconSize>22 ? 2 : 1;
        constActionBorder=constActionIconSize>32 ? 6 : 4;
    } else {
        constActionBorder=4;
        constActionIconSize=16;
        constLargeActionIconSize=22;
        constBorder=1;
    }
}

QRect ActionItemDelegate::calcActionRect(bool rtl, ActionPos actionPos, const QRect &o) const
{
    int iconSize=largeIcons ? constLargeActionIconSize : constActionIconSize;

    QRect rect=AP_HBottom==actionPos ? QRect(o.x(), o.y()+(o.height()/2), o.width(), o.height()/2) : o;
    return rtl
                ? AP_VTop==actionPos
                    ? QRect(rect.x()+constActionBorder+3,
                            rect.y()+constActionBorder,
                            iconSize, iconSize)
                    : QRect(rect.x()+constActionBorder,
                            rect.y()+((rect.height()-iconSize)/2),
                            iconSize, iconSize)
                : AP_VTop==actionPos
                    ? QRect(rect.x()+rect.width()-(iconSize+constActionBorder+3),
                            rect.y()+constActionBorder,
                            iconSize, iconSize)
                    : QRect(rect.x()+rect.width()-(iconSize+constActionBorder),
                            rect.y()+((rect.height()-iconSize)/2),
                            iconSize, iconSize);
}

void ActionItemDelegate::adjustActionRect(bool rtl, ActionPos actionPos, QRect &rect, int iconSize)
{
    if (rtl) {
        if (AP_VTop==actionPos) {
            rect.adjust(0, iconSize+constActionBorder, 0, iconSize+constActionBorder);
        } else {
            rect.adjust(iconSize+constActionBorder, 0, iconSize+constActionBorder, 0);
        }
    } else {
        if (AP_VTop==actionPos) {
            rect.adjust(0, iconSize+constActionBorder, 0, iconSize+constActionBorder);
        } else {
            rect.adjust(-(iconSize+constActionBorder), 0, -(iconSize+constActionBorder), 0);
        }
    }
}

static QPainterPath buildPath(const QRectF &r, double radius)
{
    QPainterPath path;
    double diameter(radius*2);

    path.moveTo(r.x()+r.width(), r.y()+r.height()-radius);
    path.arcTo(r.x()+r.width()-diameter, r.y(), diameter, diameter, 0, 90);
    path.arcTo(r.x(), r.y(), diameter, diameter, 90, 90);
    path.arcTo(r.x(), r.y()+r.height()-diameter, diameter, diameter, 180, 90);
    path.arcTo(r.x()+r.width()-diameter, r.y()+r.height()-diameter, diameter, diameter, 270, 90);
    return path;
}

static void drawBgnd(QPainter *painter, const QRect &rx)
{
    QRectF r(rx.x()-0.5, rx.y()-0.5, rx.width()+1, rx.height()+1);
    QPainterPath p(buildPath(r, 3.0));
    QColor c(Qt::white);

    painter->setRenderHint(QPainter::Antialiasing, true);
    c.setAlphaF(0.75);
    painter->fillPath(p, c);
    c.setAlphaF(0.95);
    painter->setPen(c);
    painter->drawPath(p);
    painter->setRenderHint(QPainter::Antialiasing, false);
}

ActionItemDelegate::ActionItemDelegate(QObject *p)
    : QStyledItemDelegate(p)
    , largeIcons(false)
    , underMouse(false)
{
}

void ActionItemDelegate::drawIcons(QPainter *painter, const QRect &r, bool mouseOver, bool rtl, ActionPos actionPos, const QModelIndex &index) const
{
    int iconSize=largeIcons ? constLargeActionIconSize : constActionIconSize;
    double opacity=painter->opacity();
    if (!mouseOver) {
        painter->setOpacity(opacity*0.2);
    }

    QRect actionRect=calcActionRect(rtl, actionPos, r);
    QList<Action *> actions=index.data(ItemView::Role_Actions).value<QList<Action *> >();

    foreach (const QPointer<Action> &a, actions) {
        QPixmap pix=a->icon().pixmap(QSize(iconSize, iconSize));
        if (!pix.isNull() && actionRect.width()>=pix.width()/* && r.x()>=0 && r.y()>=0*/) {
            drawBgnd(painter, actionRect);
            painter->drawPixmap(actionRect.x()+(actionRect.width()-pix.width())/2,
                                actionRect.y()+(actionRect.height()-pix.height())/2, pix);
        }
        adjustActionRect(rtl, actionPos, actionRect, iconSize);
    }

    if (!mouseOver) {
        painter->setOpacity(opacity);
    }
}

void ActionItemDelegate::drawDivider(QPainter *p, const QRect &r, const QColor &color) const
{
    QLinearGradient grad(r.bottomLeft(), r.bottomRight());
    QColor col(color);
    double fadeSize=64.0;
    if (r.width()<(2.2*fadeSize)) {
        fadeSize=r.width()/3.0;
    }
    double fadePos=fadeSize/r.width();
    col.setAlphaF(0.0);
    grad.setColorAt(0, col);
    col.setAlphaF(0.1);
    grad.setColorAt(fadePos, col);
    grad.setColorAt(1.0-fadePos, col);
    col.setAlphaF(0.0);
    grad.setColorAt(1, col);
    p->setPen(QPen(grad, 1));
    p->drawLine(r.bottomLeft(), r.bottomRight());
}

bool ActionItemDelegate::helpEvent(QHelpEvent *e, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (QEvent::ToolTip==e->type()) {
        QAction *act=getAction(index);
        if (act) {
            QToolTip::showText(e->globalPos(), act->toolTip(), view);
            return true;
        }
    }
    return QStyledItemDelegate::helpEvent(e, view, option, index);
}

QAction * ActionItemDelegate::getAction(const QModelIndex &index) const
{
    QAbstractItemView *view=(QAbstractItemView *)parent();
    bool rtl = Qt::RightToLeft==QApplication::layoutDirection();
    QListView *lv=qobject_cast<QListView *>(view);
    GroupedView *gv=lv ? 0 : qobject_cast<GroupedView *>(view);
    ActionPos actionPos=gv ? AP_HBottom : (lv && QListView::ListMode!=lv->viewMode() && index.child(0, 0).isValid() ? AP_VTop : AP_HMiddle);
    QRect rect = view->visualRect(index);
    rect.moveTo(view->viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
    bool showCapacity = !index.data(ItemView::Role_CapacityText).toString().isEmpty();
    if (gv || lv || showCapacity) {
        if (AP_VTop==actionPos) {
            rect.adjust(ActionItemDelegate::constBorder, ActionItemDelegate::constBorder, -ActionItemDelegate::constBorder, -ActionItemDelegate::constBorder);
        } else {
            rect.adjust(ActionItemDelegate::constBorder+3, 0, -(ActionItemDelegate::constBorder+3), 0);
        }
    }

    if (showCapacity) {
        int textHeight=QFontMetrics(QApplication::font()).height();
        rect.adjust(0, 0, 0, -(textHeight+8));
    }

    int iconSize=largeIcons ? constLargeActionIconSize : constActionIconSize;
    QRect actionRect=calcActionRect(rtl, actionPos, rect);
    QRect actionRect2(actionRect);
    ActionItemDelegate::adjustActionRect(rtl, actionPos, actionRect2, iconSize);
    QList<Action *> actions=index.data(ItemView::Role_Actions).value<QList<Action *> >();

    foreach (const QPointer<Action> &a, actions) {
        actionRect=actionPos ? actionRect.adjusted(0, -2, 0, 2) : actionRect.adjusted(-2, 0, 2, 0);
        if (actionRect.contains(QCursor::pos())) {
            return a;
        }

        ActionItemDelegate::adjustActionRect(rtl, actionPos, actionRect, iconSize);
    }

    return 0;
}
