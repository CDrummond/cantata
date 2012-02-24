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
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QApplication>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>
#include <QtGui/QHeaderView>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

static const int constCoverSize=32;
static const int constIconSize=16;
static const int constBorder=1;

// static void drawBgnd(QPainter *painter, const QRect &rx)
// {
//     QRectF r(rx.x()-0.5, rx.y()-0.5, rx.width()+1, rx.height()+1);
//     QPainterPath p;//(buildPath(r, r.width()/2.0));
//     QColor c(Qt::white);
//
//     p.addEllipse(r);
//     painter->setRenderHint(QPainter::Antialiasing, true);
//     c.setAlphaF(0.75);
//     painter->fillPath(p, c);
// //     c.setAlphaF(0.95);
// //     painter->setPen(c);
// //     painter->drawPath(p);
//     painter->setRenderHint(QPainter::Antialiasing, false);
// }

enum Type {
    AlbumHeader,
    AlbumTrack
//     SingleTrack
};

static Type getType(const QModelIndex &index)
{
    QModelIndex prev=index.row()>0 ? index.sibling(index.row()-1, 0) : QModelIndex();
    unsigned long thisKey=index.data(PlayQueueView::Role_Key).toUInt();
    unsigned long prevKey=prev.isValid() ? prev.data(PlayQueueView::Role_Key).toUInt() : 0xFFFFFFFF;

    if (thisKey==prevKey) {
        // Album Track
        return AlbumTrack;
    }
//     QModelIndex next=index.sibling(index.row()+1, 0);
//     unsigned long nextKey=next.isValid() ? next.data(PlayQueueView::Role_Key).toUInt() : 0xFFFFFFFF;
//     if (thisKey==nextKey) {
//         Album header...
    return AlbumHeader;
//     }
//     Single track
//     return SingleTrack;
}

class PlayQueueDelegate : public QStyledItemDelegate
{
public:
    PlayQueueDelegate(QObject *p)
        : QStyledItemDelegate(p)
    {
    }

    virtual ~PlayQueueDelegate()
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (0==index.column()) {
            int textHeight = QApplication::fontMetrics().height();

            if (AlbumHeader==getType(index)) {
                return QSize(64, qMax(constCoverSize, (qMax(constIconSize, textHeight)*2))+(3*constBorder));
            }
            return QSize(64, qMax(constIconSize, textHeight)+(2*constBorder));
        }
        return QStyledItemDelegate::sizeHint(option, index);;
    }

    inline QString formatNumber(int num) const
    {
        QString text=QString::number(num);
        return num<10 ? "0"+text : text;
    }

    void drawFadedLine(QPainter *p, const QRect &r, const QColor &col) const
    {
        QPoint start(r.x(), r.y());
        QPoint  end(r.x()+r.width()-1, r.y());
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

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }

        Type type=getType(index);
        QFont f(QApplication::font());
        QFontMetrics fm(f);
        int textHeight=fm.height();

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
        bool isCurrent=index.data(PlayQueueView::Role_Status).toBool();
        int indent=0;
        QString trackTitle=!song.albumartist.isEmpty() && song.albumartist != song.artist
                    ? song.title + " - " + song.artist
                    : song.title;

        switch(type) {
        case AlbumHeader: {
            QString album=song.album;

            if (song.year>0) {
                album+=QString(" (%1)").arg(song.year);
            }
            #ifdef ENABLE_KDE_SUPPORT
            title=i18nc("artist - album", "%1 - %2", song.albumArtist(), album);
            #else
            title=tr("%1 - %2").arg(song.albumartist()).arg(album);
            #endif
            track=formatNumber(song.track)+QChar(' ')+trackTitle;
//             indent=8;
            break;
        }
        case AlbumTrack:
            track=formatNumber(song.track)+QChar(' ')+trackTitle;
//             indent=8;
            break;
//         case SingleTrack:
//             #ifdef ENABLE_KDE_SUPPORT
//             track=i18nc("TrackNum TrackTitle, from Album by Artist", "%1 %2, from %3 by %4",
//                                 formatNumber(song.track), trackTitle, song.album, song.artist);
//             #else
//             track=tr("TrackNum TrackTitle, from Album by Artist", "%1 %2, from %3 by %4")
//                      .arg(formatNumber(song.track).arg(index.data(trackTitle).arg(index.data(song.album).arg(song.artist);
//             #endif
//             break;
        }

        painter->save();
