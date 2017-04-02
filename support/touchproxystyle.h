/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef TOUCHPROXYSTYLE_H
#define TOUCHPROXYSTYLE_H

#include "proxystyle.h"

class TouchProxyStyle : public ProxyStyle
{
public:
    static double constScaleFactor;
    enum SbType {
        SB_Standard,
        SB_Thin,
        SB_Gtk
    };

    TouchProxyStyle(int modView=0, bool touchSpin=true, bool gtkOverlayStyleScrollbar=false);
    ~TouchProxyStyle();
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,  const QSize &size, const QWidget *widget) const;
    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const;
    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void polish(QWidget *widget);
    void polish(QPalette &pal) { ProxyStyle::polish(pal); }
    void polish(QApplication *app) { ProxyStyle::polish(app); }

protected:
    bool touchStyleSpin;
    double spinButtonRatio;
    SbType sbarType;
    int sbarPlainViewWidth;
};

#endif
