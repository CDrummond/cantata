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

#include "playqueueview.h"
#include "playqueuemodel.h"
#include "covers.h"
#include "groupedview.h"
#include "treeview.h"
#include "settings.h"
#include "mpdstatus.h"
#include "httpserver.h"
#include <QtGui/QHeaderView>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#include <QtCore/QFile>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

PlayQueueTreeView::PlayQueueTreeView(QWidget *parent)
    : TreeView(parent)
    , menu(0)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    setDragDropOverwriteMode(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setIndentation(0);
    setItemsExpandable(false);
    setExpandsOnDoubleClick(false);
    setDropIndicatorShown(true);
    setRootIsDecorated(false);
    setUniformRowHeights(true);
}

PlayQueueTreeView::~PlayQueueTreeView()
{
}

void PlayQueueTreeView::initHeader()
{
    if (!model()) {
        return;
    }

    QFontMetrics fm(font());
    QHeaderView *hdr=header();
    if (!menu) {
        hdr->setResizeMode(QHeaderView::Interactive);
        hdr->setContextMenuPolicy(Qt::CustomContextMenu);
        hdr->resizeSection(PlayQueueModel::COL_STATUS, 20);
        hdr->resizeSection(PlayQueueModel::COL_TRACK, fm.width("999"));
        hdr->resizeSection(PlayQueueModel::COL_YEAR, fm.width("99999"));
        hdr->setResizeMode(PlayQueueModel::COL_STATUS, QHeaderView::Fixed);
        hdr->setResizeMode(PlayQueueModel::COL_TITLE, QHeaderView::Interactive);
        hdr->setResizeMode(PlayQueueModel::COL_ARTIST, QHeaderView::Interactive);
        hdr->setResizeMode(PlayQueueModel::COL_ALBUM, QHeaderView::Stretch);
        hdr->setResizeMode(PlayQueueModel::COL_TRACK, QHeaderView::Fixed);
        hdr->setResizeMode(PlayQueueModel::COL_LENGTH, QHeaderView::ResizeToContents);
        hdr->setResizeMode(PlayQueueModel::COL_DISC, QHeaderView::ResizeToContents);
        hdr->setResizeMode(PlayQueueModel::COL_YEAR, QHeaderView::Fixed);
        hdr->setStretchLastSection(false);
        connect(hdr, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu()));
    }

    //Restore state
    QByteArray state;

    if (Settings::self()->version()>=CANTATA_MAKE_VERSION(0, 4, 0)) {
        state=Settings::self()->playQueueHeaderState();
    }

    QList<int> hideAble;
    hideAble << PlayQueueModel::COL_TRACK << PlayQueueModel::COL_ALBUM << PlayQueueModel::COL_LENGTH
             << PlayQueueModel::COL_DISC << PlayQueueModel::COL_YEAR << PlayQueueModel::COL_GENRE;

    //Restore
    if (state.isEmpty()) {
        hdr->setSectionHidden(PlayQueueModel::COL_YEAR, true);
        hdr->setSectionHidden(PlayQueueModel::COL_DISC, true);
        hdr->setSectionHidden(PlayQueueModel::COL_GENRE, true);
    } else {
        hdr->restoreState(state);

        foreach (int col, hideAble) {
            if (hdr->isSectionHidden(col) || 0==hdr->sectionSize(col)) {
                hdr->setSectionHidden(col, true);
            }
        }
    }

    if (!menu) {
        menu = new QMenu(this);

        foreach (int col, hideAble) {
            QString text=PlayQueueModel::COL_TRACK==col
                            ?
                                #ifdef ENABLE_KDE_SUPPORT
                                i18n("Track")
                                #else
                                tr("Track")
                                #endif
                            : PlayQueueModel::headerText(col);
            QAction *act=new QAction(text, menu);
            act->setCheckable(true);
            act->setChecked(!hdr->isSectionHidden(col));
            menu->addAction(act);
            act->setData(col);
            connect(act, SIGNAL(toggled(bool)), this, SLOT(toggleHeaderItem(bool)));
        }
    }
}

void PlayQueueTreeView::saveHeader()
{
    if (menu && model()) {
        Settings::self()->savePlayQueueHeaderState(header()->saveState());
    }
}

void PlayQueueTreeView::showMenu()
{
    menu->exec(QCursor::pos());
}

void PlayQueueTreeView::toggleHeaderItem(bool visible)
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        int index=act->data().toInt();
        if (-1!=index) {
            header()->setSectionHidden(index, !visible);
        }
    }
}

