/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "synccollectionwidget.h"
#include "treeview.h"
#include "musiclibraryproxymodel.h"
#include "icon.h"
#include <QtGui/QAction>

SyncCollectionWidget::SyncCollectionWidget(QWidget *parent, const QString &title, const QString &action)
    : QWidget(parent)
{
    setupUi(this);
    groupBox->setTitle(title);
    button->setText(action);
    button->setEnabled(false);

    model=new MusicLibraryModel(this);
    proxy=new MusicLibraryProxyModel(this);
    proxy->setSourceModel(model);
    tree->setModel(proxy);
    tree->setPageDefaults();
    connect(tree, SIGNAL(itemsSelected(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), SLOT(copySongs()));

    QAction *act=new QAction(action, this);
    connect(act, SIGNAL(triggered(bool)), SLOT(copySongs()));
    tree->addAction(act);
}

SyncCollectionWidget::~SyncCollectionWidget()
{
}

void SyncCollectionWidget::copySongs()
{
    const QModelIndexList selected = tree->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy->mapToSource(idx));
    }

    emit copy(model->songs(mapped));
}
