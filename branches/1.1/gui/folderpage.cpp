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

#include "folderpage.h"
#include "mpdconnection.h"
#include "musiclibrarymodel.h"
#include "settings.h"
#include "localize.h"
#include "messagebox.h"
#include "actioncollection.h"
#include "stdactions.h"
#ifndef Q_OS_WIN
#include <QProcess>
#endif

FolderPage::FolderPage(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    addToPlayQueue->setDefaultAction(StdActions::self()->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
    libraryUpdate->setDefaultAction(StdActions::self()->refreshAction);
    searchButton->setDefaultAction(StdActions::self()->searchAction);
    #ifndef Q_OS_WIN
    browseAction = ActionCollection::get()->createAction("openfilemanager", i18n("Open In File Manager"), "system-file-manager");
    #endif

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
    #ifndef Q_OS_WIN
    view->addAction(browseAction);
    #endif
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
    #ifndef Q_OS_WIN
    connect(browseAction, SIGNAL(triggered(bool)), this, SLOT(openFileManager()));
    #endif
}

FolderPage::~FolderPage()
{
}

void FolderPage::setEnabled(bool e)
{
    if (e==DirViewModel::self()->isEnabled()) {
        return;
    }

    DirViewModel::self()->setEnabled(e);
    if (e) {
        connect(MPDConnection::self(), SIGNAL(updatingFileList()), view, SLOT(showSpinner()));
        connect(MPDConnection::self(), SIGNAL(updatedFileList()), view, SLOT(hideSpinner()));
        refresh();
    } else {
        disconnect(MPDConnection::self(), SIGNAL(updatingFileList()), view, SLOT(showSpinner()));
        disconnect(MPDConnection::self(), SIGNAL(updatedFileList()), view, SLOT(hideSpinner()));
    }
}

void FolderPage::refresh()
{
    if (DirViewModel::self()->isEnabled()) {
        view->setLevel(0);
        view->showSpinner();
        emit loadFolders();
    }
}

void FolderPage::clear()
{
    DirViewModel::self()->clear();
}

void FolderPage::searchItems()
{
    QString text=view->searchText().trimmed();
    proxy.update(text);
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll();
    }
}

void FolderPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
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

    #ifndef Q_OS_WIN
    browseAction->setEnabled(false);
    if (1==selected.count() && MPDConnection::self()->getDetails().dirReadable) {
        DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
        browseAction->setEnabled(DirViewItem::Type_Dir==item->type());
    }
    #endif
}

void FolderPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes();
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
    #ifndef Q_OS_WIN
    const QModelIndexList selected = view->selectedIndexes();
    if (1!=selected.size()) {
        return;
    }


    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
    if (DirViewItem::Type_Dir==item->type()) {
        QProcess::startDetached(QLatin1String("xdg-open"), QStringList() << MPDConnection::self()->getDetails().dir+item->fullName());
    }
    #endif
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

    if (0==selected.size()) {
        return QStringList();
    }
    qSort(selected);

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return DirViewModel::self()->filenames(mapped, allowPlaylists);
}

void FolderPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    QStringList files=selectedFiles(true);

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
