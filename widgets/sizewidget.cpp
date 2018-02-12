/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "sizewidget.h"
#include "toolbutton.h"
#include "sizegrip.h"
#include "support/combobox.h"

static int swHeight=-1;

SizeWidget::SizeWidget(QWidget *parent)
    : QWidget(parent)
{
    if (-1 == swHeight) {
        ComboBox c(this);
        ToolButton b(this);
        SizeGrip g(this);
        c.ensurePolished();
        b.ensurePolished();
        g.ensurePolished();
        swHeight=qMax(c.sizeHint().height(), qMax(b.sizeHint().height(), g.sizeHint().height()));
    }
    setFixedHeight(swHeight);
}

void SizeWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
}
