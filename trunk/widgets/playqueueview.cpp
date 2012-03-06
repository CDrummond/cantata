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
#include "listview.h"
#include "treeview.h"
#include "settings.h"
#include "mpdstatus.h"
#include "httpserver.h"
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QApplication>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>
#include <QtGui/QHeaderView>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#include <QtGui/QDropEvent>
#include <QtGui/QScrollBar>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

static const int constCoverSize=32;
static const int constIconSize=16;
static const int constBorder=1;

enum Type {
    AlbumHeader,
    AlbumTrack
};

class PlayQueueListView : public ListView
{
public:
    PlayQueueListView(QWidget *parent=0);
    virtual ~PlayQueueListView();

    void setFilterActive(bool f);
    bool isFilterActive() const { return filterActive; }
    void setAutoCollapsingEnabled(bool ac);
    bool isAutoCollapsingEnabled() const { return autoCollapsingEnabled; }
    QSet<qint32> getControlledSongIds() const { return controlledSongIds; }
    void setControlled(const QSet<quint16> &keys) { controlledAlbums=keys; }
    void updateRows(qint32 row, bool scroll);
    bool isExpanded(quint16 key) const { return filterActive || currentAlbum==key ||
                                                (autoCollapsingEnabled && controlledAlbums.contains(key)) ||
                                                (!autoCollapsingEnabled && !controlledAlbums.contains(key)); }
    void toggle(const QModelIndex &idx);
    QModelIndexList selectedIndexes() const;
    void dropEvent(QDropEvent *event);

private:
    bool autoCollapsingEnabled;
    bool filterActive;
    quint16 currentAlbum;
    QSet<qint32> controlledSongIds;
    QSet<quint16> controlledAlbums;
};

static Type getType(const QModelIndex &index)
{
    QModelIndex prev=index.row()>0 ? index.sibling(index.row()-1, 0) : QModelIndex();
    quint16 thisKey=index.data(PlayQueueView::Role_Key).toUInt();
    quint16 prevKey=prev.isValid() ? prev.data(PlayQueueView::Role_Key).toUInt() : Song::constNullKey;

    return thisKey==prevKey ? AlbumTrack : AlbumHeader;
}

class PlayQueueDelegate : public QStyledItemDelegate
{
public:
    PlayQueueDelegate(PlayQueueListView *p)
        : QStyledItemDelegate(p)
        , view(p)
    {
    }

    virtual ~PlayQueueDelegate()
    {
    }

