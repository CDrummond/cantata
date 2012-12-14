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

#include "folderpage.h"
#include "mpdconnection.h"
#include "musiclibrarymodel.h"
#include "settings.h"
#include "icon.h"
#include "localize.h"
#include "messagebox.h"
#include "mainwindow.h"
#include "action.h"
#include "actioncollection.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtCore/QDir>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KRun>
#endif

FolderPage::FolderPage(MainWindow *p)
    : QWidget(p)
    , mw(p)
{
    setupUi(this);
    addToPlayQueue->setDefaultAction(p->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(p->replacePlayQueueAction);
    libraryUpdate->setDefaultAction(p->refreshAction);
    #ifdef ENABLE_KDE_SUPPORT
    browseAction = ActionCollection::get()->createAction("openfilemanager", i18n("Open File Manager"), "system-file-manager");
    #endif
    Icon::init(addToPlayQueue);
    Icon::init(replacePlayQueue);
    Icon::init(libraryUpdate);

    view->setTopText(i18n("Folders"));
    view->addAction(p->addToPlayQueueAction);
    view->addAction(p->replacePlayQueueAction);
    view->addAction(p->addWithPriorityAction);
    view->addAction(p->addToStoredPlaylistAction);
//     view->addAction(p->burnAction);
    #ifdef TAGLIB_FOUND
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(p->copyToDeviceAction);
    #endif
    view->addAction(p->organiseFilesAction);
    view->addAction(p->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(p->replaygainAction);
    #endif // TAGLIB_FOUND
    #endif
    #ifdef ENABLE_KDE_SUPPORT
    view->addAction(browseAction);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    view->addAction(sep);
    view->addAction(p->deleteSongsAction);
    #endif

    proxy.setSourceModel(DirViewModel::self());
    view->setModel(&proxy);
    view->init(p->replacePlayQueueAction, p->addToPlayQueueAction);
    connect(this, SIGNAL(loadFolders()), MPDConnection::self(), SLOT(loadFolders()));
    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    #ifdef ENABLE_KDE_SUPPORT
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

    mw->addToPlayQueueAction->setEnabled(enable);
    mw->addWithPriorityAction->setEnabled(enable);
    mw->replacePlayQueueAction->setEnabled(enable);
    mw->addToStoredPlaylistAction->setEnabled(enable);
    #ifdef TAGLIB_FOUND
    mw->organiseFilesAction->setEnabled(enable && MPDConnection::self()->getDetails().dirReadable);
    mw->editTagsAction->setEnabled(mw->organiseFilesAction->isEnabled());
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    mw->replaygainAction->setEnabled(mw->organiseFilesAction->isEnabled());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    mw->deleteSongsAction->setEnabled(mw->organiseFilesAction->isEnabled());
    mw->copyToDeviceAction->setEnabled(mw->organiseFilesAction->isEnabled());
    #endif
    #endif // TAGLIB_FOUND

    #ifdef ENABLE_KDE_SUPPORT
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

#ifdef ENABLE_KDE_SUPPORT
void FolderPage::openFileManager()
{
    const QModelIndexList selected = view->selectedIndexes();
    if (1!=selected.size()) {
        return;
    }

    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
    if (DirViewItem::Type_Dir==item->type()) {
        KRun::runUrl(KUrl(MPDConnection::self()->getDetails().dir+item->fullName()), "inode/directory", this);
    }
}
#endif

QList<Song> FolderPage::selectedSongs(bool allowPlaylists) const
{
    return MusicLibraryModel::self()->songs(selectedFiles(allowPlaylists));
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
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void FolderPage::deleteSongs()
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected songs?\nThis cannot be undone."))) {
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
