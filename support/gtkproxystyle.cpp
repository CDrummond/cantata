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

#include "gtkproxystyle.h"
#include "gtkstyle.h"
#include "shortcuthandler.h"
#include "acceleratormanager.h"
#include <QSpinBox>
#include <QAbstractScrollArea>
#include <QAbstractItemView>
#include <QMenu>
#include <QToolBar>
#include <QApplication>
#include <QPainter>

static const char * constAccelProp="catata-accel";

static inline void addEventFilter(QObject *object, QObject *filter)
{
    object->removeEventFilter(filter);
    object->installEventFilter(filter);
}

GtkProxyStyle::GtkProxyStyle(int modView)
    : TouchProxyStyle(modView, true, true)
{
    shortcutHander=new ShortcutHandler(this);
    setBaseStyle(qApp->style());
}

GtkProxyStyle::~GtkProxyStyle()
{
}

int GtkProxyStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    switch (hint) {
    case SH_DialogButtonBox_ButtonsHaveIcons:
        return false;
    case SH_UnderlineShortcut:
        return widget ? shortcutHander->showShortcut(widget) : true;
    default:
        break;
    }

    return TouchProxyStyle::styleHint(hint, option, widget, returnData);
}

void GtkProxyStyle::polish(QWidget *widget)
{
    if (widget && qobject_cast<QMenu *>(widget) && !widget->property(constAccelProp).isValid()) {
        AcceleratorManager::manage(widget);
        widget->setProperty(constAccelProp, true);
    }
    TouchProxyStyle::polish(widget);
}

void GtkProxyStyle::polish(QPalette &pal)
{
    TouchProxyStyle::polish(pal);
}

void GtkProxyStyle::polish(QApplication *app)
{
    addEventFilter(app, shortcutHander);
    TouchProxyStyle::polish(app);
}

void GtkProxyStyle::unpolish(QWidget *widget)
{
    TouchProxyStyle::unpolish(widget);
}

void GtkProxyStyle::unpolish(QApplication *app)
{
    app->removeEventFilter(shortcutHander);
    TouchProxyStyle::unpolish(app);
}
