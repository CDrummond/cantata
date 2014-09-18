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

#include "gui/covers.h"
#include "groupedview.h"
#include "mpd/mpdstatus.h"
#include "mpd/song.h"
#include "actionitemdelegate.h"
#include "basicitemdelegate.h"
#include "itemview.h"
#include "config.h"
#include "support/localize.h"
#include "gui/plurals.h"
#include "widgets/icons.h"
#include "widgets/ratingwidget.h"
#include "support/gtkstyle.h"
#ifdef Q_OS_MAC
#include "support/osxstyle.h"
#endif
#include "support/utils.h"
#include "models/roles.h"
#include <QStyledItemDelegate>
#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QLinearGradient>
#include <QAction>
#include <QDropEvent>
#include <QPixmap>

static int constCoverSize=32;
static int constIconSize=16;
static int constBorder=1;
static double sizeAdjust=1.25;

void GroupedView::setup()
{
    int height=QApplication::fontMetrics().height();
    sizeAdjust=Utils::touchFriendly() ? 1.5 : 1.25;

    if (height>17) {
        constCoverSize=(((int)((height*2)/4))*4);
        constIconSize=Icon::stdSize(((int)(height/4))*4);
        constBorder=constCoverSize>48 ? 2 : 1;
    } else {
        constCoverSize=32;
        constIconSize=16;
        constBorder=1;
    }
    constCoverSize*=sizeAdjust;
}

int GroupedView::coverSize()
{
    return constCoverSize;
}

int GroupedView::borderSize()
{
    return constBorder;
}

int GroupedView::iconSize()
{
    return constIconSize;
}

void GroupedView::drawPlayState(QPainter *painter, const QStyleOptionViewItem &option, const QRect &r, int state)
{
    if (state) {
        int size=constIconSize-7;
        int hSize=size/2;
        QRect ir(r.x()-(size+6), r.y()+(((r.height()-size)/2.0)+0.5), size, size);
        QColor inside(option.palette.color(QPalette::Text));
        QColor border=inside.red()>100 && inside.blue()>100 && inside.green()>100 ? Qt::black : Qt::white;
        if (QApplication::isRightToLeft()) {
            ir.adjust(r.width()-size, 0, r.width()-size, 0);
        }
        switch (state) {
        case GroupedView::State_Stopped:
            painter->fillRect(ir, border);
            painter->fillRect(ir.adjusted(1, 1, -1, -1), inside);
            break;
        case GroupedView::State_StopAfter: {
            ir.adjust(2, 0, -2, 0);
            QPoint p1[5]={ QPoint(ir.x()-2, ir.y()-1), QPoint(ir.x(), ir.y()-1), QPoint(ir.x()+(size-hSize), ir.y()+hSize), QPoint(ir.x(), ir.y()+(ir.height())), QPoint(ir.x()-2, ir.y()+(ir.height())) };
            QPoint p2[5]={ QPoint(ir.x()-1, ir.y()), QPoint(ir.x(), ir.y()), QPoint(ir.x()+(size-hSize)-1, ir.y()+hSize), QPoint(ir.x(), ir.y()+ir.height()-1), QPoint(ir.x()-1, ir.y()+ir.height()-1) };
            QPoint p3[3]={ QPoint(ir.x(), ir.y()+1), QPoint(ir.x()+(size-hSize)-2, ir.y()+hSize), QPoint(ir.x(), ir.y()+ir.height()-2) };
            painter->save();
            painter->setPen(border);
            painter->drawPolygon(p1, 5);
            painter->drawPolygon(p3, 3);
            painter->setPen(inside);
            painter->drawPolygon(p2, 5);
            painter->restore();
            break;
        }
        case GroupedView::State_StopAfterTrack:
            painter->setPen(border);
            painter->drawRect(ir.adjusted(0, 0, -1, -1));
            painter->drawRect(ir.adjusted(2, 2, -3, -3));
            painter->setPen(inside);
            painter->drawRect(ir.adjusted(1, 1, -2, -2));
            break;
        case GroupedView::State_Paused: {
            int blockSize=hSize-1;
            painter->fillRect(ir, border);
            painter->fillRect(ir.x()+1, ir.y()+1, blockSize, size-2, inside);
            painter->fillRect(ir.x()+size-blockSize-1, ir.y()+1, blockSize, size-2, inside);
            break;
        }
        case GroupedView::State_Playing: {
            ir.adjust(2, 0, -2, 0);
            QPoint p1[5]={ QPoint(ir.x()-2, ir.y()-1), QPoint(ir.x(), ir.y()-1), QPoint(ir.x()+(size-hSize), ir.y()+hSize), QPoint(ir.x(), ir.y()+(ir.height()-1)), QPoint(ir.x()-2, ir.y()+(ir.height()-1)) };
            QPoint p2[5]={ QPoint(ir.x()-2, ir.y()-1), QPoint(ir.x(), ir.y()-1), QPoint(ir.x()+(size-hSize), ir.y()+hSize), QPoint(ir.x(), ir.y()+ir.height()), QPoint(ir.x()-2, ir.y()+ir.height()) };
            painter->save();
            painter->setBrush(border);
            painter->setPen(border);
            painter->drawPolygon(p1, 5);
            painter->setBrush(inside);
            painter->drawPolygon(p2, 5);
            painter->restore();
            break;
        }
        }
    }
}

