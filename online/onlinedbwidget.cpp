/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "onlinedbwidget.h"
#include "widgets/itemview.h"
#include "widgets/menubutton.h"
#include "widgets/icons.h"
#include "support/action.h"
#include "support/localize.h"
#include "support/messagebox.h"
#include "support/configuration.h"
#include <QTimer>

OnlineDbWidget::OnlineDbWidget(OnlineDbService *s, QWidget *p)
    : SinglePageWidget(p)
    , configGroup(s->name())
    , srv(s)
{
    srv->setParent(this);
    view->setModel(s);
    view->alwaysShowHeader();
    Configuration config(configGroup);
    view->setMode(ItemView::Mode_DetailedTree);
    view->load(config);
    srv->load(config);
    MenuButton *menu=new MenuButton(this);
    menu->addAction(createViewMenu(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                           << ItemView::Mode_DetailedTree << ItemView::Mode_List));
    menu->addAction(createMenuGroup(i18n("Group By"), QList<MenuItem>() << MenuItem(i18n("Genre"), SqlLibraryModel::T_Genre)
                                                                        << MenuItem(i18n("Artist"), SqlLibraryModel::T_Artist),
                                    srv->topLevel(), this, SLOT(groupByChanged())));
    Action *configureAction=new Action(Icons::self()->configureIcon, i18n("Configure"), this);
    connect(configureAction, SIGNAL(triggered()), SLOT(configure()));
    menu->addAction(configureAction);
    init(ReplacePlayQueue|AddToPlayQueue|Refresh, QList<QWidget *>() << menu);
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(view, SIGNAL(updateToPlayQueue(QModelIndex,bool)), this, SLOT(updateToPlayQueue(QModelIndex,bool)));
    view->setOpenAfterSearch(SqlLibraryModel::T_Album!=srv->topLevel());
}

OnlineDbWidget::~OnlineDbWidget()
{
    Configuration config(configGroup);
    view->save(config);
    srv->save(config);
}

void OnlineDbWidget::groupByChanged()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (!act) {
        return;
    }
    int mode=act->property(constValProp).toInt();
    srv->setTopLevel((SqlLibraryModel::Type)mode);
    view->setOpenAfterSearch(SqlLibraryModel::T_Album!=srv->topLevel());
}

QStringList OnlineDbWidget::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }
    return srv->filenames(selected, allowPlaylists);
}

QList<Song> OnlineDbWidget::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return srv->songs(selected, allowPlaylists);
}

void OnlineDbWidget::showEvent(QShowEvent *e)
{
    SinglePageWidget::showEvent(e);
    if (srv->isDownloading() || srv->rowCount(QModelIndex())) {
        return;
    }
    if (srv->previouslyDownloaded()) {
        srv->open();
    } else {
        QTimer::singleShot(0, this, SLOT(firstTimePrompt()));
    }
}

void OnlineDbWidget::firstTimePrompt()
{
    if (MessageBox::No==MessageBox::questionYesNo(this, srv->averageSize()
                                                        ? i18n("The music listing needs to be downloaded, this can consume over %1Mb of disk space", srv->averageSize())
                                                        : i18n("Dowload music listing?"),
                                                   QString(), GuiItem(i18n("Download")), StdGuiItem::cancel())) {
        emit close();
    } else {
        srv->download(false);
    }
}

void OnlineDbWidget::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void OnlineDbWidget::doSearch()
{
    srv->search(view->searchText());
}

// TODO: Cancel download?
void OnlineDbWidget::refresh()
{
    if (!srv->isDownloading() && MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Re-download music listing?"), QString(), GuiItem(i18n("Download")), StdGuiItem::cancel())) {
        srv->download(true);
    }
}

void OnlineDbWidget::configure()
{
    srv->configure(this);
}

void OnlineDbWidget::updateToPlayQueue(const QModelIndex &idx, bool replace)
{
    QStringList files=srv->filenames(QModelIndexList() << idx, true);
    if (!files.isEmpty()) {
        emit add(files, replace, 0);
    }
}
