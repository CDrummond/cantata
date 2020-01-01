/*
 * Cantata
 *
 * Copyright (c) 2018-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/configuration.h"
#include "support/utils.h"
#ifdef TAGLIB_FOUND
#include "tags/tags.h"
#endif
#include <QDesktopServices>

LocalFolderBrowsePage::LocalFolderBrowsePage(bool isHome, QWidget *p)
    : SinglePageWidget(p)
    , isHomeFolder(isHome)
{
    QColor col = Utils::monoIconColor();
    model = isHomeFolder
            ? new LocalBrowseModel(QLatin1String("localbrowsehome"), tr("Home"), tr("Browse files in your home folder"), MonoIcon::icon(FontAwesome::home, col), this)
            : new LocalBrowseModel(QLatin1String("localbrowseroot"), tr("Root"), tr("Browse files on your computer"), MonoIcon::icon(FontAwesome::hddo, col), this);
    proxy = new FileSystemProxyModel(model);
    browseAction = new Action(MonoIcon::icon(FontAwesome::folderopen, col), tr("Open In File Manager"), this);
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(browseAction, SIGNAL(triggered()), this, SLOT(openFileManager()));
    view->setModel(proxy);
    Configuration config(configGroup());
    view->load(config);
    MenuButton *menu=new MenuButton(this);
    menu->addActions(createViewActions(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                               << ItemView::Mode_DetailedTree));
    init(ReplacePlayQueue|AppendToPlayQueue, QList<QWidget *>() << menu);

    view->addAction(CustomActions::self());
    #ifdef TAGLIB_FOUND
    view->addAction(StdActions::self()->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(StdActions::self()->replaygainAction);
    #endif
    #endif // TAGLIB_FOUND
    view->addAction(browseAction);
    view->closeSearch();
    view->alwaysShowHeader();
    setView(view->viewMode());
}

LocalFolderBrowsePage::~LocalFolderBrowsePage()
{
    Configuration config(configGroup());
    view->save(config);
    model->deleteLater();
    model=nullptr;
}

QString LocalFolderBrowsePage::configGroup() const
{
    return isHomeFolder ? QLatin1String("localbrowsehome") : QLatin1String("localbrowseroot");
}

void LocalFolderBrowsePage::setView(int v)
{
    SinglePageWidget::setView(v);
    #ifdef Q_OS_WIN
    view->view()->setRootIndex(proxy->mapFromSource(model->setRootPath(isHomeFolder ? QDir::homePath() : (":/"))));
    #else
    view->view()->setRootIndex(proxy->mapFromSource(model->setRootPath(isHomeFolder ? QDir::homePath() : QDir::rootPath())));
    #endif
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

QList<Song> LocalFolderBrowsePage::selectedSongs(bool allowPlaylists) const
{
    QList<Song> songs;
    const QModelIndexList selected = view->selectedIndexes(true);
    for (const auto &idx : selected) {
        QString path = model->filePath(proxy->mapToSource(idx));
        if (!allowPlaylists && MPDConnection::isPlaylist(path)) {
            continue;
        }
        Song s;

        #ifdef TAGLIB_FOUND
        s = Tags::read(path);
        #endif

        s.file = path;
        s.type = Song::LocalFile;
        songs.append(s);
    }
    return songs;
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
    bool folderSelected=false;
    bool playlistSelected=false;
    int numSelectedTracks=0;

    for (const QModelIndex &idx: selected) {
        QFileInfo info = model->fileInfo(proxy->mapToSource(idx));
        if (info.isDir()) {
            folderSelected=true;
        } else if (MPDConnection::isPlaylist(info.fileName())) {
            playlistSelected = true;
        } else {
            numSelectedTracks++;
        }
    }
    StdActions::self()->enableAddToPlayQueue(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(false);

    browseAction->setEnabled(enable && 1==selected.count() && folderSelected);
    CustomActions::self()->setEnabled(numSelectedTracks>0 && !folderSelected);
    #ifdef TAGLIB_FOUND
    StdActions::self()->editTagsAction->setEnabled(!playlistSelected && numSelectedTracks>0 && numSelectedTracks<=250 && !folderSelected);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    StdActions::self()->replaygainAction->setEnabled(StdActions::self()->editTagsAction->isEnabled());
    #endif
    #endif // TAGLIB_FOUND
}

#include "moc_localfolderpage.cpp"
