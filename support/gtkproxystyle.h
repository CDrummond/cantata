/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QMap>
#include "config.h"
#include "touchproxystyle.h"

class ShortcutHandler;

class GtkProxyStyle : public TouchProxyStyle
{
public:
    GtkProxyStyle(int modView, bool thinSb, bool styleSpin, const QMap<QString, QString> &c);
    ~GtkProxyStyle();
    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const;

    void polish(QWidget *widget);
    void polish(QPalette &pal);
    void polish(QApplication *app);
    void unpolish(QWidget *widget);
    void unpolish(QApplication *app);

private:
    ShortcutHandler *shortcutHander;
    QMap<QString, QString> css;
};

#endif
