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

#include "togglebutton.h"
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QStyleOption>
#include <QtGui/QStylePainter>

ToggleButton::ToggleButton(QWidget *parent)
    : QToolButton(parent)
{
    setAutoRaise(true);
}

ToggleButton::~ToggleButton()
{
}

void ToggleButton::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
    if (isChecked()) {
        QPainter p(this);
        QRect rx(rect());
        QRectF r(rx.x()+0.5, rx.y()+0.5, rx.width()-1, rx.height()-1);
        QPainterPath path;
//         QColor c(palette().color(QPalette::Active, QPalette::Highlight));
        QColor borderCol(palette().color(QPalette::Active, QPalette::WindowText));

        path.addEllipse(r);
        p.setRenderHint(QPainter::Antialiasing, true);
//         c.setAlphaF(0.50);
//         p.fillPath(path, c);
        borderCol.setAlphaF(0.75);
        p.setPen(QPen(borderCol, 1.5));
        p.drawPath(path);
    }

    QStylePainter sp(this);
    QStyleOptionToolButton opt;

    initStyleOption(&opt);
    sp.drawControl(QStyle::CE_ToolButtonLabel, opt);
}
