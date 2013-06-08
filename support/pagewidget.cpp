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

#include "pagewidget.h"
#include "icon.h"
#include <QBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QApplication>

PageWidgetItem::PageWidgetItem(QWidget *p, const QString &header, const Icon &icon, QWidget *cfg)
    : QWidget(p) {
    static int size=-1;

    if (-1==size) {
        size=QApplication::fontMetrics().height();
        if (size>20) {
            size=Icon::stdSize(size*1.25);
        } else {
            size=22;
        }
    }

    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    QBoxLayout *titleLayout=new QBoxLayout(QBoxLayout::LeftToRight, 0);
    titleLayout->addWidget(new QLabel("<b>"+header+"</b>", this));
    titleLayout->addItem(new QSpacerItem(16, 16, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QLabel *icn=new QLabel(this);
    icn->setPixmap(icon.pixmap(size, size));
    titleLayout->addWidget(icn);
    layout->addLayout(titleLayout);
    layout->addItem(new QSpacerItem(8, 8, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(cfg);
    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(cfg->sizePolicy().hasHeightForWidth());
    cfg->setSizePolicy(sizePolicy);
    cfg->setParent(this);
    adjustSize();
}

PageWidget::PageWidget(QWidget *p)
    : FancyTabWidget(p, false, true)
{
}

PageWidgetItem * PageWidget::addPage(QWidget *widget, const QString &name, const Icon &icon, const QString &header)
{
    PageWidgetItem *item=new PageWidgetItem(parentWidget(), header, icon, widget);
    AddTab(item, icon, name);
    return item;
}

void PageWidget::allPagesAdded()
{
    SetMode(FancyTabWidget::Mode_LargeSidebar);
}
