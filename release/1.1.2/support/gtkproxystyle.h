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

#ifndef GTKPROXYSTYLE_H
#define GTKPROXYSTYLE_H

#include <QProxyStyle>
#include "config.h"

class QComboBox;
#ifdef ENABLE_OVERLAYSCROLLBARS
class QScrollBar;
class OsThumb;
#endif
class ShortcutHandler;

class GtkProxyStyle : public QProxyStyle
{
    Q_OBJECT

public:
    static const char * constSlimComboProperty;

    enum ScrollbarType {
        SB_Standard,
        SB_Thin
        #ifdef ENABLE_OVERLAYSCROLLBARS
        , SB_Overlay
        #endif
    };

    GtkProxyStyle(ScrollbarType sb);
    ~GtkProxyStyle();
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,  const QSize &size, const QWidget *widget) const;
    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const;
    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    #ifdef ENABLE_OVERLAYSCROLLBARS
    bool eventFilter(QObject *object, QEvent *event);
    void destroySliderThumb();
    #endif

    void polish(QWidget *widget);
    void polish(QPalette &pal);
    void polish(QApplication *app);
    void unpolish(QWidget *widget);
    void unpolish(QApplication *app);

#ifdef ENABLE_OVERLAYSCROLLBARS
private Q_SLOTS:
    void objectDestroyed(QObject *);
    void sbarThumbMoved(const QPoint &point);
    void sbarPageUp();
    void sbarPageDown();
    void sbarThumbHiding();
    void sbarThumbShowing();

private:
    QRect sbarGetSliderRect() const;
    void sbarUpdateOffset();
    bool usePlainScrollbars(const QWidget *widget) const;
#endif

private:
    ShortcutHandler *shortcutHander;
    QComboBox *toolbarCombo;

    ScrollbarType sbarType;
    int sbarPlainViewWidth;

    #ifdef ENABLE_OVERLAYSCROLLBARS
    OsThumb *sbarThumb;
    int sbarWidth;
    int sbarAreaWidth;
    int sbarOffset;
    int sbarLastPos;
    QScrollBar *sbarThumbTarget;
    #endif
};

#endif
