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

#include "dynamicpage.h"
#include "dynamic.h"
#include "dynamicrulesdialog.h"
#include "mainwindow.h"
#include "localize.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KActionCollection>
#include <KDE/KGlobalSettings>
#else
#include <QtGui/QAction>
#endif

DynamicPage::DynamicPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    addAction = p->actionCollection()->addAction("adddynamic");
    addAction->setText(i18n("Add Dynamic Rules"));
    editAction = p->actionCollection()->addAction("editdynamic");
    editAction->setText(i18n("Edit Dynamic Rules"));
    removeAction = p->actionCollection()->addAction("removedynamic");
    removeAction->setText(i18n("Remove Dynamic Rules"));
    startAction = p->actionCollection()->addAction("startdynamic");
    startAction->setText(i18n("Start Dynamic Mode"));
    stopAction = p->actionCollection()->addAction("stopdynamic");
    stopAction->setText(i18n("Stop Dynamic Mode"));
    #else
    addAction = new QAction(tr("Add Dynamic Rules"), this);
    editAction = new QAction(tr("Edit Dynamic Rules"), this);
    removeAction = new QAction(tr("Remove Dynamic Rules"), this);
    startAction = new QAction(tr("Start Dynamic Mode"), this);
    stopAction = new QAction(tr("Stop Dynamic Mode"), this);
    #endif
    toggleAction = new QAction(this);
    addAction->setIcon(QIcon::fromTheme("list-add"));
    editAction->setIcon(QIcon::fromTheme("document-edit"));
    removeAction->setIcon(QIcon::fromTheme("list-remove"));
    startAction->setIcon(QIcon::fromTheme("media-playback-start"));
    stopAction->setIcon(QIcon::fromTheme("process-stop"));

    MainWindow::initButton(addBtn);
    MainWindow::initButton(editBtn);
    MainWindow::initButton(removeBtn);
    MainWindow::initButton(startBtn);
    MainWindow::initButton(stopBtn);
    addBtn->setDefaultAction(addAction);
    editBtn->setDefaultAction(editAction);
    removeBtn->setDefaultAction(removeAction);
    startBtn->setDefaultAction(startAction);
    stopBtn->setDefaultAction(stopAction);

    view->addAction(editAction);
    view->addAction(removeAction);
    view->addAction(startAction);

    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(addAction, SIGNAL(triggered()), SLOT(add()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(removeAction, SIGNAL(triggered()), SLOT(remove()));
    connect(startAction, SIGNAL(triggered()), SLOT(start()));
    connect(stopAction, SIGNAL(triggered()), SLOT(stop()));
    connect(toggleAction, SIGNAL(triggered()), SLOT(toggle()));
    connect(Dynamic::self(), SIGNAL(running(bool)), SLOT(running(bool)));

    stopAction->setEnabled(false);
    proxy.setSourceModel(Dynamic::self());
    view->setTopText(i18n("Dynamic"));
    view->setModel(&proxy);
    view->hideBackButton();
    view->setDeleteAction(removeAction);
    view->init(0, 0, toggleAction);
    view->setMode(ItemView::Mode_List);
    controlActions();
}

DynamicPage::~DynamicPage()
{
}

void DynamicPage::itemDoubleClicked(const QModelIndex &)
{
//     const QModelIndexList selected = view->selectedIndexes();
//     if (1!=selected.size()) {
//         return; //doubleclick should only have one selected item
//     }
//     MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
//     if (MusicLibraryItem::Type_Song==item->itemType()) {
//         addSelectionToPlaylist();
//     }
}

void DynamicPage::searchItems()
{
    QString text=view->searchText().trimmed();
    proxy.update(text);
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll();
    }
}

void DynamicPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();

    editAction->setEnabled(1==selected.count());
    startAction->setEnabled(1==selected.count());
    removeAction->setEnabled(selected.count());
}

void DynamicPage::add()
{
    DynamicRulesDialog *dlg=new DynamicRulesDialog(this);
    dlg->edit(QString());
}

void DynamicPage::edit()
{
    QModelIndexList selected=view->selectedIndexes();

    if (1!=selected.count()) {
        return;
    }

    DynamicRulesDialog *dlg=new DynamicRulesDialog(this);
    dlg->edit(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPage::remove()
{
    QModelIndexList selected=view->selectedIndexes();

    if (selected.isEmpty()) {
        return;
    }
    QStringList names;
    foreach (const QModelIndex &idx, selected) {
        names.append(idx.data(Qt::DisplayRole).toString());
    }

    foreach (const QString &name, names) {
        Dynamic::self()->del(name);
    }
}

void DynamicPage::start()
{
    QModelIndexList selected=view->selectedIndexes();

    if (1!=selected.count()) {
        return;
    }
    Dynamic::self()->start(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPage::stop()
{
    Dynamic::self()->stop();
}

void DynamicPage::toggle()
{
    QModelIndexList selected=view->selectedIndexes();

    if (1!=selected.count()) {
        return;
    }

    Dynamic::self()->toggle(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPage::running(bool status)
{
    stopAction->setEnabled(status);
}
