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

#include "spinbox.h"
#include "icon.h"
#include "gtkstyle.h"
#include <QToolButton>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QApplication>
#include <QWheelEvent>
#include <QApplication>
#include <QPixmap>
#include <QFont>
#include <QPainter>
#include <QLinearGradient>
#include <QPalette>
#include <QStyle>
#include <QStyleOption>

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
        incIcon=createIcon(true);
    }
    if (decIcon.isNull()) {
        decIcon=createIcon(false);
    }
    btn->setAutoRaise(true);
    btn->setIcon(inc ? incIcon : decIcon);
    btn->setAutoRepeat(true);
}

static QString toString(const QColor &col, int alpha)
{
    return QString("rgba(%1, %2, %3, %4%)").arg(col.red()).arg(col.green()).arg(col.blue()).arg(alpha);
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
        QString stylesheet=QString("QToolButton { border: 0px } QToolButton:pressed { background-color: %2 } ")
                           .arg(toString(palette().highlight().color(), 20));
        decButton->setStyleSheet(stylesheet);
        incButton->setStyleSheet(stylesheet);
        decButton->setFocusPolicy(Qt::NoFocus);
        incButton->setFocusPolicy(Qt::NoFocus);
        spin->installEventFilter(this);
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

static void drawLine(QPainter *painter, QColor col, const QPoint &start, const QPoint &end)
{
    QLinearGradient grad(start, end);
    col.setAlphaF(0.0);
    grad.setColorAt(0, col);
    col.setAlphaF(0.25);
    grad.setColorAt(0.25, col);
    grad.setColorAt(0.8, col);
    col.setAlphaF(0.0);
    grad.setColorAt(1, col);
    painter->setPen(QPen(QBrush(grad), 1));
    painter->drawLine(start, end);
}

void SpinBox::paintEvent(QPaintEvent *e)
{
    if (GtkStyle::mimicWidgets()) {
        QStyleOptionFrame opt;
        opt.init(this);
        opt.rect=spin->geometry().united(incButton->geometry()).united(decButton->geometry());
        if (Qt::RightToLeft==QApplication::layoutDirection()) {
            opt.rect.adjust(-1, 0, 1, 0);
        } else {
            opt.rect.adjust(0, 0, 1, 0);
        }
        opt.state=(isEnabled() ? (QStyle::State_Enabled|(spin->hasFocus() ? QStyle::State_HasFocus : QStyle::State_None)) : QStyle::State_None)|QStyle::State_Sunken;
        opt.lineWidth=1;
        QPainter painter(this);
        QApplication::style()->drawPrimitive(QStyle::PE_PanelLineEdit, &opt, &painter, this);
        QColor col(palette().foreground().color());
        if (Qt::RightToLeft==QApplication::layoutDirection()) {
            drawLine(&painter, col, incButton->geometry().topRight(), incButton->geometry().bottomRight());
            drawLine(&painter, col, decButton->geometry().topRight(), decButton->geometry().bottomRight());
        } else {
            drawLine(&painter, col, incButton->geometry().topLeft(), incButton->geometry().bottomLeft());
            drawLine(&painter, col, decButton->geometry().topLeft(), decButton->geometry().bottomLeft());
        }
    } else {
        QWidget::paintEvent(e);
    }
}

bool SpinBox::eventFilter(QObject *obj, QEvent *event)
{
    if (GtkStyle::mimicWidgets()) {
        if ((obj==incButton || obj==decButton) && QEvent::Wheel==event->type()) {
            QWheelEvent *we=(QWheelEvent *)event;
            if (we->delta()<0) {
                decPressed();
            } else if (we->delta()>0) {
                incPressed();
            }
            return true;
        } else if (obj==spin) {
            // QGtkStyle does not seem to like spinboxes with no buttons - which is what we use. It looks like it sets the 'has focus' flag
            // on some internal gtk-line edit. If a spinbox has focus, and we change to another config dialog page - ALL line-edits on the new
            // page a re drawn as if they have focus.
            //
            // So, to 'fix' this, we ignore paint events on the spin-box itself, and handle focus in/out here...
            if (QEvent::FocusIn==event->type() || QEvent::FocusOut==event->type()) {
                // focus in/out on spin just cause this whole widget to be repainted - therefore focus is drawn around whole widget...
                repaint();
                return true;
            } else if (QEvent::Paint==event->type()) {
                // ignore paint events...
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}
