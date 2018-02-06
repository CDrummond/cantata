/*
 * Cantata
 *
 * Copyright (c) 2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "localfolderpage.h"
#include "gui/stdactions.h"
#include "gui/customactions.h"
#include "models/playqueuemodel.h"
#include "widgets/menubutton.h"
#include "support/monoicon.h"
#include <QDesktopServices>

LocalFolderBrowsePage::LocalFolderBrowsePage(bool isHome, QWidget *p)
    : SinglePageWidget(p)
{
    model = isHome ? new LocalBrowseModel(QLatin1String("localbrowsehome"), tr("Home"), tr("Browse files in your home folder"), ":home.svg", this)
                   : new LocalBrowseModel(QLatin1String("localbrowseroot"), tr("Root"), tr("Browse files on your computer"), ":hdd.svg", this);
    proxy = new FileSystemProxyModel(model);
    browseAction = new Action(MonoIcon::icon(FontAwesome::folderopen, Utils::monoIconColor()), tr("Open In File Manager"), this);
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(browseAction, SIGNAL(triggered()), this, SLOT(openFileManager()));
    init(ReplacePlayQueue|AppendToPlayQueue);

//    view->addAction(CustomActions::self());
//    #ifdef TAGLIB_FOUND
//    view->addAction(StdActions::self()->editTagsAction);
//    #ifdef ENABLE_REPLAYGAIN_SUPPORT
//    view->addAction(StdActions::self()->replaygainAction);
//    #endif
//    #endif // TAGLIB_FOUND
    view->addAction(browseAction);
    view->setModel(proxy);
    view->closeSearch();
    view->alwaysShowHeader();
    view->view()->setRootIndex(proxy->mapFromSource(model->setRootPath(isHome ? QDir::homePath() : "/")));
}

LocalFolderBrowsePage::~LocalFolderBrowsePage()
{
    model->deleteLater();
    model=nullptr;
}

void LocalFolderBrowsePage::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void LocalFolderBrowsePage::openFileManager()
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(model->filePath(proxy->mapToSource(selected.at(0)))));
}

void LocalFolderBrowsePage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return; //doubleclick should only have one selected item
    }
    addSelectionToPlaylist();
}

void LocalFolderBrowsePage::addSelectionToPlaylist(const QString &name, int action, quint8 priority, bool decreasePriority)
{
    const QModelIndexList selected = view->selectedIndexes(true);
    QStringList paths;

    for (const auto &idx: selected) {
        paths.append(model->filePath(proxy->mapToSource(idx)));
    }
    PlayQueueModel::self()->load(paths, action, priority, decreasePriority);
}

void LocalFolderBrowsePage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enable=selected.count()>0;
//    bool trackSelected=false;
//    bool folderSelected=false;

//    for (const QModelIndex &idx: selected) {
//        if (model->fileInfo(proxy->mapToSource(idx)).isDir()) {
//            folderSelected=true;
//        } else {
//            trackSelected=true;
//        }
//    }
    StdActions::self()->enableAddToPlayQueue(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(false);

    CustomActions::self()->setEnabled(false);

//    bool fileActions = trackSelected && !folderSelected;
//    CustomActions::self()->setEnabled(fileActions);
//    #ifdef TAGLIB_FOUND
//    StdActions::self()->editTagsAction->setEnabled(fileActions);
//    #ifdef ENABLE_REPLAYGAIN_SUPPORT
//    StdActions::self()->replaygainAction->setEnabled(fileActions);
//    #endif
//    #endif // TAGLIB_FOUND
}
