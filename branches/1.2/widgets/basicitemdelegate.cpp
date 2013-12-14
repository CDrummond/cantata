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

#include "basicitemdelegate.h"
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>

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
        col.setAlphaF(constAlpha);
        p->setPen(QPen(col, 1));
    }
    p->drawLine(r.bottomLeft(), r.bottomRight());
}

BasicItemDelegate::BasicItemDelegate(QObject *p)
    : QStyledItemDelegate(p)
{
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
    QStyledItemDelegate::paint(painter, option, index);
    QColor col(option.palette.color(active ? QPalette::Active : QPalette::Inactive,
                                    selected ? QPalette::HighlightedText : QPalette::Text));


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
        drawLine(painter, option.rect, col, false, false);
    }
}