enum Type {
    AlbumHeader,
    AlbumTrack
};

static Type getType(const QModelIndex &index)
{
    QModelIndex prev=index.row()>0 ? index.sibling(index.row()-1, 0) : QModelIndex();
    quint16 thisKey=index.data(Cantata::Role_Key).toUInt();
    quint16 prevKey=prev.isValid() ? prev.data(Cantata::Role_Key).toUInt() : (quint16)Song::Null_Key;

    return thisKey==prevKey ? AlbumTrack : AlbumHeader;
}

static bool isAlbumHeader(const QModelIndex &index)
{
    return !index.data(Cantata::Role_IsCollection).toBool() && AlbumHeader==getType(index);
}

static QString streamText(const Song &song, const QString &trackTitle, bool useName=true)
{
    if (song.album.isEmpty() && song.albumArtist().isEmpty()) {
        QString songName=song.name();
        return song.title.isEmpty() && songName.isEmpty()
                ? song.file
                : !useName || songName.isEmpty()
                  ? song.title
                  : song.title.isEmpty()
                    ? songName
                    : (song.title + " - " + songName);
    } else if (!song.title.isEmpty() && !song.artist.isEmpty()) {
        QString name=song.name();
        return song.artist + " - " + (!useName || name.isEmpty()
                                        ? song.title
                                        : song.title.isEmpty()
                                            ? name
                                            : (song.title + " - " + name));
    } else {
        return trackTitle;
    }
}

class GroupedViewDelegate : public ActionItemDelegate
{
public:
    GroupedViewDelegate(GroupedView *p)
        : ActionItemDelegate(p)
        , view(p)
        , ratingPainter(0)
    {
    }

    virtual ~GroupedViewDelegate()
    {
        delete ratingPainter;
    }

