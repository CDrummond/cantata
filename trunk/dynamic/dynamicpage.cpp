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
#include "localize.h"
#include "icon.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#else
#include <QtGui/QAction>
#endif

DynamicPage::DynamicPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    refreshAction = p->createAction("refreshdynamic", i18n("Refresh Dynamic Rules"), "view-refresh");
    addAction = p->createAction("adddynamic", i18n("Add Dynamic Rules"), "list-add");
    editAction = p->createAction("editdynamic", i18n("Edit Dynamic Rules"), "document-edit");
    removeAction = p->createAction("removedynamic", i18n("Remove Dynamic Rules"), "list-remove");
    startAction = p->createAction("startdynamic", i18n("Start Dynamic Mode"), "media-playback-start");
    stopAction = p->createAction("stopdynamic", i18n("Stop Dynamic Mode"), "process-stop");
    toggleAction = new Action(this);

    Icon::init(refreshBtn);
    Icon::init(addBtn);
    Icon::init(editBtn);
    Icon::init(removeBtn);
    Icon::init(startBtn);
    Icon::init(stopBtn);
    refreshBtn->setDefaultAction(refreshAction);
    addBtn->setDefaultAction(addAction);
    editBtn->setDefaultAction(editAction);
    removeBtn->setDefaultAction(removeAction);
    startBtn->setDefaultAction(startAction);
    stopBtn->setDefaultAction(stopAction);

    view->addAction(editAction);
    view->addAction(removeAction);
    view->addAction(startAction);

    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(toggle()));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(MPDConnection::self(), SIGNAL(dynamicUrl(const QString &)), this, SLOT(dynamicUrlChanged(const QString &)));
    connect(refreshAction, SIGNAL(triggered()), Dynamic::self(), SLOT(refreshList()));
    connect(addAction, SIGNAL(triggered()), SLOT(add()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(removeAction, SIGNAL(triggered()), SLOT(remove()));
    connect(startAction, SIGNAL(triggered()), SLOT(start()));
    connect(stopAction, SIGNAL(triggered()), SLOT(stop()));
    connect(toggleAction, SIGNAL(triggered()), SLOT(toggle()));
    connect(Dynamic::self(), SIGNAL(running(bool)), SLOT(running(bool)));
    connect(Dynamic::self(), SIGNAL(modelReset()), SLOT(controlActions()));

    #ifdef Q_OS_WIN
    infoMsg->setVisible(true);
    setEnabled(false);
    #else
    infoMsg->setVisible(false);
    refreshBtn->setVisible(false);
    #endif
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
    QModelIndexList selected=qobject_cast<Dynamic *>(sender()) ? QModelIndexList() : view->selectedIndexes();

    editAction->setEnabled(1==selected.count());
    startAction->setEnabled(1==selected.count());
    removeAction->setEnabled(selected.count());
}

void DynamicPage::dynamicUrlChanged(const QString &url)
{
    #ifdef Q_OS_WIN
    infoMsg->setVisible(url.isEmpty());
    setEnabled(!url.isEmpty());
    #else
    refreshBtn->setVisible(!url.isEmpty());
    #endif
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
