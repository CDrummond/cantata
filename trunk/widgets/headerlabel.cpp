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

#include "headerlabel.h"
#include "lineedit.h"
#include <QFont>
#include <QPainter>
#include <QRect>
#include <QLinearGradient>

HeaderLabel::HeaderLabel(QWidget *p)
    : SqueezedTextLabel(p)
{
    QFont f(font());
    f.setBold(true);
    setFont(f);
    setMinimumHeight(LineEdit().minimumHeight());
    setAlignment(Qt::AlignVCenter | (Qt::RightToLeft==layoutDirection() ? Qt::AlignRight : Qt::AlignLeft));
    int padding=fontMetrics().height()*0.25;
    setStyleSheet(QString("padding-left: %1px ; padding-right: %1px").arg(padding));
}

void HeaderLabel::paintEvent(QPaintEvent *ev)
{
    QPainter p(this);
    QRect r(rect());
    QLinearGradient grad(r.topLeft(), r.topRight());
    QColor col(palette().color(QPalette::Highlight));
    bool rtl=Qt::RightToLeft==layoutDirection();
    col.setAlphaF(0.85);
    grad.setColorAt(rtl ? 1 : 0, col);
    col.setAlphaF(0.0);
    grad.setColorAt(rtl ? 0 : 1, col);
    p.fillRect(r, grad);
    p.end();
    SqueezedTextLabel::paintEvent(ev);
}
