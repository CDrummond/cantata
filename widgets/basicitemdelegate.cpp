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

#include "basicitemdelegate.h"
#include "support/gtkstyle.h"
#include "support/utils.h"
#include <QStyleOption>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QAbstractItemView>

void BasicItemDelegate::drawLine(QPainter *p, const QRect &r, const QColor &color, bool fadeStart, bool fadeEnd, double alpha)
{
    QColor col(color);
    QLinearGradient grad(r.bottomLeft(), r.bottomRight());

    if (fadeStart || fadeEnd) {
        double fadeSize=(fadeStart && fadeEnd ? 64.0 : 32.0);
        if (r.width()<(2.2*fadeSize)) {
            fadeSize=r.width()/3.0;
        }
        double fadePos=fadeSize/r.width();
        col.setAlphaF(fadeStart ? 0.0 : alpha);
        grad.setColorAt(0, col);
        col.setAlphaF(alpha);
        grad.setColorAt(fadePos, col);
        grad.setColorAt(1.0-fadePos, col);
        col.setAlphaF(fadeEnd ? 0.0 : alpha);
        grad.setColorAt(1, col);
        p->setPen(QPen(grad, 1));
    } else {
        col.setAlphaF(alpha);
        p->setPen(QPen(col, 1));
    }
    p->drawLine(r.bottomLeft(), r.bottomRight());
}

BasicItemDelegate::BasicItemDelegate(QObject *p)
    : QStyledItemDelegate(p)
    , trackMouse(false)
    , underMouse(false)
{
    if (GtkStyle::isActive() && qobject_cast<QAbstractItemView *>(p)) {
        static_cast<QAbstractItemView *>(p)->setAttribute(Qt::WA_MouseTracking);
        trackMouse=true;
        p->installEventFilter(this);
    }
}

BasicItemDelegate::~BasicItemDelegate()
{
}

void BasicItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return;
    }

    bool selected=option.state&QStyle::State_Selected;
    bool active=option.state&QStyle::State_Active;
    if (GtkStyle::isActive()) {
        bool mouseOver=option.state&QStyle::State_MouseOver;
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        if (trackMouse && !underMouse) {
            mouseOver=false;
        }

        if (mouseOver) {
            opt.showDecorationSelected=true;

            GtkStyle::drawSelection(option, painter, selected ? 0.75 : 0.25);
            opt.showDecorationSelected=false;
            opt.state&=~(QStyle::State_MouseOver|QStyle::State_Selected);
            opt.backgroundBrush=QBrush(Qt::transparent);
            if (selected) {
                opt.palette.setBrush(QPalette::Text, opt.palette.highlightedText());
            }
        }
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }

    QColor col(option.palette.color(active ? QPalette::Active : QPalette::Inactive,
                                    selected ? QPalette::HighlightedText : QPalette::Text));

    switch (((QStyleOptionViewItem &)option).viewItemPosition) {
    case QStyleOptionViewItem::Beginning:
        drawLine(painter, option.rect, col, true, false);
        break;
    case QStyleOptionViewItem::Middle:
        drawLine(painter, option.rect, col, false, false);
        break;
    case QStyleOptionViewItem::End:
        drawLine(painter, option.rect, col, false, true);
        break;
    case QStyleOptionViewItem::Invalid:
    case QStyleOptionViewItem::OnlyOne:
        drawLine(painter, option.rect, col, true, true);
    }
}

bool BasicItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (object==parent()) {
        if (QEvent::Enter==event->type()) {
            underMouse=true;
            static_cast<QAbstractItemView *>(parent())->viewport()->update();
        } else if (QEvent::Leave==event->type()) {
            underMouse=false;
            static_cast<QAbstractItemView *>(parent())->viewport()->update();
        }
    }
    return QStyledItemDelegate::eventFilter(object, event);
}
