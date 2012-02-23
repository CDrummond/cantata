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
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QApplication>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QLinearGradient>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
static const int constIconSize=16;
static const int constBorder=1;

class PlayQueueDelegate : public QStyledItemDelegate
{
public:
    enum Type {
        AlbumHeader,
        AlbumTrack
//         SingleTrack
    };

    PlayQueueDelegate(QObject *p)
        : QStyledItemDelegate(p)
    {
    }

    virtual ~PlayQueueDelegate()
    {
    }

    Type getType(const QModelIndex &index) const
    {
        QModelIndex prev=index.row()>0 ? index.sibling(index.row()-1, 0) : QModelIndex();
        unsigned long thisKey=index.data(PlayQueueView::Role_Key).toUInt();
        unsigned long prevKey=prev.isValid() ? prev.data(PlayQueueView::Role_Key).toUInt() : 0xFFFFFFFF;

        if (thisKey==prevKey) {
            // Album Track
            return AlbumTrack;
        }
//         QModelIndex next=index.sibling(index.row()+1, 0);
//         unsigned long nextKey=next.isValid() ? next.data(PlayQueueView::Role_Key).toUInt() : 0xFFFFFFFF;
//         if (thisKey==nextKey) {
            // Album header...
            return AlbumHeader;
//         }
        // Single track
//         return SingleTrack;
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QSize sz=QStyledItemDelegate::sizeHint(option, index);

        if (0==index.column()) {
            int textHeight = QApplication::fontMetrics().height();

            if (AlbumHeader==getType(index)) {
                return QSize(sz.width(), (textHeight*2)+(3*constBorder));
            }
            return QSize(sz.width(), qMax(constIconSize, textHeight)+(2*constBorder));
        }
        return sz;
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
            painter->setClipRect(option.rect.adjusted(0, textHeight+constBorder, 0, 0), Qt::IntersectClip);
            QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);
            painter->restore();
        } else {
            QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);
        }
        QString title;
        QString track;
        int indent=0;

        switch(type) {
        case AlbumHeader:
            #ifdef ENABLE_KDE_SUPPORT
            title=i18nc("Album by AlbumArtist", "%1 by %2", index.data(PlayQueueView::Role_Album).toString(),
                        index.data(PlayQueueView::Role_AlbumArtist).toString());
            track=i18nc("TrackNum TrackTitle", "%1 %2", formatNumber(index.data(PlayQueueView::Role_Track).toInt()),
                        index.data(PlayQueueView::Role_Title).toString());
            #else
            title=tr("Album by AlbumArtist", "%1 by %2").arg(index.data(PlayQueueView::Role_Album).toString())
                     .arg(index.data(PlayQueueView::Role_AlbumArtist).toString());
            track=tr("TrackNum TrackTitle", "%1 %2").arg(formatNumber(index.data(PlayQueueView::Role_Track).toInt()))
                     .arg(index.data(PlayQueueView::Role_Title).toString());
            #endif
            indent=16;
            break;
        case AlbumTrack:
            #ifdef ENABLE_KDE_SUPPORT
            track=i18nc("TrackNum TrackTitle", "%1 %2", formatNumber(index.data(PlayQueueView::Role_Track).toInt()),
                        index.data(PlayQueueView::Role_Title).toString());
            #else
            track=tr("TrackNum TrackTitle", "%1 %2").arg(formatNumber(index.data(PlayQueueView::Role_Track).toInt()))
                    .arg(index.data(PlayQueueView::Role_Title).toString());
            #endif
            indent=16;
            break;
//         case SingleTrack:
//             #ifdef ENABLE_KDE_SUPPORT
//             track=i18nc("TrackNum TrackTitle, from Album by Artist", "%1 %2, from %3 by %4",
//                                 formatNumber(index.data(PlayQueueView::Role_Track).toInt()),
//                                 index.data(PlayQueueView::Role_Title).toString(),
//                                 index.data(PlayQueueView::Role_Album).toString(),
//                                 index.data(PlayQueueView::Role_Artist).toString());
//             #else
//             track=tr("TrackNum TrackTitle, from Album by Artist", "%1 %2, from %3 by %4")
//                      .arg(formatNumber(index.data(PlayQueueView::Role_Track).toInt()))
//                      .arg(index.data(PlayQueueView::Role_Title).toString())
//                      .arg(index.data(PlayQueueView::Role_Album).toString())
//                      .arg(index.data(PlayQueueView::Role_Artist).toString());
//             #endif
//             break;
        }

        painter->save();
        if (AlbumTrack==type) {
            f.setItalic(true);
        }
        painter->setFont(f);
        QColor col(QApplication::palette().color(option.state&QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
        painter->setPen(col);
        QTextOption textOpt(Qt::AlignVCenter);
        QRect r(option.rect.adjusted(constBorder+4, constBorder, -(constBorder+4), -constBorder));

        if (!title.isEmpty()) {
            QRect textRect(r.x(), r.y(), r.width(), textHeight);
            title = fm.elidedText(title, Qt::ElideRight, textRect.width(), QPalette::WindowText);
            painter->drawText(textRect, title, textOpt);
            r.adjust(0, textHeight+constBorder, 0, -(textHeight+constBorder));
            drawFadedLine(painter, r, col);
            f.setItalic(true);
            painter->setFont(f);
        }
        QRect textRect(r.x()+indent, r.y(), r.width()-indent, textHeight);
        track = fm.elidedText(track, Qt::ElideRight, textRect.width(), QPalette::WindowText);
        painter->drawText(textRect, track, textOpt);
        painter->restore();
    }
};

PlayQueueView::PlayQueueView(QWidget *parent)
        : TreeView(parent)
        , grouped(true)
        , prev(0)
        , custom(0)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropOverwriteMode(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setIndentation(0);
    setItemsExpandable(false);
    setExpandsOnDoubleClick(false);
    setGrouped(false);
}

PlayQueueView::~PlayQueueView()
{
}

void PlayQueueView::setGrouped(bool g)
{
    if (g==grouped) {
        return;
    }
    grouped=g;

    if (grouped) {
        if (!custom) {
            custom=new PlayQueueDelegate(this);
        }
        if (!prev) {
            prev=itemDelegate();
        }
        setItemDelegate(custom);
    } else if (prev) {
        setItemDelegate(prev);
    }

    setHeaderHidden(grouped);
    setUniformRowHeights(!grouped);
}