PlayQueueView::PlayQueueView(QWidget *parent)
    : QStackedWidget(parent)
{
    groupedView=new GroupedView(this);
    groupedView->setIndentation(0);
    groupedView->setItemsExpandable(false);
    groupedView->setExpandsOnDoubleClick(false);
    groupedView->init();
    treeView=new PlayQueueTreeView(this);
    addWidget(groupedView);
    addWidget(treeView);
    setCurrentWidget(treeView);
    connect(groupedView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
    connect(treeView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
    connect(groupedView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
    connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
    connect(Covers::self(), SIGNAL(coverRetrieved(const QString &, const QString &)), this, SLOT(coverRetrieved(const QString &, const QString &)));
}

PlayQueueView::~PlayQueueView()
{
}

void PlayQueueView::saveHeader()
{
    if (treeView==currentWidget()) {
        treeView->saveHeader();
    }
}

void PlayQueueView::setGrouped(bool g)
{
    bool grouped=groupedView==currentWidget();

    if (g!=grouped) {
        if (grouped) {
            treeView->setModel(groupedView->model());
            treeView->initHeader();
            groupedView->setModel(0);
        } else {
            treeView->saveHeader();
            groupedView->setModel(treeView->model());
            treeView->setModel(0);
        }
        grouped=g;
        setCurrentWidget(grouped ? static_cast<QWidget *>(groupedView) : static_cast<QWidget *>(treeView));
    }
}

void PlayQueueView::setAutoExpand(bool ae)
{
    groupedView->setAutoExpand(ae);
}

bool PlayQueueView::isAutoExpand() const
{
    return groupedView->isAutoExpand();
}

void PlayQueueView::setStartClosed(bool sc)
{
    groupedView->setStartClosed(sc);
}

bool PlayQueueView::isStartClosed() const
{
    return groupedView->isStartClosed();
}

void PlayQueueView::setFilterActive(bool f)
{
    groupedView->setFilterActive(f);
}

void PlayQueueView::updateRows(qint32 row, bool scroll)
{
    if (currentWidget()==groupedView) {
        groupedView->updateRows(row, scroll);
    }
}

void PlayQueueView::scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint)
{
    if (currentWidget()==groupedView && !groupedView->isFilterActive()) {
        return;
    }
    if (MPDState_Playing==MPDStatus::self()->state()) {
//         groupedView->scrollTo(index, hint);
        treeView->scrollTo(index, hint);
    }
}

void PlayQueueView::setModel(QAbstractItemModel *m)
{
    if (currentWidget()==groupedView) {
        groupedView->setModel(m);
    } else {
        treeView->setModel(m);
    }
}

void PlayQueueView::addAction(QAction *a)
{
    groupedView->addAction(a);
    treeView->addAction(a);
}

void PlayQueueView::setFocus()
{
    currentWidget()->setFocus();
}

QAbstractItemModel * PlayQueueView::model()
{
    return currentWidget()==groupedView ? groupedView->model() : treeView->model();
}

void PlayQueueView::setContextMenuPolicy(Qt::ContextMenuPolicy policy)
{
    groupedView->setContextMenuPolicy(policy);
    treeView->setContextMenuPolicy(policy);
}

bool PlayQueueView::haveUnSelectedItems()
{
    return currentWidget()==groupedView ? groupedView->haveUnSelectedItems() : treeView->haveUnSelectedItems();
}

QItemSelectionModel * PlayQueueView::selectionModel() const
{
    return currentWidget()==groupedView ? groupedView->selectionModel() : treeView->selectionModel();
}

void PlayQueueView::setCurrentIndex(const QModelIndex &index)
{
    if (currentWidget()==groupedView) {
        groupedView->setCurrentIndex(index);
    } else {
        treeView->setCurrentIndex(index);
    }
}

QHeaderView * PlayQueueView::header()
{
    return treeView->header();
}

QAbstractItemView * PlayQueueView::tree()
{
    return treeView;
}

QAbstractItemView * PlayQueueView::list()
{
    return groupedView;
}

bool PlayQueueView::hasFocus() const
{
    return currentWidget()==groupedView ? groupedView->hasFocus() : treeView->hasFocus();
}

QModelIndexList PlayQueueView::selectedIndexes() const
{
    return groupedView==currentWidget() ? groupedView->selectedIndexes() : selectionModel()->selectedRows();
}

QList<Song> PlayQueueView::selectedSongs() const
{
    const QModelIndexList selected = selectedIndexes();
    QList<Song> songs;

    foreach (const QModelIndex &idx, selected) {
        Song song=idx.data(GroupedView::Role_Song).value<Song>();
        if (!song.file.isEmpty() && !song.file.contains(":/") && !song.file.startsWith('/') && QFile::exists(Settings::self()->mpdDir()+song.file)) {
            songs.append(song);
        }
    }

    return songs;
}

void PlayQueueView::coverRetrieved(const QString &artist, const QString &album)
{
    if (groupedView!=currentWidget()) {
        return;
    }

    groupedView->coverRetrieved(artist, album);
}