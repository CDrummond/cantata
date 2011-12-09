/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "song.h"
#include "covers.h"
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>

static MusicLibraryItemAlbum::CoverSize coverSize=MusicLibraryItemAlbum::CoverNone;

static QPixmap *theDefaultIcon=0;

static int coverPixels()
{
    switch (coverSize) {
    default:
    case MusicLibraryItemAlbum::CoverNone:   return 0;
    case MusicLibraryItemAlbum::CoverSmall:  return 22;
    case MusicLibraryItemAlbum::CoverMedium: return 30;
    case MusicLibraryItemAlbum::CoverLarge:  return 48;
    }
}

static int stdIconSize()
{
    switch (coverSize) {
    default:
    case MusicLibraryItemAlbum::CoverNone:   return 0;
    case MusicLibraryItemAlbum::CoverSmall:  return 22;
    case MusicLibraryItemAlbum::CoverMedium: return 32;
    case MusicLibraryItemAlbum::CoverLarge:  return 48;
    }
}

MusicLibraryItemAlbum::CoverSize MusicLibraryItemAlbum::currentCoverSize()
{
    return coverSize;
}

void MusicLibraryItemAlbum::setCoverSize(MusicLibraryItemAlbum::CoverSize size)
{
    if (size!=coverSize) {
        if (theDefaultIcon) {
            delete theDefaultIcon;
            theDefaultIcon=0;
        }
        coverSize=size;
    }
}

MusicLibraryItemAlbum::MusicLibraryItemAlbum(const QString &data, const QString &dir, MusicLibraryItem *parent)
    : MusicLibraryItem(data, MusicLibraryItem::Type_Album)
    , m_dir(dir)
    , m_parentItem(static_cast<MusicLibraryItemArtist *>(parent))
{
    m_coverIsDefault=false;
}

MusicLibraryItemAlbum::~MusicLibraryItemAlbum()
{
    qDeleteAll(m_childItems);
}

void MusicLibraryItemAlbum::appendSong(MusicLibraryItemSong * const song)
{
    m_childItems.append(song);
}

MusicLibraryItem * MusicLibraryItemAlbum::child(int row) const
{
    return m_childItems.value(row);
}

int MusicLibraryItemAlbum::childCount() const
{
    return m_childItems.count();
}

MusicLibraryItem * MusicLibraryItemAlbum::parent() const
{
    return m_parentItem;
}

int MusicLibraryItemAlbum::row() const
{
    return m_parentItem->m_childItems.indexOf(const_cast<MusicLibraryItemAlbum*>(this));
}

bool MusicLibraryItemAlbum::setCover(const QImage &img)
{
    if (m_coverIsDefault) {
        QPixmap cover=QPixmap::fromImage(img).scaled(QSize(coverPixels()-2, coverPixels()-2), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_cover = QPixmap(coverPixels(), coverPixels());
        m_cover.fill(Qt::transparent);
        QPainter p(&m_cover);
        p.drawPixmap(1, 1, cover);
        p.end();
        m_coverIsDefault=false;
        return true;
    }

    return false;
}

const QPixmap & MusicLibraryItemAlbum::cover()
{
    if (m_coverIsDefault) {
        return *theDefaultIcon;
    }

    if (m_cover.isNull()) {
        if (!theDefaultIcon) {
            theDefaultIcon = new QPixmap(QIcon::fromTheme("media-optical-audio").pixmap(stdIconSize(), stdIconSize())
                                        .scaled(QSize(coverPixels(), coverPixels()), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        m_coverIsDefault = true;
        if (parent()) {
            Song song;
            song.albumartist=parent()->data(0).toString();
            song.album=m_itemData;
            song.file=m_dir;
            Covers::self()->get(song);
        }
        return *theDefaultIcon;
    }

    return m_cover;
}

QStringList MusicLibraryItemAlbum::sortedTracks()
{
    QMap<int, QString> tracks;
    quint32 trackWithoutNumberIndex=0xFFFF; // *Very* unlikely to have tracks numbered greater than 65535!!!

    for (int i = 0; i < childCount(); i++) {
        MusicLibraryItemSong *trackItem = static_cast<MusicLibraryItemSong*>(child(i));
        tracks.insert(0==trackItem->track() || trackItem->track()>0xFFFF ? trackWithoutNumberIndex++ : trackItem->track(), trackItem->file());
    }

    return tracks.values();
}
