/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "models/playqueuemodel.h"
#include "gui/covers.h"
#include "gui/currentcover.h"
#include "groupedview.h"
#include "treeview.h"
#include "gui/settings.h"
#include "mpd-interface/mpdstatus.h"
#include "support/spinner.h"
#include "messageoverlay.h"
#include "icons.h"
#include "support/gtkstyle.h"
#include "support/proxystyle.h"
#include "support/actioncollection.h"
#include "support/action.h"
#include "models/roles.h"
#include <QFile>
#include <QPainter>
#include <QApplication>
#include <qglobal.h>

// Exported by QtGui
void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);

class PlayQueueTreeStyle : public ProxyStyle
{
public:
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override
    {
        if (QStyle::PE_IndicatorItemViewItemDrop == element && widget) {
            painter->setPen(QPen(QPalette::Highlight)); // make the drop indicator more visible
            painter->setRenderHint(QPainter::Antialiasing, false);

            QStyleOption opt(*option);
            opt.rect.setLeft(0);                // let the drop indicator
            opt.rect.setRight(widget->width()); // span a whole tree widget row

            ProxyStyle::drawPrimitive(element, &opt, painter, widget);
        } else {
            ProxyStyle::drawPrimitive(element, option, painter, widget);
        }
    }
};

PlayQueueTreeView::PlayQueueTreeView(PlayQueueView *parent)
    : TableView(QLatin1String("playQueue"), parent, true)
    , view(parent)
{
    setIndentation(0);
    setItemsExpandable(false);
    setExpandsOnDoubleClick(false);
    setRootIsDecorated(false);
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

PlayQueueView::PlayQueueView(QWidget *parent)
    : QStackedWidget(parent)
    , mode(ItemView::Mode_Count)
    , groupedView(nullptr)
    , treeView(nullptr)
    , spinner(nullptr)
    , msgOverlay(nullptr)
    , backgroundImageType(BI_None)
    , fadeValue(1.0)
    , backgroundOpacity(15)
    , backgroundBlur(0)
{
    removeFromAction = new Action(Icons::self()->removeIcon, tr("Remove"), this);
    setMode(ItemView::Mode_GroupedTree);
    animator.setPropertyName("fade");
    animator.setTargetObject(this);
    connect(CurrentCover::self(), SIGNAL(coverImage(QImage)), this, SLOT(setImage(QImage)));
}

PlayQueueView::~PlayQueueView()
{
}

void PlayQueueView::readConfig()
{
    setAutoExpand(Settings::self()->playQueueAutoExpand());
    setStartClosed(Settings::self()->playQueueStartClosed());
    setMode((ItemView::Mode)Settings::self()->playQueueView());

    int origOpacity=backgroundOpacity;
    int origBlur=backgroundBlur;
    QString origCustomBackgroundFile=customBackgroundFile;
    BackgroundImage origType=backgroundImageType;
    backgroundImageType=(BackgroundImage)Settings::self()->playQueueBackground();
    backgroundOpacity=Settings::self()->playQueueBackgroundOpacity();
    backgroundBlur=Settings::self()->playQueueBackgroundBlur();
    customBackgroundFile=Settings::self()->playQueueBackgroundFile();
    switch (backgroundImageType) {
    case BI_None:
        if (origType!=backgroundImageType) {
            updatePalette();
            previousBackground=QPixmap();
            curentCover=QImage();
            curentBackground=QPixmap();
            view()->viewport()->update();
            setImage(QImage());
        }
        break;
    case BI_Cover:
        if (BI_None==origType) {
            updatePalette();
        }
        if ((origType!=backgroundImageType || backgroundOpacity!=origOpacity || backgroundBlur!=origBlur)) {
            setImage(CurrentCover::self()->isValid() ? CurrentCover::self()->image() : QImage());
        }
        break;
   case BI_Custom:
        if (BI_None==origType) {
            updatePalette();
        }
        if (origType!=backgroundImageType || backgroundOpacity!=origOpacity || backgroundBlur!=origBlur || origCustomBackgroundFile!=customBackgroundFile) {
            setImage(QImage(customBackgroundFile));
        }
        break;
    }
}

void PlayQueueView::saveConfig()
{
    if (treeView==currentWidget()) {
        treeView->saveHeader();
    }
}

void PlayQueueView::setMode(ItemView::Mode m)
{
    if (m==mode || (ItemView::Mode_GroupedTree!=m && ItemView::Mode_Table!=m)) {
        return;
    }

    if (ItemView::Mode_Table==mode) {
        treeView->saveHeader();
    }

    switch (m) {
    case ItemView::Mode_GroupedTree:
        if (!groupedView) {
            groupedView=new PlayQueueGroupedView(this);
            groupedView->setContextMenuPolicy(Qt::ActionsContextMenu);
            groupedView->setIndentation(0);
            groupedView->setItemsExpandable(false);
            groupedView->setExpandsOnDoubleClick(false);
            groupedView->installFilter(new KeyEventHandler(groupedView, removeFromAction));
            addWidget(groupedView);
            connect(groupedView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
            connect(groupedView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
            updatePalette();
            #ifdef Q_OS_MAC
            groupedView->setAttribute(Qt::WA_MacShowFocusRect, 0);
            #endif
            groupedView->setProperty(ProxyStyle::constModifyFrameProp, ProxyStyle::VF_Top);
        }
        break;
    case ItemView::Mode_Table:
        if (!treeView) {
            treeView=new PlayQueueTreeView(this);
            treeView->setStyle(new PlayQueueTreeStyle());
            treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
            treeView->installFilter(new KeyEventHandler(treeView, removeFromAction));
            treeView->initHeader();
            addWidget(treeView);
            connect(treeView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
            connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
            updatePalette();
            #ifdef Q_OS_MAC
            treeView->setAttribute(Qt::WA_MacShowFocusRect, 0);
            #endif
            treeView->setProperty(ProxyStyle::constModifyFrameProp, ProxyStyle::VF_Top);
        }
    default:
        break;
    }

    QAbstractItemModel *model=nullptr;
    QList<QAction *> actions;
    if (ItemView::Mode_Count!=mode) {
        QAbstractItemView *v=view();
        model=v->model();
        v->setModel(nullptr);
        actions=v->actions();
    }

    mode=m;
    QAbstractItemView *v=view();
    v->setModel(model);
    if (!actions.isEmpty() && v->actions().isEmpty()) {
        v->addActions(actions);
    }

    if (ItemView::Mode_Table==mode) {
        treeView->initHeader();
    }

    setCurrentWidget(static_cast<QWidget *>(view()));
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
    if (ItemView::Mode_GroupedTree==mode) {
        groupedView->setFilterActive(f);
    }
}

void PlayQueueView::updateRows(qint32 row, quint16 curAlbum, bool scroll, bool forceScroll)
{
    if (ItemView::Mode_GroupedTree==mode) {
        groupedView->updateRows(row, curAlbum, scroll, QModelIndex(), forceScroll);
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
    view()->addAction(a);
}

void PlayQueueView::setFocus()
{
    currentWidget()->setFocus();
}

bool PlayQueueView::hasFocus()
{
    return currentWidget()->hasFocus();
}

bool PlayQueueView::haveSelectedItems()
{
    switch (mode) {
    default:
    case ItemView::Mode_GroupedTree: return groupedView->haveSelectedItems();
    case ItemView::Mode_Table:       return treeView->haveSelectedItems();
    }
}

bool PlayQueueView::haveUnSelectedItems()
{
    switch (mode) {
    default:
    case ItemView::Mode_GroupedTree: return groupedView->haveUnSelectedItems();
    case ItemView::Mode_Table:       return treeView->haveUnSelectedItems();
    }
}

void PlayQueueView::clearSelection()
{
    if (groupedView && groupedView->selectionModel()) {
        groupedView->selectionModel()->clear();
    }
    if (treeView && treeView->selectionModel()) {
        treeView->selectionModel()->clear();
    }
}

QAbstractItemView * PlayQueueView::view() const
{
    switch (mode) {
    default:
    case ItemView::Mode_GroupedTree: return (QAbstractItemView *)groupedView;
    case ItemView::Mode_Table:       return (QAbstractItemView *)treeView;
    }
}

bool PlayQueueView::hasFocus() const
{
    return view()->hasFocus();
}

QModelIndexList PlayQueueView::selectedIndexes(bool sorted) const
{
    switch (mode) {
    default:
    case ItemView::Mode_GroupedTree: return groupedView->selectedIndexes(sorted);
    case ItemView::Mode_Table:       return treeView->selectedIndexes(sorted);
    }
}

QList<Song> PlayQueueView::selectedSongs() const
{
    const QModelIndexList selected = selectedIndexes();
    QList<Song> songs;

    for (const QModelIndex &idx: selected) {
        Song song=idx.data(Cantata::Role_Song).value<Song>();
        if (!song.file.isEmpty() && !song.hasProtocolOrIsAbsolute()) {
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

//    if (BI_None!=backgroundImageType) {
//        pal.setColor(QPalette::Base, Qt::transparent);
//    }
    if (groupedView) {
        #ifndef Q_OS_MAC
        groupedView->setPalette(pal);
        #endif
        groupedView->viewport()->setPalette(pal);
    }
    if (treeView) {
        #ifndef Q_OS_MAC
        treeView->setPalette(pal);
        #endif
        treeView->viewport()->setPalette(pal);
    }
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
    if (BI_Custom==backgroundImageType || !isVisible()) {
        setFade(1.0);
        update();
    } else {
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

void PlayQueueView::searchActive(bool a)
{
    view()->setProperty(ProxyStyle::constModifyFrameProp, a ? 0 : ProxyStyle::VF_Top);
}

void PlayQueueView::drawBackdrop(QWidget *widget, const QSize &size)
{
    if (BI_None==backgroundImageType) {
        return;
    }

    QPainter p(widget);

    p.fillRect(0, 0, size.width(), size.height(), QApplication::palette().color(topLevelWidget()->isActiveWindow() ? QPalette::Active : QPalette::Inactive, QPalette::Base));
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

#include "moc_playqueueview.cpp"
