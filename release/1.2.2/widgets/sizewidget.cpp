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

#include "sizewidget.h"
#include "genrecombo.h"
#include "toolbutton.h"
#include <QIcon>

static int stdHeight=-1;

int SizeWidget::standardHeight()
{
    if (-1==stdHeight)
    {
        GenreCombo combo(0);
        combo.ensurePolished();
        ToolButton tb(0);
        tb.setToolButtonStyle(Qt::ToolButtonIconOnly);
        tb.setIcon(QIcon::fromTheme("ok"));
        tb.ensurePolished();
        stdHeight=qMax(tb.sizeHint().height(), combo.sizeHint().height());
    }

    return stdHeight;
}

SizeWidget::SizeWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(stdHeight);
    setFixedWidth(0);
}
