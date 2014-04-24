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

#include "actionitemdelegate.h"
#include "roles.h"
#include "icon.h"
#include "utils.h"
#include "config.h"
#include "actionmodel.h"
#include "groupedview.h"
#include <QPainter>
#include <QPixmap>
#include <QListView>
#include <QHelpEvent>
#include <QToolTip>
#include <QPointer>
#include <QApplication>

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
        constActionBorder=Utils::touchFriendly() ? 6 : 4;
        constActionIconSize=Utils::touchFriendly() ? 22 : 16;
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
                    ? QRect(rect.x()+5,
                            rect.y()+2,
                            iconSize, iconSize)
                    : QRect(rect.x()+constActionBorder,
                            rect.y()+((rect.height()-iconSize)/2),
                            iconSize, iconSize)
                : AP_VTop==actionPos
                    ? QRect(rect.x()+rect.width()-(iconSize+5),
                            rect.y()+2,
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
    bool touch=Utils::touchFriendly();
    bool adjustOpacity=!mouseOver && !(touch && AP_VTop==actionPos);
    if (adjustOpacity) {
        painter->setOpacity(opacity*0.25);
    }
    QRect actionRect=calcActionRect(rtl, actionPos, r);
    QList<Action *> actions=index.data(Cantata::Role_Actions).value<QList<Action *> >();

    foreach (const QPointer<Action> &a, actions) {
        QPixmap pix=a->icon().pixmap(QSize(iconSize, iconSize));
        if (!pix.isNull() && actionRect.width()>=pix.width()/* && r.x()>=0 && r.y()>=0*/) {
            painter->drawPixmap(actionRect.x()+(actionRect.width()-pix.width())/2,
                                actionRect.y()+(actionRect.height()-pix.height())/2, pix);
        }
        adjustActionRect(rtl, actionPos, actionRect, iconSize);
    }

    if (adjustOpacity) {
        painter->setOpacity(opacity);
    }
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
    QList<Action *> actions=index.data(Cantata::Role_Actions).value<QList<Action *> >();
    if (actions.isEmpty()) {
        return 0;
    }

    QAbstractItemView *view=(QAbstractItemView *)parent();
    bool rtl = Qt::RightToLeft==QApplication::layoutDirection();
    QListView *lv=qobject_cast<QListView *>(view);
    GroupedView *gv=lv ? 0 : qobject_cast<GroupedView *>(view);
    ActionPos actionPos=gv ? AP_HBottom : (lv && QListView::ListMode!=lv->viewMode() && index.child(0, 0).isValid() ? AP_VTop : AP_HMiddle);
    QRect rect = view->visualRect(index);
    rect.moveTo(view->viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
    bool showCapacity = !index.data(Cantata::Role_CapacityText).toString().isEmpty();
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
    QPoint cursorPos=QCursor::pos();

    foreach (const QPointer<Action> &a, actions) {
        actionRect=actionPos ? actionRect.adjusted(0, -2, 0, 2) : actionRect.adjusted(-2, 0, 2, 0);
        if (actionRect.contains(cursorPos)) {
            return a;
        }

        ActionItemDelegate::adjustActionRect(rtl, actionPos, actionRect, iconSize);
    }

    return 0;
}
