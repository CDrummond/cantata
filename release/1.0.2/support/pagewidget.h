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

#ifndef PAGEWIDGET_H
#define PAGEWIDGET_H

#include "icon.h"

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KPageWidget>
typedef KPageWidgetItem PageWidgetItem;

class PageWidget : public KPageWidget
{
public:
    PageWidget(QWidget *p) : KPageWidget(p) { }
    virtual ~PageWidget() { }
    PageWidgetItem * addPage(QWidget *widget, const QString &name, const Icon &icon, const QString &header) {
        PageWidgetItem *item=KPageWidget::addPage(widget, name);
        item->setIcon(icon);
        item->setHeader(header);
        return item;
    }
    void allPagesAdded() { }
};

#else
#include "fancytabwidget.h"
class PageWidgetItem : public QWidget
{
public:
    PageWidgetItem(QWidget *p, const QString &header, const Icon &icon, QWidget *cfg);
    virtual ~PageWidgetItem() { }
};

class PageWidget : public FancyTabWidget
{
public:
    PageWidget(QWidget *p);
    virtual ~PageWidget() { }
    PageWidgetItem * addPage(QWidget *widget, const QString &name, const Icon &icon, const QString &header);
    void allPagesAdded();
};

#endif

#endif
