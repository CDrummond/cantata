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
#include "localize.h"
#include "messagebox.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtCore/QDir>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KActionCollection>
#include <KDE/KRun>
#else
#include <QtGui/QAction>
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
    browseAction = p->actionCollection()->addAction("openfilemanager");
    browseAction->setText(i18n("Open File Manager"));
    browseAction->setIcon(QIcon::fromTheme("system-file-manager"));
    #endif
    MainWindow::initButton(addToPlayQueue);
    MainWindow::initButton(replacePlayQueue);
    MainWindow::initButton(libraryUpdate);

    view->setTopText(i18n("Folders"));
    view->addAction(p->addToPlayQueueAction);
    view->addAction(p->replacePlayQueueAction);
    view->addAction(p->addToStoredPlaylistAction);
//     view->addAction(p->burnAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(p->copyToDeviceAction);
    #endif
    view->addAction(p->organiseFilesAction);
    view->addAction(p->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(p->replaygainAction);
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
    connect(this, SIGNAL(listAll()), MPDConnection::self(), SLOT(listAll()));
    connect(this, SIGNAL(add(const QStringList &, bool)), MPDConnection::self(), SLOT(add(const QStringList &, bool)));
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
        view->showSpinner();
        emit listAll();
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
    mw->replacePlayQueueAction->setEnabled(enable);
    mw->addToStoredPlaylistAction->setEnabled(enable);
    #ifdef ENABLE_DEVICES_SUPPORT
    mw->copyToDeviceAction->setEnabled(enable);
    #endif
    mw->organiseFilesAction->setEnabled(enable);
    mw->editTagsAction->setEnabled(enable);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    mw->replaygainAction->setEnabled(enable);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    mw->deleteSongsAction->setEnabled(enable);
    #endif
    #ifdef ENABLE_KDE_SUPPORT
    browseAction->setEnabled(false);
    if (1==selected.count() && QDir(Settings::self()->mpdDir()).isReadable()) {
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
        KRun::runUrl(KUrl(Settings::self()->mpdDir()+item->fullName()), "inode/directory", this);
    }
}
#endif

QList<Song> FolderPage::selectedSongs() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return MusicLibraryModel::self()->songs(DirViewModel::self()->filenames(mapped));
}

QStringList FolderPage::selectedFiles() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QStringList();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return DirViewModel::self()->filenames(mapped);
}

void FolderPage::addSelectionToPlaylist(const QString &name, bool replace)
{
    QStringList files=selectedFiles();

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, replace);
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
