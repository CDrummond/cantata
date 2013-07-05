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

#include "playqueueview.h"
#include "playqueuemodel.h"
#include "covers.h"
#include "groupedview.h"
#include "treeview.h"
#include "settings.h"
#include "mpdstatus.h"
#include "localize.h"
#include "spinner.h"
#include "messageoverlay.h"
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QPainter>
#include <QApplication>
#include <qglobal.h>

static const double constBgndOpacity=0.15;

static QImage setOpacity(const QImage &orig)
{
    QImage img=QImage::Format_ARGB32==orig.format() ? orig : orig.convertToFormat(QImage::Format_ARGB32);
    uchar *bits = img.bits();
    for (int i = 0; i < img.height()*img.bytesPerLine(); i+=4) {
        bits[i+3] = constBgndOpacity*255;
    }
    return img;
}

PlayQueueTreeView::PlayQueueTreeView(PlayQueueView *parent)
    : TreeView(parent, true)
    , view(parent)
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
    setUseSimpleDelegate();
}

PlayQueueTreeView::~PlayQueueTreeView()
{
}

void PlayQueueTreeView::paintEvent(QPaintEvent *e)
{
    view->drawBackdrop(viewport(), size());
    TreeView::paintEvent(e);
}

PlayQueueGroupedView::PlayQueueGroupedView(PlayQueueView *parent)
    : GroupedView(parent, true)
    , view(parent)
{
}

PlayQueueGroupedView::~PlayQueueGroupedView()
{
}

void PlayQueueGroupedView::paintEvent(QPaintEvent *e)
{
    view->drawBackdrop(viewport(), size());
    GroupedView::paintEvent(e);
}

#if QT_VERSION < 0x050000
static inline void setResizeMode(QHeaderView *hdr, int idx, QHeaderView::ResizeMode mode)
{
    hdr->setResizeMode(idx, mode);
}

static inline void setResizeMode(QHeaderView *hdr, QHeaderView::ResizeMode mode)
{
    hdr->setResizeMode(mode);
}
#else
static inline void setResizeMode(QHeaderView *hdr, int idx, QHeaderView::ResizeMode mode)
{
    hdr->setSectionResizeMode(idx, mode);
}

static inline void setResizeMode(QHeaderView *hdr, QHeaderView::ResizeMode mode)
{
    hdr->setSectionResizeMode(mode);
}
#endif

