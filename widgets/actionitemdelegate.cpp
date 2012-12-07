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

#include "actionitemdelegate.h"
#include "itemview.h"
#include "icon.h"
#include "config.h"
#include "groupedview.h"
#include <QtGui/QPainter>
#include <QtGui/QAction>
#include <QtGui/QPixmap>
#include <QtGui/QListView>
#include <QtGui/QHelpEvent>
#include <QtGui/QToolTip>

int ActionItemDelegate::constBorder = 1;
int ActionItemDelegate::constActionBorder = 4;
int ActionItemDelegate::constActionIconSize = 16;

void ActionItemDelegate::setup()
{
    int height=QApplication::fontMetrics().height();

    if (height>17) {
        constActionIconSize=Icon::stdSize(((int)(height/4))*4);
        constBorder=constActionIconSize>22 ? 2 : 1;
        constActionBorder=constActionIconSize>32 ? 6 : 4;
    } else {
        constActionBorder=4;
        constActionIconSize=16;
        constBorder=1;
    }
}

QRect ActionItemDelegate::calcActionRect(bool rtl, bool iconMode, const QRect &rect)
{
    return rtl
                ? iconMode
                    ? QRect(rect.x()+constActionBorder,
                            rect.y()+constActionBorder,
                            constActionIconSize, constActionIconSize)
                    : QRect(rect.x()+constActionBorder,
                            rect.y()+((rect.height()-constActionIconSize)/2),
                            constActionIconSize, constActionIconSize)
                : iconMode
                    ? QRect(rect.x()+rect.width()-(constActionIconSize+constActionBorder),
                            rect.y()+constActionBorder,
                            constActionIconSize, constActionIconSize)
                    : QRect(rect.x()+rect.width()-(constActionIconSize+constActionBorder),
                            rect.y()+((rect.height()-constActionIconSize)/2),
                            constActionIconSize, constActionIconSize);
}

void ActionItemDelegate::adjustActionRect(bool rtl, bool iconMode, QRect &rect)
{
    if (rtl) {
        if (iconMode) {
            rect.adjust(0, constActionIconSize+constActionBorder, 0, constActionIconSize+constActionBorder);
        } else {
            rect.adjust(constActionIconSize+constActionBorder, 0, constActionIconSize+constActionBorder, 0);
        }
    } else {
        if (iconMode) {
            rect.adjust(0, constActionIconSize+constActionBorder, 0, constActionIconSize+constActionBorder);
        } else {
            rect.adjust(-(constActionIconSize+constActionBorder), 0, -(constActionIconSize+constActionBorder), 0);
        }
    }
}

