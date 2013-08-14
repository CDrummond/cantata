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

#include "togglelist.h"
#include "localize.h"
#include "icon.h"
#include "settings.h"
#include "basicitemdelegate.h"

ToggleList::ToggleList(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    connect(upButton, SIGNAL(clicked()), SLOT(moveUp()));
    connect(downButton, SIGNAL(clicked()), SLOT(moveDown()));
    connect(addButton, SIGNAL(clicked()), SLOT(add()));
    connect(removeButton, SIGNAL(clicked()), SLOT(remove()));
    connect(available, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(availableChanged(QListWidgetItem*)));
    connect(selected, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(selectedChanged(QListWidgetItem*)));
    upButton->setIcon(Icon("go-up"));
    downButton->setIcon(Icon("go-down"));
    addButton->setIcon(Icon("list-add"));
    removeButton->setIcon(Icon("list-remove"));

    upButton->setEnabled(false);
    downButton->setEnabled(false);
    addButton->setEnabled(false);
    removeButton->setEnabled(false);
    available->setAlternatingRowColors(false);
    available->setItemDelegate(new BasicItemDelegate(available));
    selected->setAlternatingRowColors(false);
    selected->setItemDelegate(new BasicItemDelegate(selected));
}

void ToggleList::moveUp()
{
    move(-1);
}

void ToggleList::moveDown()
{
    move(+1);
}

void ToggleList::add()
{
    int index=available->currentRow();
    if (index<0 || index>available->count()) {
        return;
    }
    QListWidgetItem *item = available->takeItem(index);
    QListWidgetItem *newItem=new QListWidgetItem(selected);
    newItem->setText(item->text());
    newItem->setData(Qt::UserRole, item->data(Qt::UserRole));
    delete item;
}

void ToggleList::remove()
{
    int index=selected->currentRow();
    if (index<0 || index>selected->count()) {
        return;
    }
    QListWidgetItem *item = selected->takeItem(index);
    QListWidgetItem *newItem=new QListWidgetItem(available);
    newItem->setText(item->text());
    newItem->setData(Qt::UserRole, item->data(Qt::UserRole));
    delete item;
}

void ToggleList::move(int d)
{
    const int row = selected->currentRow();
    QListWidgetItem *item = selected->takeItem(row);
    selected->insertItem(row + d, item);
    selected->setCurrentRow(row + d);
}

void ToggleList::availableChanged(QListWidgetItem *item)
{
    addButton->setEnabled(item);
}

void ToggleList::selectedChanged(QListWidgetItem *item)
{
    if (!item) {
        upButton->setEnabled(false);
        downButton->setEnabled(false);
        removeButton->setEnabled(false);
    } else {
        const int row = available->row(item);
        upButton->setEnabled(row != 0);
        downButton->setEnabled(row != available->count() - 1);
    }
    removeButton->setEnabled(item);
}
