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
#include "gui/settings.h"
#include "support/localize.h"
#include "models/roles.h"
#include <QMenu>
#include <QAction>
#include <QActionGroup>

TableView::TableView(const QString &cfgName, QWidget *parent, bool menuAlwaysAllowed)
    : TreeView(parent, menuAlwaysAllowed)
    , menu(0)
    , configName(cfgName)
    , menuIsForCol(-1)
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
    QList<int> initiallyHidden;
    if (!menu) {
        hdr->SetStretchEnabled(true);
        stretchToggled(true);
        hdr->setContextMenuPolicy(Qt::CustomContextMenu);
        for (int i=0; i<model()->columnCount(); ++i) {
            hdr->SetColumnWidth(i, model()->headerData(i, Qt::Horizontal, Cantata::Role_Width).toDouble());
            if (model()->headerData(i, Qt::Horizontal, Cantata::Role_Hideable).toBool()) {
                hideable.append(i);
            }
            if (model()->headerData(i, Qt::Horizontal, Cantata::Role_InitiallyHidden).toBool()) {
                initiallyHidden.append(i);
            }
        }
        #if QT_VERSION >= 0x050000
        hdr->setSectionsMovable(true);
        #else
        hdr->setMovable(true);
        #endif
        connect(hdr, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)));
    }

    //Restore state
    QByteArray state=Settings::self()->headerState(configName);
    if (state.isEmpty()) {
        foreach (int i, initiallyHidden) {
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

        QMenu *alignmentMenu = new QMenu(menu);
        QActionGroup *alignGroup = new QActionGroup(alignmentMenu);
        alignLeftAction = new QAction(i18n("Left"), alignGroup);
        alignCenterAction = new QAction(i18n("Center"), alignGroup);
        alignRightAction = new QAction(i18n("Right"), alignGroup);

        alignLeftAction->setCheckable(true);
        alignCenterAction->setCheckable(true);
        alignRightAction->setCheckable(true);
        alignmentMenu->addActions(alignGroup->actions());
        connect(alignGroup, SIGNAL(triggered(QAction*)), SLOT(alignmentChanged()));
        alignAction=new QAction(i18n("Alignment"), menu);
        alignAction->setMenu(alignmentMenu);
        menu->addAction(alignAction);

        menu->addSeparator();
        foreach (int col, hideable) {
            QAction *act=new QAction(model()->headerData(col, Qt::Horizontal, Cantata::Role_ContextMenuText).toString(), menu);
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

void TableView::showMenu(const QPoint &pos)
{
    menuIsForCol=header()->logicalIndexAt(pos);
    alignAction->setEnabled(-1!=menuIsForCol);
    if (-1!=menuIsForCol) {
        Qt::Alignment al = (Qt::AlignmentFlag)model()->headerData(menuIsForCol, Qt::Horizontal, Qt::TextAlignmentRole).toInt();
        if (al&Qt::AlignLeft) {
            alignLeftAction->setChecked(true);
        } else if (al&Qt::AlignHCenter) {
            alignCenterAction->setChecked(true);
        } else if (al&Qt::AlignRight) {
            alignRightAction->setChecked(true);
        }
    }
    menu->exec(mapToGlobal(pos));
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

void TableView::alignmentChanged()
{
    if (-1!=menuIsForCol) {
        int al=alignLeftAction->isChecked()
                ? Qt::AlignLeft
                : alignRightAction->isChecked()
                    ? Qt::AlignRight
                    : Qt::AlignHCenter;
        if (model()->setHeaderData(menuIsForCol, Qt::Horizontal, al, Qt::TextAlignmentRole)) {
            header()->reset();
        }
    }
}

