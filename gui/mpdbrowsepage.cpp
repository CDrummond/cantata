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

#include "mpdbrowsepage.h"
#include "mpd-interface/mpdconnection.h"
#include "settings.h"
#include "support/messagebox.h"
#include "support/action.h"
#include "support/utils.h"
#include "support/monoicon.h"
#include "models/mpdlibrarymodel.h"
#include "widgets/menubutton.h"
#include "widgets/icons.h"
#include "stdactions.h"
#include "customactions.h"
#include <QDesktopServices>
#include <QUrl>

MpdBrowsePage::MpdBrowsePage(QWidget *p)
    : SinglePageWidget(p)
    , model(this)
{
    QColor col=Utils::monoIconColor();
    browseAction = new Action(MonoIcon::icon(FontAwesome::folderopen, col), tr("Open In File Manager"), this);
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(browseAction, SIGNAL(triggered()), this, SLOT(openFileManager()));
    connect(MPDConnection::self(), SIGNAL(updatingFileList()), view, SLOT(updating()));
    connect(MPDConnection::self(), SIGNAL(updatedFileList()), view, SLOT(updated()));
    connect(MPDConnection::self(), SIGNAL(updatingDatabase()), view, SLOT(updating()));
    connect(MPDConnection::self(), SIGNAL(updatedDatabase()), view, SLOT(updated()));
    Configuration config(metaObject()->className());
    view->setMode(ItemView::Mode_DetailedTree);
    view->load(config);
    MenuButton *menu=new MenuButton(this);
    menu->addActions(createViewActions(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                               << ItemView::Mode_DetailedTree));
    init(ReplacePlayQueue|AppendToPlayQueue, QList<QWidget *>() << menu);

    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    view->addAction(CustomActions::self());
    #ifdef TAGLIB_FOUND
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(StdActions::self()->copyToDeviceAction);
    #endif
    view->addAction(StdActions::self()->organiseFilesAction);
    view->addAction(StdActions::self()->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(StdActions::self()->replaygainAction);
    #endif // TAGLIB_FOUND
    #endif
    view->addAction(browseAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addSeparator();
    view->addAction(StdActions::self()->deleteSongsAction);
    #endif
    view->setModel(&model);
    view->closeSearch();
    view->alwaysShowHeader();
    connect(view, SIGNAL(updateToPlayQueue(QModelIndex,bool)), this, SLOT(updateToPlayQueue(QModelIndex,bool)));
    view->setInfoText(tr("No folders? Looks like your MPD is not configured correctly."));
}

MpdBrowsePage::~MpdBrowsePage()
{
    Configuration config(metaObject()->className());
    view->save(config);
}

void MpdBrowsePage::showEvent(QShowEvent *e)
{
    view->focusView();
    SinglePageWidget::showEvent(e);
    model.setEnabled(true);
    model.load();
}

void MpdBrowsePage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enable=selected.count()>0;
    bool trackSelected=false;
    bool folderSelected=false;

    for (const QModelIndex &idx: selected) {
        if (static_cast<BrowseModel::Item *>(idx.internalPointer())->isFolder()) {
            folderSelected=true;
        } else {
            trackSelected=true;
        }
    }

    StdActions::self()->enableAddToPlayQueue(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enable);
    bool fileActions = trackSelected && !folderSelected && MPDConnection::self()->getDetails().dirReadable;
    CustomActions::self()->setEnabled(fileActions);
    #ifdef TAGLIB_FOUND
    StdActions::self()->organiseFilesAction->setEnabled(fileActions);
    StdActions::self()->editTagsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    StdActions::self()->replaygainAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->deleteSongsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    StdActions::self()->copyToDeviceAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #endif // TAGLIB_FOUND

    browseAction->setEnabled(enable && 1==selected.count() && folderSelected);
}

void MpdBrowsePage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return; //doubleclick should only have one selected item
    }

    if (!static_cast<BrowseModel::Item *>(selected.at(0).internalPointer())->isFolder()) {
        addSelectionToPlaylist();
    }
}

void MpdBrowsePage::openFileManager()
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return;
    }

    BrowseModel::Item *item = static_cast<BrowseModel::Item *>(selected.at(0).internalPointer());
    if (item->isFolder()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(MPDConnection::self()->getDetails().dir+static_cast<BrowseModel::FolderItem *>(item)->getPath()));
    }
}

void MpdBrowsePage::updateToPlayQueue(const QModelIndex &idx, bool replace)
{
    BrowseModel::Item *item = static_cast<BrowseModel::Item *>(idx.internalPointer());
    if (item->isFolder()) {
        emit add(QStringList() << MPDConnection::constDirPrefix+static_cast<BrowseModel::FolderItem *>(item)->getPath(),
                 replace ? MPDConnection::ReplaceAndplay : MPDConnection::Append, 0, false);
    }
}

void MpdBrowsePage::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

QList<Song> MpdBrowsePage::selectedSongs(bool allowPlaylists) const
{
    return model.songs(view->selectedIndexes(), allowPlaylists);
}

QStringList MpdBrowsePage::selectedFiles(bool allowPlaylists) const
{
    QList<Song> songs=selectedSongs(allowPlaylists);
    QStringList files;
    for (const Song &s: songs) {
        files.append(s.file);
    }
    return files;
}

void MpdBrowsePage::addSelectionToPlaylist(const QString &name, int action, quint8 priority, bool decreasePriority)
{
    QModelIndexList selected=view->selectedIndexes();
    QStringList dirs;
    QStringList files;

    for (const QModelIndex &idx: selected) {
        if (static_cast<BrowseModel::Item *>(idx.internalPointer())->isFolder()) {
            files+=static_cast<BrowseModel::FolderItem *>(idx.internalPointer())->allEntries(false);
        } else {
            files.append(static_cast<BrowseModel::TrackItem *>(idx.internalPointer())->getSong().file);
        }
    }

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, action, priority, decreasePriority);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
void MpdBrowsePage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void MpdBrowsePage::deleteSongs()
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        if (MessageBox::Yes==MessageBox::warningYesNo(this, tr("Are you sure you wish to delete the selected songs?\n\nThis cannot be undone."),
                                                      tr("Delete Songs"), StdGuiItem::del(), StdGuiItem::cancel())) {
            emit deleteSongs(QString(), songs);
        }
        view->clearSelection();
    }
}
#endif

#include "moc_mpdbrowsepage.cpp"
