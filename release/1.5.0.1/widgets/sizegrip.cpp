/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "toolbutton.h"
#include "icons.h"
#include <QBoxLayout>
#include <QSizeGrip>

SizeGrip::SizeGrip(QWidget *parent)
    : QWidget(parent)
{
    QBoxLayout *l=new QBoxLayout(QBoxLayout::TopToBottom, this);
    l->addItem(new QSpacerItem(1, 0, QSizePolicy::Maximum, QSizePolicy::Preferred));
    QSizeGrip *grip=new QSizeGrip(this);
    l->addWidget(grip);
    l->setMargin(0);
    l->setSpacing(0);
    l->setAlignment(Qt::AlignBottom);
    ToolButton tb;
    tb.move(65535, 65535);
    tb.setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb.setIcon(Icons::self()->albumIcon);
    tb.ensurePolished();
    tb.setAttribute(Qt::WA_DontShowOnScreen);
    tb.setVisible(true);
    setMinimumWidth(qMax(grip->sizeHint().width(), tb.sizeHint().width()));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
}

