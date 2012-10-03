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

#include "spinbox.h"
#include "icon.h"
#include "gtkstyle.h"
#include <QtGui/QToolButton>
#include <QtGui/QHBoxLayout>
#include <QtGui/QSpacerItem>
#include <QtGui/QApplication>
#include <QtGui/QWheelEvent>

static Icon incIcon;
static Icon decIcon;

static void intButton(QToolButton *btn, bool inc)
{
    if (incIcon.isNull()) {
        incIcon=Icon("list-add");
    }
    if (decIcon.isNull()) {
        decIcon=Icon("list-remove");
    }
    btn->setAutoRaise(true);
    btn->setIcon(inc ? incIcon : decIcon);
    btn->setAutoRepeat(true);
    //btn->setMinimumWidth(w);
    //btn->setMaximumWidth(w);
}

SpinBox::SpinBox(QWidget *p)
    : QWidget(p)
{
    QHBoxLayout *layout=new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    spin=new EmptySpinBox(this);
    if (GtkStyle::mimicWidgets()) {
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        decButton=new QToolButton(this);
        incButton=new QToolButton(this);
        layout->addWidget(spin);
        layout->addWidget(decButton);
        layout->addWidget(incButton);
        layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
        connect(spin, SIGNAL(valueChanged(int)), SIGNAL(valueChanged(int)));
        connect(decButton, SIGNAL(pressed()), SLOT(decPressed()));
        connect(incButton, SIGNAL(pressed()), SLOT(incPressed()));
        intButton(decButton, false);
        intButton(incButton, true);
        decButton->installEventFilter(this);
        incButton->installEventFilter(this);
    } else {
        layout->addWidget(spin);
    }
}

SpinBox::~SpinBox()
{
}

void SpinBox::setValue(int v)
{
    spin->setValue(v);
    if (GtkStyle::mimicWidgets()) {
        decButton->setEnabled(spin->value()>spin->minimum());
        incButton->setEnabled(spin->value()<spin->maximum());
    }
}

void SpinBox::incPressed()
{
    spin->setValue(spin->value()+spin->singleStep());
    decButton->setEnabled(spin->value()>spin->minimum());
    incButton->setEnabled(spin->value()<spin->maximum());
}

void SpinBox::decPressed()
{
    spin->setValue(spin->value()-spin->singleStep());
    decButton->setEnabled(spin->value()>spin->minimum());
    incButton->setEnabled(spin->value()<spin->maximum());
}

bool SpinBox::eventFilter(QObject *obj, QEvent *event)
{
    if (GtkStyle::mimicWidgets() && (obj==incButton || obj==decButton) && QEvent::Wheel==event->type()) {
        QWheelEvent *we=(QWheelEvent *)event;
        if (we->delta()<0) {
            decPressed();
        } else if (we->delta()>0) {
            incPressed();
        }
        return true;
    }

    return QWidget::eventFilter(obj, event);
}
