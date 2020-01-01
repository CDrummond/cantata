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

#include "dynamicplaylistspage.h"
#include "dynamicplaylists.h"
#include "playlistrulesdialog.h"
#include "widgets/icons.h"
#include "support/action.h"
#include "support/configuration.h"
#include "support/monoicon.h"
#include "mpd-interface/mpdconnection.h"
#include "support/messagebox.h"
#include "gui/stdactions.h"

DynamicPlaylistsPage::DynamicPlaylistsPage(QWidget *p)
    : SinglePageWidget(p)
{
    addAction = new Action(Icons::self()->addNewItemIcon, tr("Add"), this);
    editAction = new Action(Icons::self()->editIcon, tr("Edit"), this);
    removeAction = new Action(Icons::self()->removeIcon, tr("Remove"), this);
    toggleAction = new Action(this);

    ToolButton *addBtn=new ToolButton(this);
    ToolButton *editBtn=new ToolButton(this);
    ToolButton *removeBtn=new ToolButton(this);
    ToolButton *startBtn=new ToolButton(this);

    addBtn->setDefaultAction(addAction);
    editBtn->setDefaultAction(editAction);
    removeBtn->setDefaultAction(removeAction);
    startBtn->setDefaultAction(DynamicPlaylists::self()->startAct());

    view->addAction(editAction);
    view->addAction(removeAction);
    view->addAction(DynamicPlaylists::self()->startAct());
    view->alwaysShowHeader();

    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(toggle()));
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(MPDConnection::self(), SIGNAL(dynamicSupport(bool)), this, SLOT(remoteDynamicSupport(bool)));
    connect(addAction, SIGNAL(triggered()), SLOT(add()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(removeAction, SIGNAL(triggered()), SLOT(remove()));
    connect(DynamicPlaylists::self()->startAct(), SIGNAL(triggered()), SLOT(start()));
    connect(DynamicPlaylists::self()->stopAct(), SIGNAL(triggered()), SLOT(stop()));
    connect(toggleAction, SIGNAL(triggered()), SLOT(toggle()));
    connect(DynamicPlaylists::self(), SIGNAL(running(bool)), SLOT(running(bool)));
    connect(DynamicPlaylists::self(), SIGNAL(loadingList()), view, SLOT(showSpinner()));
    connect(DynamicPlaylists::self(), SIGNAL(loadedList()), view, SLOT(hideSpinner()));

    DynamicPlaylists::self()->stopAct()->setEnabled(false);
    proxy.setSourceModel(DynamicPlaylists::self());
    view->setModel(&proxy);
    view->setDeleteAction(removeAction);
    view->setMode(ItemView::Mode_List);
    controlActions();
    Configuration config(metaObject()->className());
    view->load(config);
    controls=QList<QWidget *>() << addBtn << editBtn << removeBtn << startBtn;
    init(0, QList<QWidget *>(), controls);
    #ifdef Q_OS_WIN
    remoteDynamicSupport(false);
    #endif
    #ifndef Q_OS_WIN
    QString
    #endif
    infoStr = tr("A 'dynamic' playlist contains a set of rules to select tracks from your music library to play. "
                 "The play queue is filled with random tracks matching your criteria, and the play queue will be "
                 "dynamically updated after tracks are played.")
              +QLatin1String("\n\n\n")+
              tr("Use the + icon (below) to create a new 'dynamic' playlist.");
    view->setInfoText(infoStr);
}

DynamicPlaylistsPage::~DynamicPlaylistsPage()
{
    Configuration config(metaObject()->className());
    view->save(config);
}

void DynamicPlaylistsPage::doSearch()
{
    QString text=view->searchText().trimmed();
    proxy.update(text);
    if (proxy.enabled() && !proxy.filterText().isEmpty()) {
        view->expandAll();
    }
}

void DynamicPlaylistsPage::controlActions()
{
    QModelIndexList selected=qobject_cast<RulesPlaylists *>(sender()) ? QModelIndexList() : view->selectedIndexes(false); // Dont need sorted selection here...

    editAction->setEnabled(1==selected.count());
    DynamicPlaylists::self()->startAct()->setEnabled(1==selected.count());
    removeAction->setEnabled(selected.count());
}

void DynamicPlaylistsPage::remoteDynamicSupport(bool s)
{
    #ifdef Q_OS_WIN
    if (s) {
        view->setInfoText(infoStr);
    } else {
        view->setInfoText(tr("Remote dynamizer is not running. In order to support 'dynamic' playlists under Windows, Cantata requires "
                             "its 'cantata-dynamic' script to be running on the MPD server."));
    }
    enableWidgets(s);
    #endif
    if (s && server.isNull()) {
        server = MonoIcon::icon(FontAwesome::server, Utils::monoIconColor());
    }
    view->setBackgroundImage(s ? server : QIcon());
}

void DynamicPlaylistsPage::add()
{
    PlaylistRulesDialog *dlg=new PlaylistRulesDialog(this, DynamicPlaylists::self());
    dlg->edit(QString());
}

void DynamicPlaylistsPage::edit()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }

    PlaylistRulesDialog *dlg=new PlaylistRulesDialog(this, DynamicPlaylists::self());
    dlg->edit(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPlaylistsPage::remove()
{
    QModelIndexList selected=view->selectedIndexes();

    if (selected.isEmpty() ||
        MessageBox::No==MessageBox::warningYesNo(this, tr("Are you sure you wish to remove the selected rules?\n\nThis cannot be undone."),
                                                 tr("Remove Dynamic Rules"), StdGuiItem::remove(), StdGuiItem::cancel())) {
        return;
    }

    QStringList names;
    for (const QModelIndex &idx: selected) {
        names.append(idx.data(Qt::DisplayRole).toString());
    }

    for (const QString &name: names) {
        DynamicPlaylists::self()->del(name);
    }
}

void DynamicPlaylistsPage::start()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }
    DynamicPlaylists::self()->start(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPlaylistsPage::stop()
{
    DynamicPlaylists::self()->stop();
}

void DynamicPlaylistsPage::toggle()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }

    DynamicPlaylists::self()->toggle(selected.at(0).data(Qt::DisplayRole).toString());
}

void DynamicPlaylistsPage::running(bool status)
{
    DynamicPlaylists::self()->stopAct()->setEnabled(status);
}

void DynamicPlaylistsPage::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void DynamicPlaylistsPage::enableWidgets(bool enable)
{
    for (QWidget *c: controls) {
        c->setEnabled(enable);
    }

    view->setEnabled(enable);
}

void DynamicPlaylistsPage::showEvent(QShowEvent *e)
{
    view->focusView();
    DynamicPlaylists::self()->enableRemotePolling(true);
    SinglePageWidget::showEvent(e);
}

void DynamicPlaylistsPage::hideEvent(QHideEvent *e)
{
    DynamicPlaylists::self()->enableRemotePolling(false);
    SinglePageWidget::hideEvent(e);
}

#include "moc_dynamicplaylistspage.cpp"
