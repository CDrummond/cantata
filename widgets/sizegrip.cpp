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

#include "sizegrip.h"
#include <QToolButton>
#include <QIcon>

SizeGrip::SizeGrip(QWidget *parent)
    : QSizeGrip(parent)
{
}

QSize SizeGrip::sizeHint() const
{
    // Some styles, suach as MacOSX, do no have the size-grip
    // However, we still want the same space taken up on the toolbar - so that 'clear playqueue'
    // is not righ ti nth ebottom-right corner.
    //
    // Also, we want the size-grip to have the same height as a toolbutton - so that it is aligned
    // to the bottom of the main window.
    //
    // Overriding sizeHint() as follows, fixes both these issues.
    if (!sh.isValid()) {
        sh=QSizeGrip::sizeHint();
        QToolButton tb(parentWidget());
        tb.setToolButtonStyle(Qt::ToolButtonIconOnly);
        tb.setIcon(QIcon::fromTheme("ok"));
        QSize tbSize=tb.sizeHint();
        sh=QSize(sh.width()>2 ? sh.width() : tbSize.width(), tbSize.height());
    }
    return sh;
}
