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
#include "stretchheaderview.h"
#include "basicitemdelegate.h"
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QPainter>
#include <QApplication>
#include <qglobal.h>

// Exported by QtGui
void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);


class PlayQueueTreeViewItemDelegate : public QStyledItemDelegate
{
public:
    PlayQueueTreeViewItemDelegate(QObject *p) : QStyledItemDelegate(p) { }
    virtual ~PlayQueueTreeViewItemDelegate() { }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }

        bool selected=option.state&QStyle::State_Selected;
        bool active=option.state&QStyle::State_Active;
        QColor col(option.palette.color(active ? QPalette::Active : QPalette::Inactive,
                                        selected ? QPalette::HighlightedText : QPalette::Text));

        if (4==option.version) {
            const QStyleOptionViewItemV4 &v4=(QStyleOptionViewItemV4 &)option;
            QIcon icon;

            if (QStyleOptionViewItemV4::Beginning==v4.viewItemPosition) {
                icon=index.data(PlayQueueView::Role_Decoration).value<QIcon>();
            }
            QSize decorSize;
            if (!icon.isNull()) {
                decorSize = icon.actualSize(option.decorationSize);
            }

            if (decorSize.isEmpty()) {
                QStyledItemDelegate::paint(painter, option, index);
            } else {
                QStyleOptionViewItemV4 opt=v4;
                int width=decorSize.width()+(Utils::isHighDpi() ? 8 : 4);
                opt.rect.adjust(width, 0, 0, 0);
                QStyledItemDelegate::paint(painter, opt, index);

                QPixmap pix=icon.pixmap(decorSize);
                painter->drawPixmap(option.rect.x()+((width-pix.width())/2), option.rect.y()+((option.rect.height()-pix.height())/2), pix);
            }

            switch (v4.viewItemPosition) {
            case QStyleOptionViewItemV4::Beginning:
                BasicItemDelegate::drawLine(painter, option.rect, col, true, false);
                break;
            case QStyleOptionViewItemV4::Middle:
                BasicItemDelegate::drawLine(painter, option.rect, col, false, false);
                break;
            case QStyleOptionViewItemV4::End:
                BasicItemDelegate::drawLine(painter, option.rect, col, false, true);
                break;
            case QStyleOptionViewItemV4::Invalid:
            case QStyleOptionViewItemV4::OnlyOne:
                BasicItemDelegate::drawLine(painter, option.rect, col, true, true);
            }
        } else {
            QStyledItemDelegate::paint(painter, option, index);
            BasicItemDelegate::drawLine(painter, option.rect, col, false, false);
        }
    }
};

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
    setItemDelegate(new PlayQueueTreeViewItemDelegate(this));
    setHeader(new StretchHeaderView(Qt::Horizontal, this));
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

void PlayQueueTreeView::initHeader()
{
    if (!model()) {
        return;
    }

    StretchHeaderView *hdr=qobject_cast<StretchHeaderView *>(header());
    if (!menu) {
        hdr->SetStretchEnabled(true);
        hdr->setContextMenuPolicy(Qt::CustomContextMenu);
        hdr->SetColumnWidth(PlayQueueModel::COL_TRACK, 0.075);
        hdr->SetColumnWidth(PlayQueueModel::COL_DISC, 0.03);
        hdr->SetColumnWidth(PlayQueueModel::COL_TITLE, 0.3);
        hdr->SetColumnWidth(PlayQueueModel::COL_ARTIST, 0.27);
        hdr->SetColumnWidth(PlayQueueModel::COL_ALBUM, 0.27);
        hdr->SetColumnWidth(PlayQueueModel::COL_LENGTH, 0.05);
        hdr->SetColumnWidth(PlayQueueModel::COL_YEAR, 0.05);
        hdr->SetColumnWidth(PlayQueueModel::COL_GENRE, 0.1);
        hdr->SetColumnWidth(PlayQueueModel::COL_PRIO, 0.015);
        hdr->setMovable(true);
        connect(hdr, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu()));
    }

    //Restore state
    QByteArray state=Settings::self()->playQueueHeaderState();
    if (state.isEmpty()) {
        hdr->HideSection(PlayQueueModel::COL_YEAR);
        hdr->HideSection(PlayQueueModel::COL_DISC);
        hdr->HideSection(PlayQueueModel::COL_GENRE);
        hdr->HideSection(PlayQueueModel::COL_PRIO);
    } else {
        hdr->RestoreState(state);
    }

    if (!menu) {
        menu = new QMenu(this);
        QAction *stretch=new QAction(i18n("Stretch Columns To Fit Window"), this);
        stretch->setCheckable(true);
        stretch->setChecked(hdr->is_stretch_enabled());
        connect(stretch, SIGNAL(toggled(bool)), hdr, SLOT(SetStretchEnabled(bool)));
        menu->addAction(stretch);
        menu->addSeparator();
        QList<int> hideAble=QList<int>() << PlayQueueModel::COL_TRACK << PlayQueueModel::COL_ALBUM << PlayQueueModel::COL_LENGTH
                                         << PlayQueueModel::COL_DISC << PlayQueueModel::COL_YEAR << PlayQueueModel::COL_GENRE
                                         << PlayQueueModel::COL_PRIO;
        foreach (int col, hideAble) {
            QAction *act=new QAction(PlayQueueModel::headerText(col), menu);
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
        Settings::self()->savePlayQueueHeaderState(qobject_cast<StretchHeaderView *>(header())->SaveState());
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
            qobject_cast<StretchHeaderView *>(header())->SetSectionHidden(index, !visible);
        }
    }
}

