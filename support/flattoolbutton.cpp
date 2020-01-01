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

#include "flattoolbutton.h"
#include "utils.h"
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionToolButton>

FlatToolButton::FlatToolButton(QWidget *parent)
    : QToolButton(parent)
{
    setStyleSheet("QToolButton {border: 0}");
}

void FlatToolButton::paintEvent(QPaintEvent *e)
{
    if (isChecked() || isDown()) {
        QPainter p(this);
        QColor col(palette().color(QPalette::WindowText));
        QRect r(rect());
        QPainterPath path=Utils::buildPath(QRectF(r.x()+0.5, r.y()+0.5, r.width()-1, r.height()-1), 2.5);
        p.setRenderHint(QPainter::Antialiasing, true);
        col.setAlphaF(0.4);
        p.setPen(col);
        p.drawPath(path);
        col.setAlphaF(0.1);
        p.fillPath(path, col);
    }
    QStylePainter p(this);
    QStyleOptionToolButton opt;
    initStyleOption(&opt);
//    if (isDown()) {
//        opt.state&=QStyle::State_Sunken;
//    }
    p.drawComplexControl(QStyle::CC_ToolButton, opt);
}
