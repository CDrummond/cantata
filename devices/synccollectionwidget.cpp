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

#include "synccollectionwidget.h"
#include "widgets/treeview.h"
#include "widgets/toolbutton.h"
#include "widgets/icons.h"
#include "models/musiclibraryitemartist.h"
#include "models/musiclibraryitemalbum.h"
#include "models/musiclibraryitemsong.h"
#include "support/icon.h"
#include "support/actioncollection.h"
#include <QTimer>
#include <QAction>
#include <algorithm>

SyncCollectionWidget::SyncCollectionWidget(QWidget *parent, const QString &title)
    : QWidget(parent)
    , performedSearch(false)
    , searchTimer(nullptr)
{
    setupUi(this);
    titleLabel->setText(title);
    cfgButton->setIcon(Icons::self()->configureIcon);
    connect(cfgButton, SIGNAL(clicked(bool)), SIGNAL(configure()));

    proxy.setSourceModel(&model);
    tree->setModel(&proxy);
    tree->setPageDefaults();
    tree->setUseSimpleDelegate();
    search->setText(QString());
    search->setPlaceholderText(tr("Search"));
    connect(&proxy, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect(search, SIGNAL(returnPressed()), this, SLOT(delaySearchItems()));
    connect(search, SIGNAL(textChanged(const QString)), this, SLOT(delaySearchItems()));

    checkAction=new Action(tr("Check Items"), this);
    connect(checkAction, SIGNAL(triggered()), SLOT(checkItems()));
    unCheckAction=new Action(tr("Uncheck Items"), this);
    connect(unCheckAction, SIGNAL(triggered()), SLOT(unCheckItems()));
    tree->addAction(checkAction);
    tree->addAction(unCheckAction);
    tree->setContextMenuPolicy(Qt::ActionsContextMenu);

    QAction *expand=ActionCollection::get()->action("expandall");
    QAction *collapse=ActionCollection::get()->action("collapseall");
    if (expand && collapse) {
        tree->addAction(expand);
        tree->addAction(collapse);
        addAction(expand);
        addAction(collapse);
        connect(expand, SIGNAL(triggered()), this, SLOT(expandAll()));
        connect(collapse, SIGNAL(triggered()), this, SLOT(collapseAll()));
    }

    connect(tree, SIGNAL(itemsSelected(bool)), checkAction, SLOT(setEnabled(bool)));
    connect(tree, SIGNAL(itemsSelected(bool)), unCheckAction, SLOT(setEnabled(bool)));
    connect(tree, SIGNAL(itemActivated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(tree, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
}

SyncCollectionWidget::~SyncCollectionWidget()
{
}

void SyncCollectionWidget::update(const QSet<Song> &songs)
{
    model.setSongs(songs);
}

QList<Song> SyncCollectionWidget::checkedSongs() const
{
    QList<Song> songs;
    for (const Song *s: checked) {
        songs.append(*s);
    }
    std::sort(songs.begin(), songs.end());
    return songs;
}

void SyncCollectionWidget::dataChanged(const QModelIndex &tl, const QModelIndex &br)
{
    bool haveChecked=numCheckedSongs()>0;
    QModelIndex firstIndex = proxy.mapToSource(tl);
    QModelIndex lastIndex = proxy.mapToSource(br);
    const MusicLibraryItem *item=static_cast<const MusicLibraryItem *>(firstIndex.internalPointer());
    switch (item->itemType()) {
    case MusicLibraryItem::Type_Artist:
        for (int i=firstIndex.row(); i<=lastIndex.row(); ++i) {
            QModelIndex index=model.index(i, 0, firstIndex.parent());
            const MusicLibraryItemArtist *artist=static_cast<const MusicLibraryItemArtist *>(index.internalPointer());
            for (const MusicLibraryItem *alItem: artist->childItems()) {
                for (const MusicLibraryItem *sItem: static_cast<const MusicLibraryItemContainer *>(alItem)->childItems()) {
                    songToggled(static_cast<const MusicLibraryItemSong *>(sItem));
                }
            }
        }
        break;
    case MusicLibraryItem::Type_Album:
        for (int i=firstIndex.row(); i<=lastIndex.row(); ++i) {
            QModelIndex index=model.index(i, 0, firstIndex.parent());
            const MusicLibraryItemAlbum *album=static_cast<const MusicLibraryItemAlbum *>(index.internalPointer());
            for (const MusicLibraryItem *sItem: album->childItems()) {
                songToggled(static_cast<const MusicLibraryItemSong *>(sItem));
            }
        }
        break;
    case MusicLibraryItem::Type_Song:
        for (int i=firstIndex.row(); i<=lastIndex.row(); ++i) {
            QModelIndex index=model.index(i, 0, firstIndex.parent());
            songToggled(static_cast<MusicLibraryItemSong *>(index.internalPointer()));
        }
    default:
        break;
    }

    if (haveChecked!=(numCheckedSongs()>0)) {
        emit selectionChanged();
    }
}

void SyncCollectionWidget::songToggled(const MusicLibraryItemSong *song)
{
    const Song &s=song->song();
    if (Qt::Checked==song->checkState()) {
        checked.insert(&s);
        spaceRequired+=s.size;
    } else {
        checked.remove(&s);
        spaceRequired-=s.size;
    }
}

void SyncCollectionWidget::checkItems()
{
    checkItems(true);
}

void SyncCollectionWidget::unCheckItems()
{
    checkItems(false);
}

void SyncCollectionWidget::checkItems(bool c)
{
    const QModelIndexList selected = tree->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    for (const QModelIndex &idx: selected) {
        model.setData(proxy.mapToSource(idx), c, Qt::CheckStateRole);
    }
}

void SyncCollectionWidget::delaySearchItems()
{
    if (search->text().trimmed().isEmpty()) {
        if (searchTimer) {
            searchTimer->stop();
        }
        if (performedSearch) {
            tree->collapseToLevel(0);
        }
        searchItems();
        performedSearch=false;
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
    proxy.update(text);
    if (proxy.enabled() && !text.isEmpty()) {
        tree->expandAll();
    }
    performedSearch=true;
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

void SyncCollectionWidget::itemClicked(const QModelIndex &index)
{
    if (TreeView::getForceSingleClick() && !tree->checkBoxClicked(index)) {
        tree->setExpanded(index, !tree->isExpanded(index));
    }
}

void SyncCollectionWidget::itemActivated(const QModelIndex &index)
{
    if (!TreeView::getForceSingleClick()) {
        tree->setExpanded(index, !tree->isExpanded(index));
    }
}

#include "moc_synccollectionwidget.cpp"
