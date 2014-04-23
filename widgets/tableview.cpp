/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "tableview.h"
#include "stretchheaderview.h"
#include "settings.h"
#include "localize.h"
#include "roles.h"
#include <QMenu>
#include <QAction>

TableView::TableView(const QString &cfgName, QWidget *parent, bool menuAlwaysAllowed)
    : TreeView(parent, menuAlwaysAllowed)
    , menu(0)
    , configName(cfgName)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    setDragDropOverwriteMode(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDropIndicatorShown(true);
    setUniformRowHeights(true);
    setAttribute(Qt::WA_MouseTracking, true);
    StretchHeaderView *hdr=new StretchHeaderView(Qt::Horizontal, this);
    setHeader(hdr);
    connect(hdr, SIGNAL(StretchEnabledChanged(bool)), SLOT(stretchToggled(bool)));
}

void TableView::initHeader()
{
    if (!model()) {
        return;
    }

    StretchHeaderView *hdr=qobject_cast<StretchHeaderView *>(header());
    QList<int> hideable;
    if (!menu) {
        hdr->SetStretchEnabled(true);
        stretchToggled(true);
        hdr->setContextMenuPolicy(Qt::CustomContextMenu);
        for (int i=0; i<model()->columnCount(); ++i) {
            hdr->SetColumnWidth(i, model()->headerData(i, Qt::Horizontal, Cantata::Role_Width).toDouble());
            if (model()->headerData(i, Qt::Horizontal, Cantata::Role_Hideable).toBool()) {
                hideable.append(i);
            }
        }
        #if QT_VERSION >= 0x050000
        hdr->setSectionsMovable(true);
        #else
        hdr->setMovable(true);
        #endif
        connect(hdr, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu()));
    }

    //Restore state
    QByteArray state=Settings::self()->headerState(configName);
    if (state.isEmpty()) {
        foreach (int i, hideable) {
            hdr->HideSection(i);
        }
    } else {
        hdr->RestoreState(state);
    }

    if (!menu) {
        menu = new QMenu(this);
        QAction *stretch=new QAction(i18n("Stretch Columns To Fit Window"), this);
        stretch->setCheckable(true);
        stretch->setChecked(hdr->is_stretch_enabled());
        connect(stretch, SIGNAL(toggled(bool)), hdr, SLOT(SetStretchEnabled(bool)));
        menu->addAction(stretch);
        menu->addSeparator();
        foreach (int col, hideable) {
            QAction *act=new QAction(model()->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString(), menu);
            act->setCheckable(true);
            act->setChecked(!hdr->isSectionHidden(col));
            menu->addAction(act);
            act->setData(col);
            connect(act, SIGNAL(toggled(bool)), this, SLOT(toggleHeaderItem(bool)));
        }
    }
}

void TableView::saveHeader()
{
    if (menu && model()) {
        Settings::self()->saveHeaderState(configName, qobject_cast<StretchHeaderView *>(header())->SaveState());
    }
}

void TableView::showMenu()
{
    menu->exec(QCursor::pos());
}

void TableView::toggleHeaderItem(bool visible)
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        int index=act->data().toInt();
        if (-1!=index) {
            qobject_cast<StretchHeaderView *>(header())->SetSectionHidden(index, !visible);
        }
    }
}

void TableView::stretchToggled(bool e)
{
    setHorizontalScrollBarPolicy(e ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}