    QSize sizeHint(Type type) const
    {
        int textHeight = QApplication::fontMetrics().height();

        if (AlbumHeader==type) {
            return QSize(64, qMax(constCoverSize, (qMax(constIconSize, textHeight)*2))+(3*constBorder));
        }
        return QSize(64, qMax(constIconSize, textHeight)+(2*constBorder));
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (0==index.column()) {
            return sizeHint(getType(index));
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }

    inline QString formatNumber(int num) const
    {
        QString text=QString::number(num);
        return num<10 ? "0"+text : text;
    }

    static void drawFadedLine(QPainter *p, const QRect &r, const QColor &col)
    {
        QPoint start(r.x(), r.y());
        QPoint end(r.x()+r.width()-1, r.y());
        QLinearGradient grad(start, end);
        QColor c(col);
        c.setAlphaF(0.45);
        QColor fade(c);
        const int fadeSize=64;
        double fadePos=1.0-((r.width()-fadeSize)/(r.width()*1.0));
        bool rtl=Qt::RightToLeft==QApplication::layoutDirection();

        fade.setAlphaF(0.0);
        grad.setColorAt(0, rtl ? fade : c);
        if(fadePos>=0 && fadePos<=1.0) {
            if (rtl) {
                grad.setColorAt(fadePos, c);
            } else {
                grad.setColorAt(1.0-fadePos, c);
            }
        }
        grad.setColorAt(1, rtl ? c : fade);
        p->save();
        p->setPen(QPen(QBrush(grad), 1));
        p->drawLine(start, end);
        p->restore();
    }

    static QPainterPath buildPath(const QRectF &r, double radius)
    {
        QPainterPath path;
        double diameter(radius*2);

        path.moveTo(r.x()+r.width(), r.y()+r.height()-radius);
        path.arcTo(r.x()+r.width()-diameter, r.y(), diameter, diameter, 0, 90);
        path.arcTo(r.x(), r.y(), diameter, diameter, 90, 90);
        path.arcTo(r.x(), r.y()+r.height()-diameter, diameter, diameter, 180, 90);
        path.arcTo(r.x()+r.width()-diameter, r.y()+r.height()-diameter, diameter, diameter, 270, 90);
        return path;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }

        Type type=getType(index);

        if (AlbumHeader==type) {
            QStyleOptionViewItem opt(option);
            painter->save();
            painter->setOpacity(0.75);
            QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);
            painter->restore();
            painter->save();
            painter->setClipRect(option.rect.adjusted(0, option.rect.height()/2, 0, 0), Qt::IntersectClip);
            QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);
            painter->restore();
        } else {
            QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);
        }
        Song song=index.data(PlayQueueView::Role_Song).value<Song>();
        QString title;
        QString track;
        QString duration=Song::formattedTime(song.time);
        bool stream=song.isStream();
        int state=index.data(PlayQueueView::Role_Status).toInt();
        QString trackTitle=!song.albumartist.isEmpty() && song.albumartist != song.artist
                    ? song.title + " - " + song.artist
                    : song.title;
        QFont f(QApplication::font());
        if (stream) {
            f.setItalic(true);
        }
        QFontMetrics fm(f);
        int textHeight=fm.height();

        switch(type) {
        case AlbumHeader: {
            QString album=song.album;

            if (stream && album.isEmpty() && song.albumArtist().isEmpty()) {
                title=song.file;
                if (song.title.isEmpty()) {
                    trackTitle=QString();
                }
                break;
            }
            if (song.year>0) {
                album+=QString(" (%1)").arg(song.year);
            }
            #ifdef ENABLE_KDE_SUPPORT
            title=i18nc("artist - album", "%1 - %2", song.albumArtist(), album);
            #else
            title=tr("%1 - %2").arg(song.albumArtist()).arg(album);
            #endif
            track=formatNumber(song.track)+QChar(' ')+trackTitle;
            break;
        }
        case AlbumTrack:
            track=formatNumber(song.track)+QChar(' ')+trackTitle;
            break;
        }

        painter->save();
        painter->setFont(f);
        QColor col(option.palette.color(option.state&QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
        QTextOption textOpt(Qt::AlignVCenter);
        QRect r(option.rect.adjusted(constBorder+4, constBorder, -(constBorder+4), -constBorder));
        bool rtl=Qt::RightToLeft==QApplication::layoutDirection();

        if (state) {
            QRectF border(option.rect.x()+1.5, option.rect.y()+1.5, option.rect.width()-3, option.rect.height()-3);
            if (!title.isEmpty()) {
                border.adjust(0, textHeight+constBorder, 0, 0);
            }
            QLinearGradient g(border.topLeft(), border.bottomLeft());
            QColor gradCol(QApplication::palette().color(QPalette::Highlight));
            gradCol.setAlphaF(option.state&QStyle::State_Selected ? 0.6 : 0.45);
            g.setColorAt(0, gradCol.dark(165));
            g.setColorAt(1, gradCol.light(165));
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->fillPath(buildPath(border, 3), g);
            painter->setPen(QPen(gradCol, 1));
            painter->drawPath(buildPath(border.adjusted(-1, -1, 1, 1), 3.5));
            painter->setPen(QPen(QApplication::palette().color(QPalette::HighlightedText), 1));
            painter->drawPath(buildPath(border, 3));
            painter->setRenderHint(QPainter::Antialiasing, false);
        }

        painter->setPen(col);
        bool showTrackDuration=true;

        if (!title.isEmpty()) {
            // Draw cover...
            QPixmap *cover=Covers::self()->get(song, constCoverSize);
            QPixmap pix=cover ? *cover : QIcon::fromTheme(stream ? "applications-internet" : "media-optical-audio").pixmap(constCoverSize, constCoverSize);

            if (rtl) {
                painter->drawPixmap(r.x()+r.width()-(pix.width()+constBorder), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
                r.adjust(0, 0, -(constCoverSize+constBorder), 0);
            } else {
                painter->drawPixmap(r.x()-2, r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
                r.adjust(constCoverSize+constBorder, 0, 0, 0);
            }
            QString totalDuration=Song::formattedTime(index.data(PlayQueueView::Role_AlbumDuration).toUInt());
            QRect duratioRect(r.x(), r.y(), r.width(), textHeight);
            int totalDurationWidth=fm.width(totalDuration)+8;
            QRect textRect(r.x(), r.y(), r.width()-totalDurationWidth, textHeight);
            QFont tf(f);
            tf.setBold(true);
            tf.setItalic(true);
            title = QFontMetrics(tf).elidedText(title, Qt::ElideRight, textRect.width(), QPalette::WindowText);
            painter->setFont(tf);
            painter->drawText(textRect, title, textOpt);
            painter->drawText(duratioRect, totalDuration, QTextOption(Qt::AlignVCenter|Qt::AlignRight));
            drawFadedLine(painter, r.adjusted(0, (r.height()/2)-1, 0, 0), col);
            r.adjust(0, textHeight+constBorder, 0, 0);
            r=QRect(r.x(), r.y()+r.height()-(textHeight+1), r.width(), textHeight);
            painter->setFont(f);
            if (rtl) {
                r.adjust(0, 0, (constCoverSize+constBorder), 0);
            }

            quint16 key=index.data(PlayQueueView::Role_Key).toUInt();
            if (!view->isExpanded(key)) {
                showTrackDuration=false;
                #ifdef ENABLE_KDE_SUPPORT
                track=i18np("1 Track", "%1 Tracks", index.data(PlayQueueView::Role_SongCount).toUInt());
                #else
                track=tr("%1 Track(s)").arg(index.data(PlayQueueView::Role_SongCount).toUInt());
                #endif
            }
        } else if (!rtl) {
            r.adjust(constCoverSize+constBorder, 0, 0, 0);
        }

        if (state) {
            int size=9;
            QRect ir(r.x()-(size+6), r.y()+(((r.height()-size)/2.0)+0.5), size, size);
            switch (state) {
            case PlayQueueView::State_Stopped:
                painter->fillRect(ir, Qt::white);
                painter->fillRect(ir.adjusted(1, 1, -1, -1), Qt::black);
                break;
            case PlayQueueView::State_Paused:
                painter->fillRect(ir, Qt::white);
                painter->fillRect(ir.x()+1, ir.y()+1, 3, size-2, Qt::black);
                painter->fillRect(ir.x()+size-4, ir.y()+1, 3, size-2, Qt::black);
                break;
            case PlayQueueView::State_Playing: {
                ir.adjust(2, 0, -2, 0);
                QPoint p1[5]={ QPoint(ir.x()-2, ir.y()-1), QPoint(ir.x(), ir.y()-1), QPoint(ir.x()+(size-4), ir.y()+4), QPoint(ir.x(), ir.y()+(ir.height()-1)), QPoint(ir.x()-2, ir.y()+(ir.height()-1)) };
                QPoint p2[5]={ QPoint(ir.x()-2, ir.y()-1), QPoint(ir.x(), ir.y()-1), QPoint(ir.x()+(size-4), ir.y()+4), QPoint(ir.x(), ir.y()+ir.height()), QPoint(ir.x()-2, ir.y()+ir.height()) };
                painter->save();
                painter->setBrush(Qt::white);
                painter->setPen(Qt::white);
                painter->drawPolygon(p1, 5);
                painter->setBrush(Qt::black);
                painter->drawPolygon(p2, 5);
                painter->restore();
                break;
            }
            }
            painter->setPen(col);
        }
        int durationWidth=showTrackDuration ? fm.width(duration)+8 : 0;
        QRect duratioRect(r.x(), r.y(), r.width(), textHeight);
        QRect textRect(r.x(), r.y(), r.width()-durationWidth, textHeight);
        track = fm.elidedText(track, Qt::ElideRight, textRect.width(), QPalette::WindowText);
        painter->drawText(textRect, track, textOpt);
        if (showTrackDuration) {
            painter->drawText(duratioRect, duration, QTextOption(Qt::AlignVCenter|Qt::AlignRight));
        }
        painter->restore();
    }

private:
    PlayQueueListView *view;
};

PlayQueueListView::PlayQueueListView(QWidget *parent)
    : ListView(parent)
    , autoCollapsingEnabled(false)
    , filterActive(false)
    , currentAlbum(Song::constNullKey)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    setDragDropOverwriteMode(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDropIndicatorShown(true);
    setUniformItemSizes(false);
    setSelectionBehavior(SelectRows);
    setItemDelegate(new PlayQueueDelegate(this));
}

PlayQueueListView::~PlayQueueListView()
{
}

void PlayQueueListView::setFilterActive(bool f)
{
    if (f==filterActive) {
        return;
    }
    filterActive=f;
    if (filterActive && model()) {
        quint32 count=model()->rowCount();
        for (quint32 i=0; i<count; ++i) {
            setRowHidden(i, false);
        }
    }
}

void PlayQueueListView::setAutoCollapsingEnabled(bool ac)
{
    if (ac==autoCollapsingEnabled) {
        return;
    }
    controlledSongIds.clear();
    controlledAlbums.clear();
    autoCollapsingEnabled=ac;
}

#define FIX_SCROLL_TO

void PlayQueueListView::updateRows(qint32 row, bool scroll)
{
    currentAlbum=Song::constNullKey;

    if (row<0) {
        row=0;
    }

    if (!model() || row>model()->rowCount()) {
        return;
    }

    if (filterActive && model() && MPDStatus::State_Playing==MPDStatus::self()->state()) {
        if (scroll) {
            scrollTo(model()->index(row, 0), QAbstractItemView::PositionAtCenter);
        }
        return;
    }

    qint32 count=model()->rowCount();
    quint16 lastKey=Song::constNullKey;

    #ifdef FIX_SCROLL_TO
    // scrollTo() is broken if some rows are hidden!
    quint32 titleHeight=scroll ? static_cast<PlayQueueDelegate *>(itemDelegate())->sizeHint(AlbumHeader).height() : 0;
    quint32 trackHeight=scroll ? static_cast<PlayQueueDelegate *>(itemDelegate())->sizeHint(AlbumTrack).height() : 0;
    quint32 totalHeight=0;
    quint32 heightToRow=0;
    bool haveHidden=false;
    #endif

    currentAlbum=model()->index(row, 0).data(PlayQueueView::Role_Key).toUInt();
    for (qint32 i=0; i<count; ++i) {
        quint16 key=model()->index(i, 0).data(PlayQueueView::Role_Key).toUInt();
        bool hide=key==lastKey && key!=currentAlbum && (( autoCollapsingEnabled && !controlledAlbums.contains(key)) ||
                                                        ( !autoCollapsingEnabled && controlledAlbums.contains(key)));
        setRowHidden(i, hide);

        #ifdef FIX_SCROLL_TO
        if (hide && i<row) {
            haveHidden=true;
        }

        if (scroll && !hide) {
            totalHeight+=(key==lastKey ? trackHeight : titleHeight);
            if (i<row) {
                heightToRow=totalHeight;
            }
        }
        #endif
        lastKey=key;
    }

    if (MPDStatus::State_Playing==MPDStatus::self()->state() && scroll) {
        #ifdef FIX_SCROLL_TO
        if (!haveHidden) {
            scrollTo(model()->index(row, 0), QAbstractItemView::PositionAtCenter);
        } else if(totalHeight) {
            unsigned int viewHeight=viewport()->size().height();
            unsigned int halfViewHeight=viewHeight/2;
            unsigned int max=verticalScrollBar()->maximum();
            unsigned int min=verticalScrollBar()->minimum();
            if (heightToRow<halfViewHeight) {
                verticalScrollBar()->setValue(min);
            } else if ((heightToRow)>(totalHeight-halfViewHeight)) {
                verticalScrollBar()->setValue(max);
            } else {
                unsigned int pos=min+(((max-min)*((1.0*(heightToRow-halfViewHeight))/(1.0*(totalHeight-viewHeight))))+0.5);
                verticalScrollBar()->setValue(pos<min ? min : (pos>max ? max : pos));
            }
        }
        #else
        scrollTo(model()->index(row, 0), QAbstractItemView::PositionAtCenter);
        #endif
    }
}

void PlayQueueListView::toggle(const QModelIndex &idx)
{
    quint16 indexKey=idx.data(PlayQueueView::Role_Key).toUInt();

    if (indexKey==currentAlbum) {
        return;
    }

    qint32 id=idx.data(PlayQueueView::Role_Id).toInt();
    bool toBeHidden=false;
    if (controlledAlbums.contains(indexKey)) {
        controlledAlbums.remove(indexKey);
        controlledSongIds.remove(id);
        toBeHidden=autoCollapsingEnabled;
    } else {
        controlledAlbums.insert(indexKey);
        controlledSongIds.insert(id);
        toBeHidden=!autoCollapsingEnabled;
    }

    if (model()) {
        quint32 count=model()->rowCount();
        for (quint32 i=idx.row()+1; i<count; ++i) {
            quint16 key=model()->index(i, 0).data(PlayQueueView::Role_Key).toUInt();
            if (indexKey==key) {
                setRowHidden(i, toBeHidden);
            } else {
                break;
            }
        }
    }
}

// Calculate list of selected indexes. If a collapsed album is selected, we also pretend all of its tracks
// are selected.
QModelIndexList PlayQueueListView::selectedIndexes() const
{
    QModelIndexList indexes = ListView::selectedIndexes();
    QModelIndexList allIndexes;
    quint32 rowCount=model()->rowCount();

    foreach (const QModelIndex &idx, indexes) {
        quint16 key=idx.data(PlayQueueView::Role_Key).toUInt();
        allIndexes.append(idx);
        if (!isExpanded(key)) {
            for (quint32 i=idx.row()+1; i<rowCount; ++i) {
                QModelIndex next=idx.sibling(i, 0);
                if (next.isValid()) {
                    quint16 nextKey=next.data(PlayQueueView::Role_Key).toUInt();
                    if (nextKey==key) {
                        allIndexes.append(next);
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
        }
    }

    return allIndexes;
}

// If we are dropping onto a collapsed album, and we are in the bottom 1/2 of the row - then
// we need to drop the item at the end of the album's tracks. So, we adjust the 'drop row'
// by the amount of tracks.
void PlayQueueListView::dropEvent(QDropEvent *event)
{
    quint32 dropRowAdjust=0;
    PlayQueueModel *m=qobject_cast<PlayQueueModel *>(model());
    if (m && viewport()->rect().contains(event->pos())) {
        QModelIndex idx=ListView::indexAt(event->pos());
        if (idx.isValid() && AlbumHeader==getType(idx)) {
            QRect rect(visualRect(idx));
            if (event->pos().y()>(rect.y()+(rect.height()/2))) {
                quint16 key=idx.data(PlayQueueView::Role_Key).toUInt();
                if (!isExpanded(key)) {
                    quint32 rowCount=model()->rowCount();
                    for (quint32 i=idx.row()+1; i<rowCount; ++i) {
                        QModelIndex next=idx.sibling(i, 0);
                        if (next.isValid()) {
                            quint16 nextKey=next.data(PlayQueueView::Role_Key).toUInt();
                            if (nextKey==key) {
                                dropRowAdjust++;
                            } else {
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                    m->setDropAdjust(dropRowAdjust);
                }
            }
        }
    }

    ListView::dropEvent(event);
    m->setDropAdjust(0);
}

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
    listView=new PlayQueueListView(this);
    treeView=new PlayQueueTreeView(this);
    addWidget(listView);
    addWidget(treeView);
    setCurrentWidget(treeView);
    connect(listView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
    connect(treeView, SIGNAL(itemsSelected(bool)), SIGNAL(itemsSelected(bool)));
    connect(listView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
    connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), SIGNAL(doubleClicked(const QModelIndex &)));
    connect(listView, SIGNAL(clicked(const QModelIndex &)), SLOT(itemClicked(const QModelIndex &)));
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
    bool grouped=listView==currentWidget();

    if (g!=grouped) {
        if (grouped) {
            treeView->setModel(listView->model());
            treeView->initHeader();
            listView->setModel(0);
        } else {
            treeView->saveHeader();
            listView->setModel(treeView->model());
            treeView->setModel(0);
        }
        grouped=g;
        setCurrentWidget(grouped ? static_cast<QWidget *>(listView) : static_cast<QWidget *>(treeView));
    }
}

void PlayQueueView::setAutoCollapsingEnabled(bool ac)
{
    listView->setAutoCollapsingEnabled(ac);
}

bool PlayQueueView::isAutoCollapsingEnabled() const
{
    return listView->isAutoCollapsingEnabled();
}

QSet<qint32> PlayQueueView::getControlledSongIds() const
{
    return currentWidget()==listView ? listView->getControlledSongIds() : QSet<qint32>();
}

void PlayQueueView::setControlled(const QSet<quint16> &keys)
{
    listView->setControlled(keys);
}

void PlayQueueView::setFilterActive(bool f)
{
    listView->setFilterActive(f);
}

void PlayQueueView::updateRows(qint32 row, bool scroll)
{
    if (currentWidget()==listView) {
        listView->updateRows(row, scroll);
    }
}

void PlayQueueView::scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint)
{
    if (currentWidget()==listView && !listView->isFilterActive()) {
        return;
    }
    if (MPDStatus::State_Playing==MPDStatus::self()->state()) {
//         listView->scrollTo(index, hint);
        treeView->scrollTo(index, hint);
    }
}

void PlayQueueView::setModel(QAbstractItemModel *m)
{
    if (currentWidget()==listView) {
        listView->setModel(m);
    } else {
        treeView->setModel(m);
    }
}

void PlayQueueView::addAction(QAction *a)
{
    listView->addAction(a);
    treeView->addAction(a);
}

void PlayQueueView::setFocus()
{
    currentWidget()->setFocus();
}

QAbstractItemModel * PlayQueueView::model()
{
    return currentWidget()==listView ? listView->model() : treeView->model();
}

void PlayQueueView::setContextMenuPolicy(Qt::ContextMenuPolicy policy)
{
    listView->setContextMenuPolicy(policy);
    treeView->setContextMenuPolicy(policy);
}

bool PlayQueueView::haveUnSelectedItems()
{
    return currentWidget()==listView ? listView->haveUnSelectedItems() : treeView->haveUnSelectedItems();
}

QItemSelectionModel * PlayQueueView::selectionModel() const
{
    return currentWidget()==listView ? listView->selectionModel() : treeView->selectionModel();
}

void PlayQueueView::setCurrentIndex(const QModelIndex &index)
{
    if (currentWidget()==listView) {
        listView->setCurrentIndex(index);
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
    return listView;
}

QModelIndexList PlayQueueView::selectedIndexes() const
{
    return listView==currentWidget() ? listView->selectedIndexes() : selectionModel()->selectedRows();
}

void PlayQueueView::itemClicked(const QModelIndex &idx)
{
    if (listView==currentWidget() && AlbumHeader==getType(idx)) {
        QRect indexRect(listView->visualRect(idx));
        QRect icon(indexRect.x()+constBorder+4, indexRect.y()+constBorder+((indexRect.height()-constCoverSize)/2),
                   constCoverSize, constCoverSize);
        QRect header(indexRect);
        header.setHeight(header.height()/2);
        header.moveTo(listView->viewport()->mapToGlobal(QPoint(header.x(), header.y())));
        icon.moveTo(listView->viewport()->mapToGlobal(QPoint(icon.x(), icon.y())));
        if (icon.contains(QCursor::pos())) {
            listView->toggle(idx);
        } else if (header.contains(QCursor::pos())) {
            QModelIndexList list;
            unsigned int key=idx.data(PlayQueueView::Role_Key).toUInt();
            QModelIndex i=idx.sibling(idx.row()+1, 0);
            QModelIndexList sel=listView->selectedIndexes();
            QItemSelectionModel *selModel=listView->selectionModel();
            QModelIndexList unsel;

            while (i.isValid() && i.data(PlayQueueView::Role_Key).toUInt()==key) {
                if (!sel.contains(i)) {
                    unsel.append(i);
                }
                list.append(i);
                i=i.sibling(i.row()+1, 0);
            }
            if (list.count()) {
                if (unsel.isEmpty()) { // TODO: This is not working!!!
                    foreach(const QModelIndex &i, list) {
                        selModel->select(i, QItemSelectionModel::Deselect|QItemSelectionModel::Rows);
                    }
                } else {
                    foreach(const QModelIndex &i, unsel) {
                        selModel->select(i, QItemSelectionModel::Select|QItemSelectionModel::Rows);
                    }
                }
            }
        }
    }
}
