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

#include "wizardpage.h"
#include "icon.h"
#include <QPainter>
#include <QStyle>

void WizardPage::setBackground(const Icon &i)
{
    int size=fontMetrics().height()*10;
    size=((int)(size/4))*4;
    pix=i.pixmap(size, QIcon::Disabled);
    if (pix.width()<size && pix.height()<size) {
        pix=pix.scaled(QSize(size, size), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
}

void WizardPage::paintEvent(QPaintEvent *e)
{
    int spacing=style()->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Qt::Vertical);
    if (spacing<0) {
        spacing=4;
    }
    spacing*=2;

    QWizardPage::paintEvent(e);
    QRect r(rect());
    QPainter painter(this);

    painter.setOpacity(0.2);
    painter.drawPixmap(Qt::RightToLeft == layoutDirection() ? r.left() + spacing : r.right() - (pix.width() + spacing),
                       r.bottom() - (pix.height() + spacing), pix);
}