bool ActionItemDelegate::hasActions(const QModelIndex &index, int actLevel)
{
    if (actLevel<0) {
        return true;
    }

    int level=0;

    QModelIndex idx=index;
    while(idx.parent().isValid()) {
        if (++level>actLevel) {
            return false;
        }
        idx=idx.parent();
    }
    return true;
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

void ActionItemDelegate::drawIcons(QPainter *painter, const QRect &r, bool mouseOver, bool rtl, bool iconMode, const QModelIndex &index) const
{
    double opacity=painter->opacity();
    if (!mouseOver) {
        painter->setOpacity(opacity*0.2);
    }

    QRect actionRect=calcActionRect(rtl, iconMode, r);

    if (act1) {
        QPixmap pix=act1->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
        if (!pix.isNull() && actionRect.width()>=pix.width()/* && r.x()>=0 && r.y()>=0*/) {
            drawBgnd(painter, actionRect);
            painter->drawPixmap(actionRect.x()+(actionRect.width()-pix.width())/2,
                                actionRect.y()+(actionRect.height()-pix.height())/2, pix);
        }
    }

    if (act2) {
        if (act1) {
            adjustActionRect(rtl, iconMode, actionRect);
        }
        QPixmap pix=act2->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
        if (!pix.isNull() && actionRect.width()>=pix.width()/* && r.x()>=0 && r.y()>=0*/) {
            drawBgnd(painter, actionRect);
            painter->drawPixmap(actionRect.x()+(actionRect.width()-pix.width())/2,
                                actionRect.y()+(actionRect.height()-pix.height())/2, pix);
        }
    }

    if (toggle) {
        QIcon icon=index.data(ItemView::Role_ToggleIcon).value<QIcon>();
        if (!icon.isNull()) {
            if (act1 || act2) {
                adjustActionRect(rtl, iconMode, actionRect);
            }
            QPixmap pix=icon.pixmap(QSize(constActionIconSize, constActionIconSize));
            if (!pix.isNull() && actionRect.width()>=pix.width()/* && r.x()>=0 && r.y()>=0*/) {
                drawBgnd(painter, actionRect);
                painter->drawPixmap(actionRect.x()+(actionRect.width()-pix.width())/2,
                                    actionRect.y()+(actionRect.height()-pix.height())/2, pix);
            }
        }
    }
    if (!mouseOver) {
        painter->setOpacity(opacity);
    }
}

bool ActionItemDelegate::helpEvent(QHelpEvent *e, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (QEvent::ToolTip==e->type() && (act1 || act2 || toggle)) {
        QAction *act=getAction(view, index);
        if (act) {
            if (act==toggle) {
                QString tt=index.data(ItemView::Role_ToggleToolTip).toString();
                if (!tt.isEmpty()) {
                    QToolTip::showText(e->globalPos(), tt, view);
                    return true;
                }
            }
            QToolTip::showText(e->globalPos(), act->toolTip(), view);
            return true;
        }
    }
    return QStyledItemDelegate::helpEvent(e, view, option, index);
}

QAction * ActionItemDelegate::getAction(QAbstractItemView *view, const QModelIndex &index) const
{
    if (!hasActions(index, actLevel)) {
        return 0;
    }

    bool rtl = Qt::RightToLeft==QApplication::layoutDirection();
    QListView *lv=qobject_cast<QListView *>(view);
    GroupedView *gv=lv ? 0 : qobject_cast<GroupedView *>(view);
    bool iconMode=lv && QListView::ListMode!=lv->viewMode() && index.child(0, 0).isValid();
    QRect rect = view->visualRect(index);
    rect.moveTo(view->viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
    bool showCapacity = !index.data(ItemView::Role_CapacityText).toString().isEmpty();
    bool haveToggle = toggle && !index.data(ItemView::Role_ToggleIcon).value<QIcon>().isNull();
    if (gv || lv || showCapacity) {
        if (iconMode) {
            rect.adjust(ActionItemDelegate::constBorder, ActionItemDelegate::constBorder, -ActionItemDelegate::constBorder, -ActionItemDelegate::constBorder);
        } else {
            rect.adjust(ActionItemDelegate::constBorder+3, 0, -(ActionItemDelegate::constBorder+3), 0);
        }
    }

    if (showCapacity) {
        int textHeight=QFontMetrics(QApplication::font()).height();
        rect.adjust(0, 0, 0, -(textHeight+8));
    }

    QRect actionRect=ActionItemDelegate::calcActionRect(rtl, iconMode, rect);
    QRect actionRect2(actionRect);
    ActionItemDelegate::adjustActionRect(rtl, iconMode, actionRect2);

    actionRect=iconMode ? actionRect.adjusted(0, -2, 0, 2) : actionRect.adjusted(-2, 0, 2, 0);
    if (act1 && actionRect.contains(QCursor::pos())) {
        return act1;
    }

    if (act1) {
        ActionItemDelegate::adjustActionRect(rtl, iconMode, actionRect);
    }

    actionRect=iconMode ? actionRect.adjusted(0, -2, 0, 2) : actionRect.adjusted(-2, 0, 2, 0);
    if (act2 && actionRect.contains(QCursor::pos())) {
        return act2;
    }

    if (haveToggle) {
        if (act1 || act2) {
            ActionItemDelegate::adjustActionRect(rtl, iconMode, actionRect);
        }

        actionRect=iconMode ? actionRect.adjusted(0, -2, 0, 2) : actionRect.adjusted(-2, 0, 2, 0);
        if (toggle && actionRect.contains(QCursor::pos())) {
            return toggle;
        }
    }

    return 0;
}
