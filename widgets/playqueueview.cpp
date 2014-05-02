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
#include "currentcover.h"
#include "groupedview.h"
#include "treeview.h"
#include "settings.h"
#include "mpdstatus.h"
#include "localize.h"
#include "spinner.h"
#include "messageoverlay.h"
#include "basicitemdelegate.h"
#include "icons.h"
#include "gtkstyle.h"
#include "actioncollection.h"
#include "action.h"
#include "roles.h"
#include <QFile>
#include <QPainter>
#include <QApplication>
#include <qglobal.h>

// Exported by QtGui
void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);

class PlayQueueListViewItemDelegate : public BasicItemDelegate
{
public:
    PlayQueueListViewItemDelegate(QObject *p) : BasicItemDelegate(p) { }
    virtual ~PlayQueueListViewItemDelegate() { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (0==index.column()) {            
            int imageSize = GroupedView::coverSize();
            int bordeSize = GroupedView::borderSize();
            int textHeight = QApplication::fontMetrics().height()*2;
            return QSize(qMax(64, imageSize) + (bordeSize * 2), qMax(textHeight, imageSize) + (bordeSize*2));
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }

    static inline double subTextAlpha(bool selected)
    {
        return selected ? 0.7 : 0.6;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }
        int state=index.data(Cantata::Role_Status).toInt();
        bool mouseOver=option.state&QStyle::State_MouseOver;
        bool gtk=mouseOver && GtkStyle::isActive();
        bool selected=option.state&QStyle::State_Selected;
        bool active=option.state&QStyle::State_Active;
        bool drawBgnd=true;
        QStyleOptionViewItemV4 opt(option);
        opt.showDecorationSelected=true;

        if (!underMouse) {
            if (mouseOver && !selected) {
                drawBgnd=false;
            }
            mouseOver=false;
        }

        if (drawBgnd) {
            if (mouseOver && gtk) {
                GtkStyle::drawSelection(opt, painter, selected ? 0.75 : 0.25);
            } else {
                QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, 0L);
            }
        }

        Song song=index.data(Cantata::Role_Song).value<Song>();
        QString text = index.data(Cantata::Role_MainText).toString();
        QRect r(option.rect);
        QString childText = index.data(Cantata::Role_SubText).toString();
        QString duration = index.data(Cantata::Role_Time).toString();
        int coverSize = GroupedView::coverSize();
        int borderSize = GroupedView::borderSize();
        bool stream=song.isStandardStream();
        QPixmap *cover= stream ? 0 : Covers::self()->get(song, coverSize);
        QPixmap pix=cover ? *cover : (stream && !song.isCdda() ? Icons::self()->streamIcon : Icons::self()->albumIcon).pixmap(coverSize, coverSize);
        bool rtl = Qt::RightToLeft==QApplication::layoutDirection();

        painter->save();
        painter->setClipRect(r);

        QFont textFont(index.data(Qt::FontRole).value<QFont>());
        QFontMetrics textMetrics(textFont);
        int textHeight=textMetrics.height();
        int adjust=qMax(pix.width(), pix.height());

        r.adjust(borderSize, 0, -borderSize, 0);