PlayQueueView::PlayQueueView(QWidget *parent)
    : QStackedWidget(parent)
    , spinner(0)
    , msgOverlay(0)
    , backgroundImageType(BI_None)
    , backgroundOpacity(15)
    , backgroundBlur(0)
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

bool PlayQueueView::readConfig()
{
    int origOpacity=backgroundOpacity;
    int origBlur=backgroundBlur;
    QString origCustomBackgroundFile=customBackgroundFile;
    BackgroundImage origType=backgroundImageType;
    setAutoExpand(Settings::self()->playQueueAutoExpand());
    setStartClosed(Settings::self()->playQueueStartClosed());
    backgroundImageType=(BackgroundImage)Settings::self()->playQueueBackground();
    backgroundOpacity=Settings::self()->playQueueBackgroundOpacity();
    backgroundBlur=Settings::self()->playQueueBackgroundBlur();
    customBackgroundFile=Settings::self()->playQueueBackgroundFile();
    setGrouped(Settings::self()->playQueueGrouped());
    switch (backgroundImageType) {
    case BI_None:
        if (origType!=backgroundImageType) {
            updatePalette();
            previousBackground=QPixmap();
            curentCover=QImage();
            curentBackground=QPixmap();
            view()->viewport()->update();
        }
        break;
    case BI_Cover:
        if (BI_None==origType) {
            updatePalette();
        }
        return origType!=backgroundImageType || backgroundOpacity!=origOpacity || backgroundBlur!=origBlur;
   case BI_Custom:
        if (BI_None==origType) {
            updatePalette();
        }
        if (origType!=backgroundImageType || backgroundOpacity!=origOpacity || backgroundBlur!=origBlur || origCustomBackgroundFile!=customBackgroundFile) {
            setImage(QImage(customBackgroundFile));
        }
        break;
    }
    return false;
}

void PlayQueueView::saveConfig()
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
            spinner->setWidget(view());
            if (spinner->isActive()) {
                spinner->start();
            }
        }
        if (msgOverlay) {
            msgOverlay->setWidget(view());
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
    view()->scrollTo(index, hint);
}

QModelIndex PlayQueueView::indexAt(const QPoint &point)
{
    return view()->indexAt(point);
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

void PlayQueueView::clearSelection()
{
    if (groupedView->selectionModel()) {
        groupedView->selectionModel()->clear();
    }
    if (treeView->selectionModel()) {
        treeView->selectionModel()->clear();
    }
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

QModelIndexList PlayQueueView::selectedIndexes(bool sorted) const
{
    return isGrouped() ? groupedView->selectedIndexes(sorted) : treeView->selectedIndexes(sorted);
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
    spinner->setWidget(view());
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

void PlayQueueView::updatePalette()
{
    QPalette pal=palette();

    if (BI_None!=backgroundImageType) {
        pal.setColor(QPalette::Base, Qt::transparent);
    }

    groupedView->setPalette(pal);
    groupedView->viewport()->setPalette(pal);
    treeView->setPalette(pal);
    treeView->viewport()->setPalette(pal);
}

void PlayQueueView::setImage(const QImage &img)
{
    if (BI_None==backgroundImageType || (sender() && BI_Custom==backgroundImageType)) {
        return;
    }
    previousBackground=curentBackground;
    if (img.isNull() || QImage::Format_ARGB32==img.format()) {
        curentCover = img;
    } else {
        curentCover = img.convertToFormat(QImage::Format_ARGB32);
    }
    if (!curentCover.isNull()) {
        if (backgroundOpacity<100) {
            curentCover=TreeView::setOpacity(curentCover, (backgroundOpacity*1.0)/100.0);
        }
        if (backgroundBlur>0) {
            QImage blurred(curentCover.size(), QImage::Format_ARGB32_Premultiplied);
            blurred.fill(Qt::transparent);
            QPainter painter(&blurred);
            qt_blurImage(&painter, curentCover, backgroundBlur, true, false);
            painter.end();
            curentCover = blurred;
        }
    }

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
        msgOverlay->setWidget(view());
        connect(msgOverlay, SIGNAL(cancel()), SIGNAL(cancelStreamFetch()));
        connect(msgOverlay, SIGNAL(cancel()), SLOT(hideSpinner()));
    }
    msgOverlay->setText(msg);
}

void PlayQueueView::drawBackdrop(QWidget *widget, const QSize &size)
{
    if (BI_None==backgroundImageType) {
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
