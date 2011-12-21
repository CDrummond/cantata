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

static MusicLibraryItemAlbum::CoverSize coverSize=MusicLibraryItemAlbum::CoverNone;

static QPixmap *theDefaultIcon=0;

int MusicLibraryItemAlbum::iconSize(MusicLibraryItemAlbum::CoverSize sz)
{
    switch (sz) {
    default:
    case MusicLibraryItemAlbum::CoverNone:   return 0;
    case MusicLibraryItemAlbum::CoverSmall:  return 22;
    case MusicLibraryItemAlbum::CoverMedium: return 32;
    case MusicLibraryItemAlbum::CoverLarge:  return 48;
    }
}

int MusicLibraryItemAlbum::iconSize()
{
    return MusicLibraryItemAlbum::iconSize(coverSize);
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
    , m_coverIsDefault(false)
    , m_cover(0)
    , m_parentItem(static_cast<MusicLibraryItemArtist *>(parent))
{
}

MusicLibraryItemAlbum::~MusicLibraryItemAlbum()
{
    qDeleteAll(m_childItems);
    delete m_cover;
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
        m_cover = new QPixmap(QPixmap::fromImage(img).scaled(QSize(iconSize(), iconSize()), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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

    if (!m_cover) {
        int iSize=iconSize();

        if (!theDefaultIcon) {
            int cSize=iSize;
            if (0==cSize) {
                cSize=22;
            }
            theDefaultIcon = new QPixmap(QIcon::fromTheme("media-optical-audio").pixmap(cSize, cSize)
                                        .scaled(QSize(cSize, cSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        m_coverIsDefault = true;
        if (parent() && iSize) {
            Song song;
            song.albumartist=parent()->data(0).toString();
            song.album=m_itemData;
            song.file=m_dir;
            Covers::self()->get(song);
        }
        return *theDefaultIcon;
    }

    return *m_cover;
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
