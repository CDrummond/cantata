/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#else
#include <QAction>
#endif

FolderPage::FolderPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->updateDbAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

#ifdef ENABLE_KDE_SUPPORT
    search->setPlaceholderText(i18n("Search files..."));
#else
    search->setPlaceholderText(tr("Search files..."));
#endif
    view->setPageDefaults();
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);

    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    connect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *)), &model, SLOT(updateDirView(DirViewItemRoot *)));
    connect(search, SIGNAL(returnPressed()), this, SLOT(searchItems()));
    connect(search, SIGNAL(textChanged(const QString)), this, SLOT(searchItems()));
    connect(this, SIGNAL(listAll()), MPDConnection::self(), SLOT(listAll()));
    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
}

FolderPage::~FolderPage()
{
}

void FolderPage::refresh()
{
    emit listAll();
}

void FolderPage::clear()
{
    model.clear();
}

void FolderPage::searchItems()
{
    if (search->text().isEmpty()) {
        proxy.setFilterRegExp("");
        return;
    }

    proxy.setFilterRegExp(search->text());
}

void FolderPage::itemActivated(const QModelIndex &index)
{
    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(index).internalPointer());

    if (DirViewItem::Type_File!=item->type()) {
        view->setExpanded(index, !view->isExpanded(index));
    }
}

void FolderPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();
    if (selectionSize != 1) {
        return; //doubleclick should only have one selected item
    }

    const QModelIndex current = selected.at(0);
    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(current).internalPointer());
    if (DirViewItem::Type_File==item->type()) {
        addSelectionToPlaylist();
    }
}

void FolderPage::addSelectionToPlaylist()
{
    // Get selected view indexes
    const QModelIndexList selected = view->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();

    if (selectionSize == 0) {
        return;
    }

    QStringList files;

    for (int selectionPos = 0; selectionPos < selectionSize; selectionPos++) {
        QModelIndex current = selected.at(selectionPos);
        DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(current).internalPointer());
        QStringList tmp;

        switch (item->type()) {
        case DirViewItem::Type_Dir:
            tmp = walk(current);
            for (int i = 0; i < tmp.size(); i++) {
                if (!files.contains(tmp.at(i)))
                    files << tmp.at(i);
            }
            break;
        case DirViewItem::Type_File:
            if (!files.contains(item->fileName()))
                files << item->fileName();
            break;
        default:
            break;
        }
    }

    if (!files.isEmpty()) {
        emit add(files);
        view->selectionModel()->clearSelection();
    }
}

QStringList FolderPage::walk(QModelIndex rootItem)
{
    QStringList files;

    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(rootItem).internalPointer());

    if (item->type() == DirViewItem::Type_File) {
        return QStringList(item->fileName());
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
