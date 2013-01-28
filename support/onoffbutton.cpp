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

#ifndef Q_OS_WIN

#include "onoffbutton.h"
#include "localize.h"
#include "gtkstyle.h"
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

static QString onText;
static QString offText;
static QSize fixedSize;
static const int constBorderSize=4;

OnOffButton::OnOffButton(QWidget *p)
    : QCheckBox(p)
{
    if (GtkStyle::mimicWidgets()) {
        QFont f(font());
        f.setPointSize(f.pointSize()*0.9);
        setFont(f);
        if (onText.isEmpty()) {
            onText=i18n("ON");
            offText=i18n("OFF");
            QFontMetrics fm(f);
            int onWidth=fm.width(onText);
            int offWidth=fm.width(offText);
            int fontHeight=fm.height();
            fixedSize=QSize((qMax(onWidth, offWidth)+(constBorderSize*4))*2, fontHeight+(2*constBorderSize));
        }
        setCheckable(true);
        setFixedSize(fixedSize);
    }
}

QSize OnOffButton::sizeHint() const
{
    return GtkStyle::mimicWidgets() ? fixedSize : QCheckBox::sizeHint();
}

bool OnOffButton::hitButton(const QPoint &pos) const
{
    return GtkStyle::mimicWidgets() ? true : QCheckBox::hitButton(pos);
}

static QPainterPath buildPath(const QRectF &r, double radius)
{
    QPainterPath path;
    double diameter(radius*2);

    path.moveTo(r.x()+r.width(), r.y()+r.height()-radius);
    path.arcTo(r.x()+r.width()-diameter, r.y(), diameter, diameter, 0, 90);
    path.arcTo(r.x(), r.y(), diameter, diameter, 90, 90);
    path.arcTo(r.x(), r.y()+r.height()-diameter, diameter, diameter, 180, 90);
    path.arcTo(r.x()+r.width()-diameter, r.y()+r.height()-diameter, diameter, diameter, 270, 90);
    return path;
}

void OnOffButton::paintEvent(QPaintEvent *e)
{
    if (!GtkStyle::mimicWidgets()) {
        QCheckBox::paintEvent(e);
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette &pal=palette();
    bool isOn=isChecked();
    bool isActive=isEnabled() && QPalette::Active==pal.currentColorGroup();
    QRect orig=rect();
    QRectF r(orig.x()+1.5, orig.y()+1.5, orig.width()-3, orig.height()-3); //.adjusted(1.5, 1.5, -2, -2);
    QRectF borderRect=r.adjusted(1, 1, -1, -1);
    //QRect borderInnder=borderRect.adjusted(1, 1, -1, -1);
    QRectF onRect=QRectF(borderRect.x(), borderRect.y(), borderRect.width()/2, borderRect.height());
    QRectF offRect=QRectF(borderRect.x()+(borderRect.width()/2), borderRect.y(), borderRect.width()/2, borderRect.height());
    QPainterPath border=buildPath(borderRect, 3.0);
    //QPainterPath inner=buildPath(borderInnder, 2.0);
    QPainterPath slider=buildPath(isOn ? offRect : onRect, 3.0);
    QPainterPath sliderInner=buildPath((isOn ? offRect : onRect).adjusted(1, 1, -1, -1), 2.0);
    QLinearGradient grad(borderRect.topLeft(), borderRect.bottomLeft());
    bool active=isOn && isActive;
    QColor bgndCol=active ? pal.highlight().color() : pal.mid().color();
    grad.setColorAt(0, active ? bgndCol.darker(110) : bgndCol.lighter(110));
    grad.setColorAt(1, active ? bgndCol.darker(102) : bgndCol.lighter(120));
    p.fillPath(border, grad);
    QColor borderCol(bgndCol.darker(active ? 145 : 110));
    p.setPen(borderCol);
    p.drawPath(border);
    //p.setPen(pal.light().color());
    //p.drawText((isOn ? onRect : offRect).adjusted(1, 1, 1, 1), isOn ? onText : offText, QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    QColor textcol(isOn && isActive ? pal.highlightedText().color() : pal.text().color());
    if (!isOn && textcol.red()<64 && textcol.green()<64 && textcol.blue()<64) {
        QColor col=Qt::white;
        col.setAlphaF(0.4);
	    p.setPen(col);
    	p.drawText(offRect.adjusted(0, 1, 0, 1), offText, QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    } else if (isOn && (isActive ? (textcol.red()>196 && textcol.green()>196 && textcol.blue()>196)
                                 : (textcol.red()<64 && textcol.green()<64 && textcol.blue()<64))) {
        QColor col=isActive ? bgndCol.darker(110) : Qt::white;
        col.setAlphaF(isActive ? 0.9 : 0.4);
	    p.setPen(col);
    	p.drawText(isActive ? onRect.adjusted(0, -1, 0, -1) : onRect.adjusted(0, 1, 0, 1), onText, QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    }
    p.setPen(textcol);
    p.drawText(isOn ? onRect : offRect, isOn ? onText : offText, QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));

    grad.setColorAt(0, pal.mid().color().lighter(150));
    grad.setColorAt(1, pal.mid().color().darker(82));
    p.fillPath(slider, grad);
    QColor sliderCol(pal.mid().color().lighter(170));
    sliderCol.setAlphaF(0.2);
    p.setPen(sliderCol);
    p.drawPath(sliderInner);
    p.setPen(borderCol);
    p.drawPath(slider);

    if (hasFocus()) {
        p.setPen(pal.highlight().color());
        p.drawPath(buildPath(QRectF(orig.x()+0.5, orig.y()+0.5, orig.width()-1, orig.height()-1), 4.0));
    }
}

#endif
