/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef PROXYSTYLE_H
#define PROXYSTYLE_H

#include <QProxyStyle>
#include <QIcon>
#include <QMap>

class ProxyStyle : public QProxyStyle
{
public:
    static const char * constModifyFrameProp;

    enum FrameMod {
        VF_None = 0x00,
        VF_Side = 0x01,
        VF_Top  = 0x02
    };

    ProxyStyle(int modView=VF_None);
    ~ProxyStyle() override { }
    void setModifyViewFrame(int modView) { modViewFrame=modView; }
    int modifyViewFrame() const { return modViewFrame; }
    void polish(QPalette &pal) override { QProxyStyle::polish(pal); }
    void polish(QApplication *app) override { QProxyStyle::polish(app); }
    void polish(QWidget *widget) override;
    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override;
    QPixmap standardPixmap(StandardPixmap sp, const QStyleOption *opt, const QWidget *widget) const override;
    QIcon standardIcon(StandardPixmap sp, const QStyleOption *opt, const QWidget *widget) const override;

private:
    int modViewFrame;
    QMap<StandardPixmap, QIcon> icons;
};

#endif
