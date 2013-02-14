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

#include "gtkproxystyle.h"
#include <QComboBox>
#include <QToolBar>
#include <QApplication>

GtkProxyStyle::GtkProxyStyle()
    : QProxyStyle()
{
    setBaseStyle(qApp->style());
    toolbarCombo=new QComboBox(new QToolBar());
}

QSize GtkProxyStyle::sizeFromContents(ContentsType type, const QStyleOption *option,  const QSize &size, const QWidget *widget) const
{
    if (CT_ComboBox==type && widget && widget->parentWidget() && QString(widget->parentWidget()->metaObject()->className()).endsWith("Page")) {
        QSize orig=baseStyle()->sizeFromContents(type, option, size, widget);
        QSize other=baseStyle()->sizeFromContents(type, option, size, toolbarCombo);
        if (orig.height()>other.height()) {
            return QSize(orig.width(), other.height());
        }
        return orig;
    }
    return baseStyle()->sizeFromContents(type, option, size, widget);
}