    QSize sizeHint(Type type, bool isCollection) const
    {
        int textHeight = QApplication::fontMetrics().height()*sizeAdjust;

        if (isCollection || AlbumHeader==type) {
            return QSize(64, qMax(constCoverSize, (qMax(constIconSize, textHeight)*2)+constBorder)+(2*constBorder));
        }
        return QSize(64, qMax(constIconSize, textHeight)+(2*constBorder));
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (0==index.column()) {
            return sizeHint(getType(index), index.data(Cantata::Role_IsCollection).toBool());
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }

        Type type=getType(index);
        bool isCollection=index.data(Cantata::Role_IsCollection).toBool();
        Song song=index.data(Cantata::Role_Song).value<Song>();
        int state=index.data(Cantata::Role_Status).toInt();
        quint32 collection=index.data(Cantata::Role_CollectionId).toUInt();
        bool selected=option.state&QStyle::State_Selected;
        bool mouseOver=underMouse && option.state&QStyle::State_MouseOver;
        bool gtk=mouseOver && GtkStyle::isActive();
        bool rtl=QApplication::isRightToLeft();

        if (!isCollection && AlbumHeader==type) {
            if (mouseOver && gtk) {
                GtkStyle::drawSelection(option, painter, (selected ? 0.75 : 0.25)*0.75);
            } else {
                painter->save();
                painter->setOpacity(0.75);
                QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, view);
                painter->restore();
            }
            painter->save();
            painter->setClipRect(option.rect.adjusted(0, option.rect.height()/2, 0, 0), Qt::IntersectClip);
            if (mouseOver && gtk) {
                GtkStyle::drawSelection(option, painter, selected ? 0.75 : 0.25);
            } else {
                QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, view);
            }
            painter->restore();
            if (!state && !view->isExpanded(song.key, collection) && view->isCurrentAlbum(song.key)) {
                QVariant cs=index.data(Cantata::Role_CurrentStatus);
                if (cs.isValid()) {
                    state=cs.toInt();
                }
            }
        } else {
            if (mouseOver && gtk) {
                GtkStyle::drawSelection(option, painter, selected ? 0.75 : 0.25);
            } else {
                QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, view);
            }
        }
        QString title;
        QString track;
        QString duration=song.time>0 ? Utils::formatTime(song.time) : QString();
        bool stream=!isCollection && song.isStandardStream();
        bool isEmpty=song.title.isEmpty() && song.artist.isEmpty() && !song.file.isEmpty();
        QString trackTitle=isEmpty
                        ? song.file
                        : song.diffArtist()
                            ? song.artistSong()
                            : song.title;
        QFont f(QApplication::font());
        if (stream) {
            f.setItalic(true);
        }
        QFontMetrics fm(f);
        int textHeight=fm.height()*sizeAdjust;


        if (isCollection) {
            title=index.data(Qt::DisplayRole).toString();
        } else if (AlbumHeader==type) {
            if (stream) {
                QModelIndex next=index.sibling(index.row()+1, 0);
                quint16 nextKey=next.isValid() ? next.data(Cantata::Role_Key).toUInt() : (quint16)Song::Null_Key;
                if (nextKey!=song.key && !song.name().isEmpty()) {
                    title=song.name();
                    track=streamText(song, trackTitle, false);
                } else {
                    title=song.isCdda() ? i18n("Audio CD") : i18n("Streams");
                    track=streamText(song, trackTitle);
                }
            } else if (isEmpty) {
                title=Song::unknown();
                track=trackTitle;
            } else if (song.album.isEmpty()) {
                title=song.artistOrComposer();
                track=song.trackAndTitleStr();
            } else {
                if (song.isFromOnlineService()) {
                    title=Song::displayAlbum(song.album, Song::albumYear(song));
                } else {
                    title=song.artistOrComposer()+QLatin1String(" - ")+Song::displayAlbum(song.album, Song::albumYear(song));
                }
                if (Song::useComposer()) {
                    while (title.contains(") (")) {
                        title=title.replace(") (", ", ");
                    }
                }
                track=song.trackAndTitleStr();
            }
        } else {
            if (stream) {
                track=streamText(song, trackTitle);
            } else {
                track=song.trackAndTitleStr();
            }
        }

        if (song.priority>0) {
            track=track+QLatin1String(" [")+QString::number(song.priority)+QChar(']');
        }

        painter->save();
        painter->setFont(f);
        QColor col(option.palette.color(option.state&QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
        QTextOption textOpt(Qt::AlignVCenter);
        QRect r(option.rect.adjusted(constBorder+4, constBorder, -(constBorder+4), -constBorder));

        if (state && GroupedView::State_StopAfterTrack!=state) {
            QRectF border(option.rect.x()+1.5, option.rect.y()+1.5, option.rect.width()-3, option.rect.height()-3);
            if (!title.isEmpty()) {
                border.adjust(0, textHeight+constBorder, 0, 0);
            }
            QLinearGradient g(border.topLeft(), border.bottomLeft());
            #ifdef Q_OS_MAC
            QColor gradCol(OSXStyle::self()->viewPalette().color(QPalette::Highlight));
            QColor borderCol(OSXStyle::self()->viewPalette().color(QPalette::HighlightedText));
            #else
            QColor gradCol(QApplication::palette().color(QPalette::Highlight));
            QColor borderCol(QApplication::palette().color(QPalette::HighlightedText));
            #endif
            gradCol.setAlphaF(option.state&QStyle::State_Selected ? 0.6 : 0.45);
            g.setColorAt(0, gradCol.dark(165));
            g.setColorAt(1, gradCol.light(165));
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->fillPath(Utils::buildPath(border, 3), g);
            painter->setPen(QPen(gradCol, 1));
            painter->drawPath(Utils::buildPath(border.adjusted(-1, -1, 1, 1), 3.5));
            painter->setPen(QPen(borderCol, 1));
            painter->drawPath(Utils::buildPath(border, 3));
            painter->setRenderHint(QPainter::Antialiasing, false);
        }

        painter->setPen(col);
        bool showTrackDuration=!duration.isEmpty();

        if (isCollection || AlbumHeader==type) {
            QPixmap pix;
            // Draw cover...
            if (isCollection) {
                pix=index.data(Qt::DecorationRole).value<QIcon>().pixmap(constCoverSize, constCoverSize);
            } else {
                QPixmap *cover=/*stream ? 0 : */Covers::self()->get(song, constCoverSize);
                pix=cover ? *cover : (stream && !song.isCdda() ? Icons::self()->streamIcon : Icons::self()->albumIcon).pixmap(constCoverSize, constCoverSize);
            }

            #if QT_VERSION >= 0x050100
            int maxSize=constCoverSize*pix.devicePixelRatio();
            #else
            int maxSize=constCoverSize;
            #endif

            if (pix.width()>maxSize) {
                pix=pix.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }

            #if QT_VERSION >= 0x050100
            QSize pixSize = pix.isNull() ? QSize(0, 0) : (pix.size() / pix.devicePixelRatio());
            #else
            QSize pixSize = pix.size();
            #endif

            if (rtl) {
                painter->drawPixmap(r.x()+r.width()-(pixSize.width()-constBorder), r.y()+((r.height()-pixSize.height())/2), pixSize.width(), pixSize.height(), pix);
                r.adjust(0, 0, -(constCoverSize+constBorder), 0);
            } else {
                painter->drawPixmap(r.x()-2, r.y()+((r.height()-pixSize.height())/2), pixSize.width(), pixSize.height(), pix);
                r.adjust(constCoverSize+constBorder, 0, 0, 0);
            }

            int td=index.data(Cantata::Role_AlbumDuration).toUInt();
            QString totalDuration=td>0 ? Utils::formatTime(td) : QString();
            QRect duratioRect(r.x(), r.y(), r.width(), textHeight);
            int totalDurationWidth=fm.width(totalDuration)+8;
            QRect textRect(r.x(), r.y(), r.width()-(rtl ? (4*constBorder) : totalDurationWidth), textHeight);
            QFont tf(f);
            tf.setBold(true);
            title = QFontMetrics(tf).elidedText(title, Qt::ElideRight, textRect.width(), QPalette::WindowText);
            painter->setFont(tf);
            painter->drawText(textRect, title, textOpt);
            if (!totalDuration.isEmpty()) {
                painter->drawText(duratioRect, totalDuration, QTextOption(Qt::AlignVCenter|Qt::AlignRight));
            }
            BasicItemDelegate::drawLine(painter, r.adjusted(0, 0, 0, -r.height()/2), col, rtl, !rtl, 0.45);
            r.adjust(0, textHeight+constBorder, 0, 0);
            r=QRect(r.x(), r.y()+r.height()-(textHeight+1), r.width(), textHeight);
            painter->setFont(f);
            if (rtl) {
                r.adjust(0, 0, (constCoverSize-(constBorder*3)), 0);
            }

            if (isCollection || !view->isExpanded(song.key, collection)) {
                showTrackDuration=false;
                track=Plurals::tracks(index.data(Cantata::Role_SongCount).toUInt());
            }
        } else if (rtl) {
            r.adjust(0, 0, -(constBorder*4), 0);
        } else {
            r.adjust(constCoverSize+constBorder, 0, 0, 0);
        }

        GroupedView::drawPlayState(painter, option, r, state);
        painter->setPen(col);

        int ratingsStart=rtl ? 0 : drawRatings(painter, song, r, fm, option.palette.color(QPalette::Active, QPalette::Text)); // TODO!!!

        int durationWidth=showTrackDuration ? fm.width(duration)+8 : 0;
        QRect duratioRect(r.x(), r.y(), r.width(), textHeight);
        QRect textRect(r.x(), r.y(), r.width()-durationWidth, textHeight);
        if (ratingsStart>0) {
            textRect.setWidth(ratingsStart-r.x());
        }
        track = fm.elidedText(track, Qt::ElideRight, textRect.width(), QPalette::WindowText);
        painter->drawText(textRect, track, textOpt);
        if (showTrackDuration) {
            painter->drawText(duratioRect, duration, QTextOption(Qt::AlignVCenter|Qt::AlignRight));
        }

        if (mouseOver || Utils::touchFriendly()) {
            drawIcons(painter, option.rect, mouseOver || (selected && Utils::touchFriendly()), rtl, AlbumHeader==type || isCollection ? AP_HBottom : AP_HMiddle, index);
        }
        BasicItemDelegate::drawLine(painter, option.rect, col);
        painter->restore();
    }

    int drawRatings(QPainter *painter, const Song &song, const QRect &r, const QFontMetrics &fm, const QColor &col) const
    {
        if (song.rating>0 && song.rating<=Song::Rating_Max) {
            if (!ratingPainter) {
                ratingPainter=new RatingPainter(r.height()-(constBorder*8));
                ratingPainter->setColor(col);
            }

            painter->save();
            painter->setOpacity(painter->opacity()*0.75);
            const QSize &ratingSize=ratingPainter->size();
            int spacing=constBorder*2;
            int durationWidth=fm.width("0:00:00")+spacing;
            QRect ratingRect(r.x()+r.width()-(durationWidth+ratingSize.width()+spacing),
                             r.y()+(r.height()-ratingSize.height())/2,
                             ratingSize.width(), ratingSize.height());
            ratingPainter->paint(painter, ratingRect, song.rating);
            painter->restore();
            return ratingRect.x()-spacing;
        }
        return 0;
    }

private:
    GroupedView *view;
    mutable RatingPainter *ratingPainter;
};

