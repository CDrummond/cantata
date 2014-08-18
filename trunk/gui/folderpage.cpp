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

#include "folderpage.h"
#include "mpd/mpdconnection.h"
#include "models/musiclibrarymodel.h"
#include "settings.h"
#include "support/localize.h"
#include "support/messagebox.h"
#include "support/actioncollection.h"
#include "support/utils.h"
#include "stdactions.h"
#include <QDesktopServices>
#include <QUrl>

FolderPage::FolderPage(QWidget *p)
    : QWidget(p)
    , loaded(false)
{
    setupUi(this);
    addToPlayQueue->setDefaultAction(StdActions::self()->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
    browseAction = ActionCollection::get()->createAction("openfilemanager", i18n("Open In File Manager"), "system-file-manager");

    view->addAction(StdActions::self()->addToPlayQueueAction);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addWithPriorityAction);
    view->addAction(StdActions::self()->addToStoredPlaylistAction);
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
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    view->addAction(sep);
    view->addAction(StdActions::self()->deleteSongsAction);
    #endif

    proxy.setSourceModel(DirViewModel::self());
    view->setModel(&proxy);
    connect(this, SIGNAL(loadFolders()), MPDConnection::self(), SLOT(loadFolders()));
    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(browseAction, SIGNAL(triggered(bool)), this, SLOT(openFileManager()));
    connect(MPDConnection::self(), SIGNAL(updatingFileList()), view, SLOT(updating()));
    connect(MPDConnection::self(), SIGNAL(updatedFileList()), view, SLOT(updated()));
    connect(MPDConnection::self(), SIGNAL(updatingDatabase()), view, SLOT(updating()));
    connect(MPDConnection::self(), SIGNAL(updatedDatabase()), view, SLOT(updated()));
    view->load(metaObject()->className());
}

FolderPage::~FolderPage()
{
    view->save(metaObject()->className());
}

void FolderPage::setEnabled(bool e)
{
    if (e==DirViewModel::self()->isEnabled()) {
        return;
    }

    DirViewModel::self()->setEnabled(e);
    if (e) {
        refresh();
    } else {
        loaded=false;
    }
}

void FolderPage::refresh()
{
    view->goToTop();
    if (DirViewModel::self()->isEnabled()) {
        if (!isVisible()) {
            loaded=false; // Refresh called for, but we are not currently visible...
        } else if (!DirViewModel::self()->fromXML()) {
            emit loadFolders();
            loaded=true;
        }
    }
}

void FolderPage::clear()
{
    DirViewModel::self()->clear();
    loaded=false;
}

void FolderPage::showEvent(QShowEvent *e)
{
    view->focusView();
    QWidget::showEvent(e);
    if (!loaded) {
        if (!DirViewModel::self()->fromXML()) {
            emit loadFolders();
        }
        loaded=true;
    }
}

void FolderPage::searchItems()
{
    QString text=view->searchText().trimmed();
    proxy.update(text);
    if (proxy.enabled() && !proxy.filterText().isEmpty()) {
        view->expandAll();
    }
}

void FolderPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enable=selected.count()>0;

    StdActions::self()->addToPlayQueueAction->setEnabled(enable);
    StdActions::self()->addWithPriorityAction->setEnabled(enable);
    StdActions::self()->replacePlayQueueAction->setEnabled(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enable);
    #ifdef TAGLIB_FOUND
    StdActions::self()->organiseFilesAction->setEnabled(enable && MPDConnection::self()->getDetails().dirReadable);
    StdActions::self()->editTagsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    StdActions::self()->replaygainAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->deleteSongsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    StdActions::self()->copyToDeviceAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #endif // TAGLIB_FOUND

    browseAction->setEnabled(false);
    if (1==selected.count() && MPDConnection::self()->getDetails().dirReadable) {
        DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
        browseAction->setEnabled(DirViewItem::Type_Dir==item->type());
    }
}

void FolderPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return; //doubleclick should only have one selected item
    }

    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
    if (DirViewItem::Type_File==item->type()) {
        addSelectionToPlaylist();
    }
}

void FolderPage::openFileManager()
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return;
    }

    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
    if (DirViewItem::Type_Dir==item->type()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(MPDConnection::self()->getDetails().dir+item->fullName()));
    }
}

QList<Song> FolderPage::selectedSongs(EmptySongMod esMod, bool allowPlaylists) const
{
    QList<Song> songs=MusicLibraryModel::self()->songs(selectedFiles(allowPlaylists), ES_None!=esMod);

    if (ES_None!=esMod) {
        QList<Song>::Iterator it(songs.begin());
        QList<Song>::Iterator end(songs.end());
        for (; it!=end; ++it) {
            if ((*it).isEmpty()) {
                if (ES_GuessTags==esMod) {
                    (*it).guessTags();
                }
                (*it).fillEmptyFields();
            }
        }
    }

    return songs;
}

QStringList FolderPage::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }
    return DirViewModel::self()->filenames(proxy.mapToSource(selected, proxy.enabled() && Settings::self()->filteredOnly()), allowPlaylists);
}

void FolderPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    QStringList files=selectedFiles(name.isEmpty());

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, replace, priorty);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
void FolderPage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs(ES_GuessTags);

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void FolderPage::deleteSongs()
{
    QList<Song> songs=selectedSongs(ES_GuessTags);

    if (!songs.isEmpty()) {
        if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Are you sure you wish to delete the selected songs?\nThis cannot be undone."),
                                                      i18n("Delete Songs"), StdGuiItem::del(), StdGuiItem::cancel())) {
            emit deleteSongs(QString(), songs);
        }
        view->clearSelection();
    }
}
#endif

QStringList FolderPage::walk(QModelIndex rootItem)
{
    QStringList files;
    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(rootItem).internalPointer());

    if (DirViewItem::Type_File==item->type()) {
        return QStringList(item->fullName());
    }

    for (int i = 0; ; i++) {
        QModelIndex current = rootItem.child(i, 0);
        if (!current.isValid())
            return files;

        QStringList tmp = walk(current);
        for (int j = 0; j < tmp.size(); j++) {
            if (!files.contains(tmp.at(j)))
                files << tmp.at(j);
        }
    }
    return files;
}
