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

#include "dynamicpage.h"
#include "dynamic.h"
#include "dynamicrulesdialog.h"
#include "localize.h"
#include "icons.h"
#include "action.h"
#include "actioncollection.h"
#include "mpdconnection.h"
#include "statuslabel.h"
#include "messagebox.h"

DynamicPage::DynamicPage(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    refreshAction = ActionCollection::get()->createAction("refreshdynamic", i18n("Refresh Dynamic Rules"), "view-refresh");
    addAction = ActionCollection::get()->createAction("adddynamic", i18n("Add Dynamic Rules"), "document-new");
    editAction = ActionCollection::get()->createAction("editdynamic", i18n("Edit Dynamic Rules"), Icons::self()->editIcon);
    removeAction = ActionCollection::get()->createAction("removedynamic", i18n("Remove Dynamic Rules"), "list-remove");
    toggleAction = new Action(this);

    refreshBtn->setDefaultAction(refreshAction);
    addBtn->setDefaultAction(addAction);
    editBtn->setDefaultAction(editAction);
    removeBtn->setDefaultAction(removeAction);
    startBtn->setDefaultAction(Dynamic::self()->startAct());
    stopBtn->setDefaultAction(Dynamic::self()->stopAct());

    view->addAction(editAction);
    view->addAction(removeAction);
    view->addAction(Dynamic::self()->startAct());

    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(toggle()));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(MPDConnection::self(), SIGNAL(dynamicUrl(const QString &)), this, SLOT(dynamicUrlChanged(const QString &)));
    connect(refreshAction, SIGNAL(triggered()), Dynamic::self(), SLOT(refreshList()));
    connect(addAction, SIGNAL(triggered()), SLOT(add()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(removeAction, SIGNAL(triggered()), SLOT(remove()));
    connect(Dynamic::self()->startAct(), SIGNAL(triggered()), SLOT(start()));
    connect(Dynamic::self()->stopAct(), SIGNAL(triggered()), SLOT(stop()));
    connect(toggleAction, SIGNAL(triggered()), SLOT(toggle()));
    connect(Dynamic::self(), SIGNAL(running(bool)), SLOT(running(bool)));
    connect(Dynamic::self(), SIGNAL(remoteRunning(bool)), SLOT(remoteRunning(bool)));
    connect(Dynamic::self(), SIGNAL(loadingList()), view, SLOT(showSpinner()));
    connect(Dynamic::self(), SIGNAL(loadedList()), view, SLOT(hideSpinner()));

    #ifdef Q_OS_WIN
    infoLabel->setType(StatusLabel::Error);
    enableWidgets(false);
    #else
    infoLabel->setVisible(false);
    refreshBtn->setVisible(false);
    #endif
    remoteRunningLabel->setType(StatusLabel::Error);
    remoteRunningLabel->setVisible(false);
    Dynamic::self()->stopAct()->setEnabled(false);
    proxy.setSourceModel(Dynamic::self());
    view->setTopText(i18n("Dynamic"));
    view->setModel(&proxy);
    view->hideBackButton();
    view->setDeleteAction(removeAction);
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
    Dynamic::self()->startAct()->setEnabled(1==selected.count());
    removeAction->setEnabled(selected.count());
}

void DynamicPage::dynamicUrlChanged(const QString &url)
{
    #ifdef Q_OS_WIN
    infoLabel->setVisible(url.isEmpty());
    enableWidgets(!url.isEmpty());
    #else
    refreshBtn->setVisible(!url.isEmpty());
    #endif
    remoteRunningLabel->setVisible(false);
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

    if (selected.isEmpty() ||
        MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected rules?\nThis cannot be undone."),
                                                 i18n("Remove Dynamic Rules"), StdGuiItem::remove(), StdGuiItem::cancel())) {
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
    Dynamic::self()->stopAct()->setEnabled(status);
}

void DynamicPage::remoteRunning(bool status)
{
    remoteRunningLabel->setVisible(!status);
    enableWidgets(status);
}

void DynamicPage::enableWidgets(bool enable)
{
    controls->setEnabled(enable);
    view->setEnabled(enable);
}