GroupedView::GroupedView(QWidget *parent, bool isPlayQueue)
    : TreeView(parent, isPlayQueue)
    , allowClose(!Utils::touchFriendly())
    , startClosed(allowClose)
    , autoExpand(true)
    , filterActive(false)
    , isMultiLevel(false)
    , currentAlbum(Song::Null_Key)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    setDragDropOverwriteMode(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDropIndicatorShown(true);
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setSelectionBehavior(SelectRows);
    setForceSingleColumn(true);
    connect(this, SIGNAL(clicked(const QModelIndex &)), this, SLOT(itemClicked(const QModelIndex &)));
    setStyleSheet("QTreeView::branch { border: 0px; }");
    GroupedViewDelegate *delegate=new GroupedViewDelegate(this);
    setItemDelegate(delegate);
    if (isPlayQueue) {
        // 'underMouse' is used to work-around mouse over issues with some styles.
        // PlayQueue does not have mouse over - so we never need to check this.
        delegate->setUnderMouse(true);
    }
}

GroupedView::~GroupedView()
{
}

void GroupedView::setModel(QAbstractItemModel *model)
{
    TreeView::setModel(model);
    if (model) {
        if (startClosed) {
            updateCollectionRows();
        }
        connect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
    } else {
        controlledAlbums.clear();
        disconnect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
    }
}