//         if (AlbumTrack==type) {
//             f.setItalic(true);
//         }
        painter->setFont(f);
        QColor col(QApplication::palette().color(option.state&QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
        painter->setPen(col);
        QTextOption textOpt(Qt::AlignVCenter);
        QRect r(option.rect.adjusted(constBorder+4, constBorder, -(constBorder+4), -constBorder));
//         QRect sr(r);
        bool rtl=Qt::RightToLeft==QApplication::layoutDirection();

        if (!title.isEmpty()) {
            // Draw cover...
            QPixmap *cover=Covers::self()->get(song, constCoverSize);
            QPixmap pix=cover ? *cover : QIcon::fromTheme("media-optical-audio").pixmap(constCoverSize, constCoverSize);

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
            title = fm.elidedText(title, Qt::ElideRight, textRect.width(), QPalette::WindowText);
            QFont tf(f);
            tf.setBold(true);
            tf.setItalic(true);
            painter->setFont(tf);
            painter->drawText(textRect, title, textOpt);
            painter->drawText(duratioRect, totalDuration, QTextOption(Qt::AlignVCenter|Qt::AlignRight));
            drawFadedLine(painter, r.adjusted(0, r.height()/2, 0, 0), col);
            r.adjust(0, textHeight+constBorder, 0, 0);
            r=QRect(r.x(), r.y()+r.height()-(textHeight+1), r.width(), textHeight);
//             sr.adjust(0, textHeight+constBorder, 0, 0);
//             f.setItalic(true);
            painter->setFont(f);
            if (rtl) {
                r.adjust(0, 0, (constCoverSize+constBorder), 0);
            }
        } else if (!rtl) {
            r.adjust(constCoverSize+constBorder, 0, 0, 0);
        }

        if (isCurrent) {
            f.setBold(true);
            painter->setFont(f);
//             QPixmap pix=statusIcon.value<QIcon>().pixmap(constIconSize, constIconSize);
//             int adjust=(constCoverSize-constIconSize)-constBorder;
//             QRect pixRect=rtl
//                     ? QRect(sr.x()+sr.width()-(pix.width()+constBorder+adjust), sr.y()+((sr.height()-pix.height())/2), pix.width(), pix.height())
//                     : QRect(sr.x()+adjust, sr.y()+((sr.height()-pix.height())/2), pix.width(), pix.height());
//
//             drawBgnd(painter, pixRect.adjusted(-1, -1, 1, 1));
//             painter->drawPixmap(pixRect, pix);
        }
        int durationWidth=fm.width(duration)+8;
        QRect duratioRect(r.x()+indent, r.y(), r.width()-indent, textHeight);
        QRect textRect(r.x()+indent, r.y(), r.width()-(indent+durationWidth), textHeight);
        track = fm.elidedText(track, Qt::ElideRight, textRect.width(), QPalette::WindowText);
        painter->drawText(textRect, track, textOpt);
        painter->drawText(duratioRect, duration, QTextOption(Qt::AlignVCenter|Qt::AlignRight));
        painter->restore();
    }
};

class PlayQueueListView : public ListView
{
public:

    PlayQueueListView(QWidget *parent=0);
    virtual ~PlayQueueListView();
};

PlayQueueListView::PlayQueueListView(QWidget *parent)
        : ListView(parent)
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

void PlayQueueView::scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint)
{
    if (currentWidget()==listView) {
        listView->scrollTo(index, hint);
    } else {
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

void PlayQueueView::itemClicked(const QModelIndex &idx)
{
    if (listView==currentWidget() && AlbumHeader==getType(idx)) {
        QRect rect(listView->visualRect(idx));
        rect.setHeight(rect.height()/2);
        rect.moveTo(listView->viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
        if (rect.contains(QCursor::pos())) {
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
