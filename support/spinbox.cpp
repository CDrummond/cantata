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
#include <QtGui/QApplication>
#include <QtGui/QPixmap>
#include <QtGui/QFont>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtGui/QStyle>
#include <QtGui/QStyleOption>

static Icon incIcon;
static Icon decIcon;

static QPixmap createPixmap(int size, QColor &col, double opacity, bool isPlus)
{
    int lineWidth=size<32 ? 2 : 4;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.setPen(QPen(col, lineWidth));
    p.setOpacity(opacity);

    int offset=lineWidth*2;
    int pos=(size/2);
    p.drawLine(offset, pos, size-offset, pos);
    if (isPlus) {
        p.drawLine(pos, offset, pos, size-offset);
    }
    p.end();
    return pix;
}

static Icon createIcon(bool isPlus)
{
    Icon icon;
    QColor stdColor=QColor(QApplication::palette().color(QPalette::Active, QPalette::ButtonText));
    QColor highlightColor=stdColor.red()<100 ? stdColor.lighter(50) : stdColor.darker(50);
    QList<int> sizes=QList<int>() << 16 << 22;

    foreach (int s, sizes) {
        icon.addPixmap(createPixmap(s, stdColor, 1.0, isPlus));
        icon.addPixmap(createPixmap(s, stdColor, 0.5, isPlus), QIcon::Disabled);
        icon.addPixmap(createPixmap(s, highlightColor, 1.0, isPlus), QIcon::Active);
    }

    return icon;
}

static void intButton(QToolButton *btn, bool inc)
{
    if (incIcon.isNull()) {
        incIcon=createIcon(true); // Icon("list-add");
    }
    if (decIcon.isNull()) {
        decIcon=createIcon(false); // Icon("list-remove");
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
        connect(spin, SIGNAL(valueChanged(int)), SLOT(checkValue()));
        connect(spin, SIGNAL(valueChanged(int)), SIGNAL(valueChanged(int)));
        connect(decButton, SIGNAL(pressed()), SLOT(decPressed()));
        connect(incButton, SIGNAL(pressed()), SLOT(incPressed()));
        intButton(decButton, false);
        intButton(incButton, true);
        decButton->installEventFilter(this);
        incButton->installEventFilter(this);
        QPalette pal(palette());
        pal.setColor(QPalette::Base, palette().color(QPalette::Window));
        setPalette(pal);
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
        checkValue();
    }
}

void SpinBox::incPressed()
{
    spin->setValue(spin->value()+spin->singleStep());
    checkValue();
}

void SpinBox::decPressed()
{
    spin->setValue(spin->value()-spin->singleStep());
    checkValue();
}

void SpinBox::checkValue()
{
    decButton->setEnabled(spin->value()>spin->minimum());
    incButton->setEnabled(spin->value()<spin->maximum());
}

void SpinBox::paintEvent(QPaintEvent *e)
{
    if (GtkStyle::mimicWidgets()) {
        QStyleOptionFrame opt;
        opt.init(this);
        opt.rect=spin->geometry().united(incButton->geometry()).united(decButton->geometry());
        opt.state=(isEnabled() ? QStyle::State_Enabled : QStyle::State_None)|QStyle::State_Sunken;
        opt.lineWidth=1;
        QPainter painter(this);
        QApplication::style()->drawPrimitive(QStyle::PE_PanelLineEdit, &opt, &painter, this);
    } else {
        QWidget::paintEvent(e);
    }
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