void GroupedView::setFilterActive(bool f)
{
    if (f==filterActive) {
        return;
    }
    filterActive=f;
    if (filterActive && model()) {
        quint32 count=model()->rowCount();
        for (quint32 i=0; i<count; ++i) {
            setRowHidden(i, QModelIndex(), false);
            if (isMultiLevel) {
                QModelIndex idx=model()->index(i, 0);
                if (model()->hasChildren(idx)) {
                    quint32 childCount=model()->rowCount(idx);
                    for (quint32 c=0; c<childCount; ++c) {
                        setRowHidden(c, idx, false);
                    }
                }
            }
        }
    }
}

void GroupedView::setStartClosed(bool sc)
{
    if (!allowClose || sc==startClosed) {
        return;
    }
    controlledAlbums.clear();
    startClosed=sc;
    if (startClosed) {
        updateCollectionRows();
    }
}

void GroupedView::updateRows(qint32 row, quint16 curAlbum, bool scroll, const QModelIndex &parent, bool forceScroll)
{
    currentAlbum=curAlbum;

    if (row<0) {
        row=0;
    }

    if (!model() || row>model()->rowCount(parent)) {
        return;
    }

    if (filterActive && model() && MPDState_Playing==MPDStatus::self()->state()) {
        if (scroll) {
            scrollTo(model()->index(row, 0, parent), QAbstractItemView::PositionAtCenter);
        }
        return;
    }

    updateRows(parent);
    if (scroll && (MPDState_Playing==MPDStatus::self()->state() || forceScroll)) {
        scrollTo(model()->index(row, 0, parent), QAbstractItemView::PositionAtCenter);
    }
}

