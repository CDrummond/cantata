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

#include "synccollectionwidget.h"
#include "treeview.h"
#include "musiclibraryproxymodel.h"
#include "musiclibraryitemsong.h"
#include "icon.h"
#include "localize.h"
#include "actioncollection.h"
#include <QTimer>
#include <QAction>
#include <QDebug>

SyncCollectionWidget::SyncCollectionWidget(QWidget *parent, const QString &title, const QString &action, bool showCovers)
    : QWidget(parent)
    , searchTimer(0)
{
    setupUi(this);
    groupBox->setTitle(title);
    button->setText(action);
    button->setEnabled(false);

    model=new MusicLibraryModel(this, false, true);
    proxy=new MusicLibraryProxyModel(this);
    model->setUseAlbumImages(showCovers);
    proxy->setSourceModel(model);
    tree->setModel(proxy);
    tree->setPageDefaults();
    search->setText(QString());
    search->setPlaceholderText(i18n("Search"));
    connect(proxy, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect(model, SIGNAL(checkedSongs(QSet<Song>)), this, SLOT(songsChecked(QSet<Song>)));
    connect(button, SIGNAL(clicked()), SLOT(copySongs()));
    connect(search, SIGNAL(returnPressed()), this, SLOT(delaySearchItems()));
    connect(search, SIGNAL(textChanged(const QString)), this, SLOT(delaySearchItems()));

    copyAction=new Action(action, this);
    connect(copyAction, SIGNAL(triggered(bool)), SLOT(copySongs()));
    deselectAction=new Action(i18n("Uncheck All"), this);
    connect(deselectAction, SIGNAL(triggered(bool)), SLOT(deselectAll()));
    tree->addAction(copyAction);
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    tree->addAction(sep);
    tree->addAction(deselectAction);
    tree->setContextMenuPolicy(Qt::ActionsContextMenu);

    QAction *expand=ActionCollection::get()->action("expandall");
    QAction *collapse=ActionCollection::get()->action("collapseall");
    if (expand && collapse) {
        tree->addAction(expand);
        tree->addAction(collapse);
        addAction(expand);
        addAction(collapse);
        connect(expand, SIGNAL(triggered(bool)), this, SLOT(expandAll()));
        connect(collapse, SIGNAL(triggered(bool)), this, SLOT(collapseAll()));
    }

    updateStats();
}

SyncCollectionWidget::~SyncCollectionWidget()
{
}

void SyncCollectionWidget::deselectAll()
{
    model->uncheckAll();
}

void SyncCollectionWidget::copySongs()
{
    if (checkedSongs.isEmpty()) {
        return;
    }
    QList<Song> songs=checkedSongs.toList();
    qSort(songs);
    emit copy(songs);
}

void SyncCollectionWidget::delaySearchItems()
{
    if (search->text().trimmed().isEmpty()) {
        if (searchTimer) {
            searchTimer->stop();
        }
        searchItems();
    } else {
        if (!searchTimer) {
            searchTimer=new QTimer(this);
            searchTimer->setSingleShot(true);
            connect(searchTimer, SIGNAL(timeout()), SLOT(searchItems()));
        }
        searchTimer->start(500);
    }
}

void SyncCollectionWidget::searchItems()
{
    QString text=search->text().trimmed();
    proxy->update(text);
    if (proxy->enabled() && !text.isEmpty()) {
        tree->expandAll();
    }
}

void SyncCollectionWidget::expandAll()
{
    QWidget *f=QApplication::focusWidget();
    if (f && qobject_cast<QTreeView *>(f)) {
        static_cast<QTreeView *>(f)->expandAll();
    }
}

void SyncCollectionWidget::collapseAll()
{
    QWidget *f=QApplication::focusWidget();
    if (f && qobject_cast<QTreeView *>(f)) {
        static_cast<QTreeView *>(f)->collapseAll();
    }
}

void SyncCollectionWidget::songsChecked(const QSet<Song> &songs)
{
    checkedSongs=songs;
    button->setEnabled(!checkedSongs.isEmpty());
    updateStats();
}

void SyncCollectionWidget::dataChanged(const QModelIndex &tl, const QModelIndex &br)
{
    Q_UNUSED(br)
    QModelIndex index = proxy->mapToSource(tl);
    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(index.internalPointer());
    if (MusicLibraryItem::Type_Song==item->itemType()) {
        Song s=static_cast<MusicLibraryItemSong *>(item)->song();
        if (Qt::Checked==item->checkState()) {
            checkedSongs.insert(s);
        } else {
            checkedSongs.remove(s);
        }
        button->setEnabled(!checkedSongs.isEmpty());
        updateStats();
    }
}

void SyncCollectionWidget::updateStats()
{
    QSet<QString> artists;
    QSet<QString> albums;

    foreach (const Song &s, checkedSongs) {
        artists.insert(s.albumArtist());
        albums.insert(s.albumArtist()+"--"+s.album);
    }

    if (checkedSongs.isEmpty()) {
        selection->setText(i18n("Nothing selected"));
    } else {
        selection->setText(i18n("Artists:%1, Albums:%2, Songs:%3").arg(artists.count()).arg(albums.count()).arg(checkedSongs.count()));
    }
    copyAction->setEnabled(!checkedSongs.isEmpty());
    deselectAction->setEnabled(!checkedSongs.isEmpty());
}