void PlayQueueTreeView::initHeader()
{
    if (!model()) {
        return;
    }

    QHeaderView *hdr=header();
    if (!menu) {
        QFont f(font());
        f.setBold(true);
        QFontMetrics fm(f);
        setResizeMode(hdr, QHeaderView::Interactive);
        hdr->setContextMenuPolicy(Qt::CustomContextMenu);
        int statusSize=model()->data(QModelIndex(), Qt::SizeHintRole).toSize().width();
        if (statusSize<20) {
            statusSize=20;
        }
        hdr->resizeSection(PlayQueueModel::COL_STATUS, statusSize);
        hdr->resizeSection(PlayQueueModel::COL_TRACK, fm.width("999"));
        hdr->resizeSection(PlayQueueModel::COL_YEAR, fm.width("99999"));
        setResizeMode(hdr, PlayQueueModel::COL_STATUS, QHeaderView::Fixed);
        setResizeMode(hdr, PlayQueueModel::COL_TITLE, QHeaderView::Interactive);
        setResizeMode(hdr, PlayQueueModel::COL_ARTIST, QHeaderView::Interactive);
        setResizeMode(hdr, PlayQueueModel::COL_ALBUM, QHeaderView::Stretch);
        setResizeMode(hdr, PlayQueueModel::COL_TRACK, QHeaderView::Fixed);
        setResizeMode(hdr, PlayQueueModel::COL_LENGTH, QHeaderView::ResizeToContents);
        setResizeMode(hdr, PlayQueueModel::COL_DISC, QHeaderView::ResizeToContents);
        setResizeMode(hdr, PlayQueueModel::COL_PRIO, QHeaderView::ResizeToContents);
        setResizeMode(hdr, PlayQueueModel::COL_YEAR, QHeaderView::Fixed);
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
             << PlayQueueModel::COL_DISC << PlayQueueModel::COL_YEAR << PlayQueueModel::COL_GENRE << PlayQueueModel::COL_PRIO;

    //Restore
    if (state.isEmpty()) {
        hdr->setSectionHidden(PlayQueueModel::COL_YEAR, true);
        hdr->setSectionHidden(PlayQueueModel::COL_DISC, true);
        hdr->setSectionHidden(PlayQueueModel::COL_GENRE, true);
        hdr->setSectionHidden(PlayQueueModel::COL_PRIO, true);
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
                            ? i18n("Track")
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
    , spinner(0)
    , msgOverlay(0)
    , useCoverAsBgnd(false)
{
    groupedView=new PlayQueueGroupedView(this);
    groupedView->setIndentation(0);
    groupedView->setItemsExpandable(false);
    groupedView->setExpandsOnDoubleClick(false);
    treeView=new PlayQueueTreeView(this);
    addWidget(groupedView);
    addWidget(treeView);
    setCurrentWidget(treeView);
    connect(groupedView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
    connect(treeView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
    connect(groupedView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
    connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
    setContextMenuPolicy(Qt::ActionsContextMenu);

    animator.setPropertyName("fade");
    animator.setTargetObject(this);
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
        if (spinner) {
            spinner->setWidget(view()->viewport());
            if (spinner->isActive()) {
                spinner->start();
            }
        }
        if (msgOverlay) {
            msgOverlay->setWidget(view()->viewport());
        }
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
    if (isGrouped()) {
        groupedView->setFilterActive(f);
    }
}

void PlayQueueView::updateRows(qint32 row, quint16 curAlbum, bool scroll)
{
    if (isGrouped()) {
        groupedView->updateRows(row, curAlbum, scroll);
    }
}

void PlayQueueView::scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint)
{
    if (isGrouped() && !groupedView->isFilterActive()) {
        return;
    }
    if (MPDState_Playing==MPDStatus::self()->state()) {
//         groupedView->scrollTo(index, hint);
        treeView->scrollTo(index, hint);
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

bool PlayQueueView::hasFocus()
{
    return currentWidget()->hasFocus();
}

void PlayQueueView::setContextMenuPolicy(Qt::ContextMenuPolicy policy)
{
    groupedView->setContextMenuPolicy(policy);
    treeView->setContextMenuPolicy(policy);
}

bool PlayQueueView::haveSelectedItems()
{
    return isGrouped() ? groupedView->haveSelectedItems() : treeView->haveSelectedItems();
}

bool PlayQueueView::haveUnSelectedItems()
{
    return isGrouped() ? groupedView->haveUnSelectedItems() : treeView->haveUnSelectedItems();
}

QHeaderView * PlayQueueView::header()
{
    return treeView->header();
}

QAbstractItemView * PlayQueueView::tree() const
{
    return treeView;
}

QAbstractItemView * PlayQueueView::list() const
{
    return groupedView;
}

QAbstractItemView * PlayQueueView::view() const
{
    return isGrouped() ? (QAbstractItemView *)groupedView : (QAbstractItemView *)treeView;
}

bool PlayQueueView::hasFocus() const
{
    return isGrouped() ? groupedView->hasFocus() : treeView->hasFocus();
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
        if (!song.file.isEmpty() && !song.file.contains(":/") && !song.file.startsWith('/')) {
            songs.append(song);
        }
    }

    return songs;
}

void PlayQueueView::showSpinner()
{
    if (!spinner) {
        spinner=new Spinner(this);
    }
    spinner->setWidget(view()->viewport());
    spinner->start();
}

void PlayQueueView::hideSpinner()
{
    if (spinner) {
        spinner->stop();
    }
}

void PlayQueueView::setFade(float value)
{
    if (fadeValue!=value) {
        fadeValue = value;
        if (qFuzzyCompare(fadeValue, qreal(1.0))) {
            previousBackground=QPixmap();
        }
        view()->viewport()->update();
    }
}

void PlayQueueView::setUseCoverAsBackgrond(bool u)
{
    if (u==useCoverAsBgnd) {
        return;
    }
    useCoverAsBgnd=u;
    updatePalette();

    if (!u) {
        previousBackground=QPixmap();
        curentCover=QImage();
        curentBackground=QPixmap();
        view()->viewport()->update();
    }
}

void PlayQueueView::updatePalette()
{
    QPalette pal=palette();

    if (useCoverAsBgnd) {
        pal.setColor(QPalette::Base, Qt::transparent);
    }

    groupedView->setPalette(pal);
    groupedView->viewport()->setPalette(pal);
    treeView->setPalette(pal);
    treeView->viewport()->setPalette(pal);
}

void PlayQueueView::setImage(const QImage &img)
{
    if (!useCoverAsBgnd) {
        return;
    }
    previousBackground=curentBackground;
    curentCover=img.isNull() ? QImage() : setOpacity(img);
    curentBackground=QPixmap();
    animator.stop();
    if (isVisible()) {
        fadeValue=0.0;
        animator.setDuration(250);
        animator.setEndValue(1.0);
        animator.start();
    }
}

void PlayQueueView::streamFetchStatus(const QString &msg)
{
    if (!msgOverlay) {
        msgOverlay=new MessageOverlay(this);
        msgOverlay->setWidget(view()->viewport());
        connect(msgOverlay, SIGNAL(cancel()), SIGNAL(cancelStreamFetch()));
        connect(msgOverlay, SIGNAL(cancel()), SLOT(hideSpinner()));
    }
    msgOverlay->setText(msg);
}

void PlayQueueView::drawBackdrop(QWidget *widget, const QSize &size)
{
    if (!useCoverAsBgnd) {
        return;
    }

    QPainter p(widget);

    p.fillRect(0, 0, size.width(), size.height(), QApplication::palette().color(QPalette::Base));
    if (!curentCover.isNull() || !previousBackground.isNull()) {
        if (!curentCover.isNull() && (size!=lastBgndSize || curentBackground.isNull())) {
            curentBackground = QPixmap::fromImage(curentCover.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            lastBgndSize=size;
        }

        if (!previousBackground.isNull()) {
            if (!qFuzzyCompare(fadeValue, qreal(0.0))) {
                p.setOpacity(1.0-fadeValue);
            }
            p.drawPixmap((size.width()-previousBackground.width())/2, (size.height()-previousBackground.height())/2, previousBackground);
        }
        if (!curentBackground.isNull()) {
            p.setOpacity(fadeValue);
            p.drawPixmap((size.width()-curentBackground.width())/2, (size.height()-curentBackground.height())/2, curentBackground);
        }
    }
}