void GroupedView::updateRows(const QModelIndex &parent)
{
    if (!model()) {
        return;
    }

    qint32 count=model()->rowCount(parent);
    quint16 lastKey=Song::Null_Key;
    quint32 collection=parent.data(Cantata::Role_CollectionId).toUInt();
    QSet<quint16> keys;

    for (qint32 i=0; i<count; ++i) {
        quint16 key=model()->index(i, 0, parent).data(Cantata::Role_Key).toUInt();
        keys.insert(key);
        bool hide=key==lastKey &&
                        !(key==currentAlbum && autoExpand) &&
                        ( ( startClosed && !controlledAlbums[collection].contains(key)) ||
                          ( !startClosed && controlledAlbums[collection].contains(key)));
        setRowHidden(i, parent, hide);
        lastKey=key;
    }

    // Check that 'controlledAlbums' only contains valid keys...
    controlledAlbums[collection].intersect(keys);
}

void GroupedView::updateCollectionRows()
{
    if (!model()) {
        return;
    }

    currentAlbum=Song::Null_Key;
    qint32 count=model()->rowCount();
    for (int i=0; i<count; ++i) {
        updateRows(model()->index(i, 0));
    }
}

void GroupedView::toggle(const QModelIndex &idx)
{
    if (!allowClose) {
        return;
    }

    quint16 indexKey=idx.data(Cantata::Role_Key).toUInt();

    if (indexKey==currentAlbum && autoExpand) {
        return;
    }

    quint32 collection=idx.data(Cantata::Role_CollectionId).toUInt();
    bool toBeHidden=false;
    if (controlledAlbums[collection].contains(indexKey)) {
        controlledAlbums[collection].remove(indexKey);
        toBeHidden=startClosed;
    } else {
        controlledAlbums[collection].insert(indexKey);
        toBeHidden=!startClosed;
    }

    if (model()) {
        QModelIndex parent=idx.parent();
        quint32 count=model()->rowCount(idx.parent());
        for (quint32 i=0; i<count; ++i) {
            QModelIndex index=model()->index(i, 0, parent);
            quint16 key=index.data(Cantata::Role_Key).toUInt();
            if (indexKey==key) {
                if (isAlbumHeader(index)) {
                    dataChanged(index, index);
                } else {
                    setRowHidden(i, parent, toBeHidden);
                }
            }
        }
    }
}