        if (rtl) {
            painter->drawPixmap(r.x()+r.width()-pix.width(), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
            r.adjust(3, 0, -(3+adjust), 0);
        } else {
            painter->drawPixmap(r.x(), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
            r.adjust(adjust+3, 0, -3, 0);
        }

        QRect textRect;
        QColor color(option.palette.color(active ? QPalette::Active : QPalette::Inactive, selected ? QPalette::HighlightedText : QPalette::Text));
        QTextOption textOpt(Qt::AlignVCenter);
        textOpt.setWrapMode(QTextOption::NoWrap);

        QFont childFont(Utils::smallFont(QApplication::font()));
        QFontMetrics childMetrics(childFont);
        QRect childRect;

        int childHeight=childMetrics.height();
        int totalHeight=textHeight+childHeight;
        int durationWidth=duration.isEmpty() ? 0 : childMetrics.width(duration)+8;
        textRect=QRect(r.x(), r.y()+((r.height()-totalHeight)/2), r.width(), textHeight);
        childRect=QRect(r.x(), r.y()+textHeight+((r.height()-totalHeight)/2), r.width()-durationWidth, textHeight);

        text = textMetrics.elidedText(text, Qt::ElideRight, textRect.width(), QPalette::WindowText);
        painter->setPen(color);
        painter->setFont(textFont);
        painter->drawText(textRect, text, textOpt);

        childText = childMetrics.elidedText(childText, Qt::ElideRight, childRect.width(), QPalette::WindowText);
        color.setAlphaF(subTextAlpha(selected));
        painter->setPen(color);
        painter->setFont(childFont);
        painter->drawText(childRect, childText, textOpt);
        if (!duration.isEmpty()) {
            painter->drawText(QRect(childRect.x(), childRect.y(), r.width(), childRect.height()), duration, QTextOption(Qt::AlignVCenter|Qt::AlignRight));
        }

        GroupedView::drawPlayState(painter, option, textRect, state);
        BasicItemDelegate::drawLine(painter, option.rect, color);

        painter->restore();
    }
};

PlayQueueListView::PlayQueueListView(PlayQueueView *parent)
    : ListView(parent)
    , view(parent)
{
    setItemDelegate(new PlayQueueListViewItemDelegate(this));
}

void PlayQueueListView::paintEvent(QPaintEvent *e)
{
    view->drawBackdrop(viewport(), size());
    ListView::paintEvent(e);
}

void PlayQueueListView::setModel(QAbstractItemModel *model)
{
    ListView::setModel(model);
    if (model) {
        connect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
    } else {
        disconnect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
    }
}

void PlayQueueListView::coverLoaded(const Song &song, int size)
{
    if (!isVisible() || size!=GroupedView::coverSize() || song.isArtistImageRequest()) {
        return;
    }
    quint32 count=model()->rowCount();
    quint16 lastKey=Song::constNullKey;
    QString albumArtist=song.albumArtist();
    QString album=song.album;
    int rowFrom=-1;
    int rowTo=-1;

    for (quint32 i=0; i<count; ++i) {
        QModelIndex index=model()->index(i, 0);
        if (!index.isValid()) {
            continue;
        }
        quint16 key=index.data(Cantata::Role_Key).toUInt();

        if (key==lastKey) {
            rowTo=i;
        } else {
            if (-1!=rowTo && -1!=rowFrom) {
                dataChanged(model()->index(rowFrom, 0), model()->index(rowTo, 0));
                rowTo=-1;
            }
            Song song=index.data(Cantata::Role_Song).value<Song>();
            if (song.albumArtist()==albumArtist && song.album==album) {
                rowFrom=rowTo=i;
            } else {
                rowFrom=rowTo=-1;
            }
            lastKey=key;
        }
    }
    if (-1!=rowTo && -1!=rowFrom) {
        dataChanged(model()->index(rowFrom, 0), model()->index(rowTo, 0));
    }
}

class PlayQueueTreeViewItemDelegate : public BasicItemDelegate
{
public:
    PlayQueueTreeViewItemDelegate(QObject *p) : BasicItemDelegate(p) { }
    virtual ~PlayQueueTreeViewItemDelegate() { }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }

        QStyleOptionViewItemV4 v4((QStyleOptionViewItemV4 &)option);
        if (QStyleOptionViewItemV4::Beginning==v4.viewItemPosition) {
            v4.icon=index.data(Cantata::Role_Decoration).value<QIcon>();
            if (!v4.icon.isNull()) {
                v4.features |= QStyleOptionViewItemV2::HasDecoration;
                v4.decorationSize=v4.icon.actualSize(option.decorationSize, QIcon::Normal, QIcon::Off);
            }
        }

