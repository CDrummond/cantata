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
#include "sizewidget.h"
#include <QIcon>

// Some styles, such as MacOSX, do not have a size-grip.
// However, we still want the same space taken up on the toolbar - so that 'clear playqueue'
// is not right in the bottom-right corner.
//
// Also, we want the size-grip to have the same height as a toolbutton - so that it is aligned
// to the bottom of the main window.
//
// This class attempts to rectify these issues...
SizeGrip::SizeGrip(QWidget *parent)
    : QSizeGrip(parent)
{
    setFixedHeight(SizeWidget::standardHeight());
}

QSize SizeGrip::sizeHint() const
{
    if (!sh.isValid()) {
        int itemHeight=SizeWidget::standardHeight();
        sh=QSizeGrip::sizeHint();
        sh=QSize(sh.width()>2 ? sh.width() : itemHeight, itemHeight);
    }
    return sh;
}
