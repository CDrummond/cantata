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

#ifndef PAGEWIDGET_H
#define PAGEWIDGET_H

#include "icon.h"
#include <QWidget>
#include <QMap>

class QListWidget;
class QListWidgetItem;
class QStackedWidget;

class PageWidgetItem : public QWidget
{
public:
    PageWidgetItem(QWidget *p, const QString &header, const QIcon &icon, QWidget *cfg, bool showHeader);
    ~PageWidgetItem() override { }
    QWidget * widget() const { return wid; }

private:
    QWidget *wid;
};

class PageWidget : public QWidget
{
    Q_OBJECT

public:
    PageWidget(QWidget *p, bool listView=false, bool headers=true);
    ~PageWidget() override { }
    PageWidgetItem * addPage(QWidget *widget, const QString &name, const QIcon &icon, const QString &header);
    int count();
    PageWidgetItem * currentPage() const;
    void setCurrentPage(PageWidgetItem *item);

public Q_SLOTS:
    void setFocus();

Q_SIGNALS:
    void currentPageChanged();

private:
    bool showHeaders;
    QListWidget *list;
    QStackedWidget *stack;
    QMap<QListWidgetItem *, PageWidgetItem*> pages;
};

#endif