        BasicItemDelegate::paint(painter, v4, index);
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
    setItemDelegate(new PlayQueueTreeViewItemDelegate(this));
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
    , listView(0)
    , groupedView(0)
    , treeView(0)
    , spinner(0)
    , msgOverlay(0)
    , backgroundImageType(BI_None)
    , backgroundOpacity(15)
    , backgroundBlur(0)
{
    removeFromAction = ActionCollection::get()->createAction("removefromplaylist", i18n("Remove From Play Queue"), "list-remove");
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
    setMode((ItemView::Mode)Settings::self()->playQueueView());
    switch (backgroundImageType) {
    case BI_None:
        if (origType!=backgroundImageType) {
            updatePalette();
            previousBackground=QPixmap();
            curentCover=QImage();
            curentBackground=QPixmap();
            view()->viewport()->update();
            if (!CurrentCover::self()->image().isNull()) {
                setImage(CurrentCover::self()->image());
            }
        }
        break;
    case BI_Cover:
        if (BI_None==origType) {
            updatePalette();
        }
        if ((origType!=backgroundImageType || backgroundOpacity!=origOpacity || backgroundBlur!=origBlur) && !CurrentCover::self()->image().isNull()) {
            setImage(CurrentCover::self()->image());
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
    if (m==mode || (ItemView::Mode_GroupedTree!=m && ItemView::Mode_Table!=m && ItemView::Mode_List!=m)) {
        return;
    }

    if (ItemView::Mode_Table==mode) {
        treeView->saveHeader();
    }

    switch (m) {
    case ItemView::Mode_List:
        if (!listView) {
            listView=new PlayQueueListView(this);
            listView->setContextMenuPolicy(Qt::ActionsContextMenu);
            listView->installEventFilter(new DeleteKeyEventHandler(listView, removeFromAction));
            addWidget(listView);
            connect(listView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
            connect(listView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
            updatePalette();
        }
    case ItemView::Mode_GroupedTree:
        if (!groupedView) {
            groupedView=new PlayQueueGroupedView(this);
            groupedView->setIndentation(0);
            groupedView->setItemsExpandable(false);
            groupedView->setExpandsOnDoubleClick(false);
            groupedView->setContextMenuPolicy(Qt::ActionsContextMenu);
            groupedView->installEventFilter(new DeleteKeyEventHandler(groupedView, removeFromAction));
            addWidget(groupedView);
            connect(groupedView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
            connect(groupedView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
            updatePalette();
        }
        break;
    case ItemView::Mode_Table:
        if (!treeView) {
            treeView=new PlayQueueTreeView(this);
            treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
            treeView->installEventFilter(new DeleteKeyEventHandler(treeView, removeFromAction));
            treeView->initHeader();
            addWidget(treeView);
            connect(treeView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
            connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
            updatePalette();
        }
    default:
        break;
    }

    QAbstractItemModel *model=0;
    QList<QAction *> actions;
    if (ItemView::Mode_Count!=mode) {
        QAbstractItemView *v=view();
        model=v->model();
        v->setModel(0);
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
    case ItemView::Mode_List:        return listView->haveSelectedItems();
    case ItemView::Mode_GroupedTree: return groupedView->haveSelectedItems();
    case ItemView::Mode_Table:       return treeView->haveSelectedItems();
    }
}

bool PlayQueueView::haveUnSelectedItems()
{
    switch (mode) {
    default:
    case ItemView::Mode_List:        return listView->haveUnSelectedItems();
    case ItemView::Mode_GroupedTree: return groupedView->haveUnSelectedItems();
    case ItemView::Mode_Table:       return treeView->haveUnSelectedItems();
    }
}

void PlayQueueView::clearSelection()
{
    if (listView && listView->selectionModel()) {
        listView->selectionModel()->clear();
    }
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
    case ItemView::Mode_List:        return (QAbstractItemView *)listView;
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
    case ItemView::Mode_List:        return listView->selectedIndexes(sorted);
    case ItemView::Mode_GroupedTree: return groupedView->selectedIndexes(sorted);
    case ItemView::Mode_Table:       return treeView->selectedIndexes(sorted);
    }
}

QList<Song> PlayQueueView::selectedSongs() const
{
    const QModelIndexList selected = selectedIndexes();
    QList<Song> songs;

    foreach (const QModelIndex &idx, selected) {
        Song song=idx.data(Cantata::Role_Song).value<Song>();
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
    if (listView) {
        listView->setPalette(pal);
        listView->viewport()->setPalette(pal);
    }
    if (groupedView) {
        groupedView->setPalette(pal);
        groupedView->viewport()->setPalette(pal);
    }
    if (treeView) {
        treeView->setPalette(pal);
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
