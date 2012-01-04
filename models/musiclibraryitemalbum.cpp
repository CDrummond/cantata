/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
static bool useDate=false;

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

void MusicLibraryItemAlbum::setShowDate(bool sd)
{
    useDate=sd;
}

bool MusicLibraryItemAlbum::showDate()
{
    return useDate;
}

MusicLibraryItemAlbum::MusicLibraryItemAlbum(const QString &data, const QString &dir, quint32 year, MusicLibraryItem *parent)
    : MusicLibraryItem(data, MusicLibraryItem::Type_Album, parent)
    , m_dir(dir)
    , m_year(year)
    , m_coverIsDefault(false)
    , m_cover(0)
    , m_singleTracks(false)
{
}

MusicLibraryItemAlbum::~MusicLibraryItemAlbum()
{
    delete m_cover;
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
            song.albumartist=parent()->data();
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

void MusicLibraryItemAlbum::addTracks(MusicLibraryItemAlbum *other)
{
    m_singleTracks=true;
    foreach (MusicLibraryItem *track, other->m_childItems) {
        track->setParent(this);
    }
}