// Calculate list of selected indexes. If a collapsed album is selected, we also pretend all of its tracks
// are selected.
QModelIndexList GroupedView::selectedIndexes(bool sorted) const
{
    QModelIndexList indexes = TreeView::selectedIndexes(sorted);
    QSet<QModelIndex> indexSet;
    QModelIndexList sel;

    foreach (const QModelIndex &idx, indexes) {
        if (!indexSet.contains(idx)) {
            indexSet.insert(idx);
            sel.append(idx);
        }
        if (!idx.data(Cantata::Role_IsCollection).toBool()) {
            quint16 key=idx.data(Cantata::Role_Key).toUInt();
            quint32 collection=idx.data(Cantata::Role_CollectionId).toUInt();
            if (!isExpanded(key, collection)) {
                quint32 rowCount=model()->rowCount(idx.parent());
                for (quint32 i=idx.row()+1; i<rowCount; ++i) {
                    QModelIndex next=idx.sibling(i, 0);
                    if (next.isValid()) {
                        quint16 nextKey=next.data(Cantata::Role_Key).toUInt();
                        quint32 nextCollection=idx.data(Cantata::Role_CollectionId).toUInt();

                        if (nextCollection==collection && nextKey==key) {
                            if (!indexSet.contains(next)) {
                                indexSet.insert(next);
                                sel.append(next);
                            }
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }
        }
    }
    return sel;
}

// If we are dropping onto a collapsed album, and we are in the bottom 1/2 of the row - then
// we need to drop the item at the end of the album's tracks. So, we adjust the 'drop row'
// by the amount of tracks.
void GroupedView::dropEvent(QDropEvent *event)
{
    QModelIndex parent;
    quint32 dropRowAdjust=0;
    if (model() && viewport()->rect().contains(event->pos())) {
        // Dont allow to drop on an already selected row - as this seems to cuase a crash!!!
        QModelIndex idx=TreeView::indexAt(event->pos());
        if (idx.isValid() && selectionModel() && selectionModel()->isSelected(idx)) {
            return;
        }
        if (idx.isValid() && isAlbumHeader(idx)) {
            QRect rect(visualRect(idx));
            if (event->pos().y()>(rect.y()+(rect.height()/2))) {
                quint16 key=idx.data(Cantata::Role_Key).toUInt();
                quint32 collection=idx.data(Cantata::Role_CollectionId).toUInt();
                if (!isExpanded(key, collection)) {
                    parent=idx.parent();
                    quint32 rowCount=model()->rowCount(parent);
                    for (quint32 i=idx.row()+1; i<rowCount; ++i) {
                        QModelIndex next=idx.sibling(i, 0);
                        if (next.isValid()) {
                            quint16 nextKey=next.data(Cantata::Role_Key).toUInt();
                            if (nextKey==key) {
                                dropRowAdjust++;
                            } else {
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                    model()->setData(parent, dropRowAdjust, Cantata::Role_DropAdjust);
                }
            }
        }
    }

    TreeView::dropEvent(event);
    model()->setData(parent, 0, Cantata::Role_DropAdjust);
}

void GroupedView::coverLoaded(const Song &song, int size)
{
    if (filterActive || !isVisible() || size!=constCoverSize || song.isArtistImageRequest()) {
        return;
    }
    quint32 count=model()->rowCount();
    quint16 lastKey=Song::Null_Key;
    QString albumArtist=song.albumArtist();
    QString album=song.album;

    for (quint32 i=0; i<count; ++i) {
        QModelIndex index=model()->index(i, 0);
        if (!index.isValid()) {
            continue;
        }
        if (isMultiLevel && model()->hasChildren(index)) {
            lastKey=Song::Null_Key;
            quint32 childCount=model()->rowCount(index);
            for (quint32 c=0; c<childCount; ++c) {
                QModelIndex child=model()->index(c, 0, index);
                if (!child.isValid()) {
                    continue;
                }
                quint16 key=child.data(Cantata::Role_Key).toUInt();

                if (key!=lastKey && !isRowHidden(c, index)) {
                    Song song=child.data(Cantata::Role_Song).value<Song>();
                    if (song.albumArtist()==albumArtist && song.album==album) {
                        dataChanged(child, child);
                    }
                }
                lastKey=key;
            }
        } else {
            quint16 key=index.data(Cantata::Role_Key).toUInt();

            if (key!=lastKey && !isRowHidden(i, QModelIndex())) {
                Song song=index.data(Cantata::Role_Song).value<Song>();
                if (song.albumArtist()==albumArtist && song.album==album) {
                    dataChanged(index, index);
                }
            }
            lastKey=key;
        }
    }
}

void GroupedView::collectionRemoved(quint32 key)
{
    controlledAlbums.remove(key);
}

void GroupedView::itemClicked(const QModelIndex &idx)
{
    if (isAlbumHeader(idx)) {
        QRect indexRect(visualRect(idx));
        QRect icon(indexRect.x()+constBorder+4, indexRect.y()+constBorder+((indexRect.height()-constCoverSize)/2),
                   constCoverSize, constCoverSize);
        QRect header(indexRect);
        header.setHeight(header.height()/2);
        header.moveTo(viewport()->mapToGlobal(QPoint(header.x(), header.y())));
        icon.moveTo(viewport()->mapToGlobal(QPoint(icon.x(), icon.y())));
        if (allowClose && icon.contains(QCursor::pos())) {
            toggle(idx);
        } else if (header.contains(QCursor::pos())) {
            QModelIndexList list;
            unsigned int key=idx.data(Cantata::Role_Key).toUInt();
            QModelIndex i=idx.sibling(idx.row()+1, 0);
//            QModelIndexList sel=selectedIndexes();
            QItemSelectionModel *selModel=selectionModel();
            QModelIndexList unsel;

            while (i.isValid() && i.data(Cantata::Role_Key).toUInt()==key) {
                #if 0 // The following does not seem to work from the grouped playlist view - the 2nd row never get selected!
                if (!sel.contains(i)) {
                    unsel.append(i);
                }
                #else
                unsel.append(i);
                #endif
                list.append(i);
                i=i.sibling(i.row()+1, 0);
            }
            if (list.count()) {
                #if 0 // Commendted out as (as noted below) unselection is not working, and we always add to 'unsel' above (because of playlists)
                if (unsel.isEmpty()) { // TODO: This is not working!!!   CHECK selModel if re-add!!!
                    foreach(const QModelIndex &i, list) {
                        selModel->select(i, QItemSelectionModel::Deselect|QItemSelectionModel::Rows);
                    }
                } else {
                    foreach(const QModelIndex &i, unsel) {
                        selModel->select(i, QItemSelectionModel::Select|QItemSelectionModel::Rows);
                    }
                }
                #else
                if (!unsel.isEmpty() && selModel) {
                    foreach(const QModelIndex &i, unsel) {
                        selModel->select(i, QItemSelectionModel::Select|QItemSelectionModel::Rows);
                    }
                }
                #endif
            }
        }
    }
}

void GroupedView::expand(const QModelIndex &idx, bool singleOnly)
{
    if (allowClose && idx.isValid()) {
        if (idx.data(Cantata::Role_IsCollection).toBool()) {
            setExpanded(idx, true);
            if (!singleOnly) {
                quint32 count=model()->rowCount(idx);
                for (quint32 i=0; i<count; ++i) {
                    expand(idx.child(i, 0));
                }
            }
        } else if (AlbumHeader==getType(idx)) {
            quint16 indexKey=idx.data(Cantata::Role_Key).toUInt();
            quint32 collection=idx.data(Cantata::Role_CollectionId).toUInt();
            if (!isExpanded(indexKey, collection)) {
                toggle(idx);
            }
        }
    }
}

void GroupedView::collapse(const QModelIndex &idx, bool singleOnly)
{
    if (allowClose && idx.isValid()) {
        if (idx.data(Cantata::Role_IsCollection).toBool()) {
            setExpanded(idx, false);
            if (!singleOnly) {
                quint32 count=model()->rowCount(idx);
                for (quint32 i=0; i<count; ++i) {
                    collapse(idx.child(i, 0));
                }
            }
        }
    }
}
