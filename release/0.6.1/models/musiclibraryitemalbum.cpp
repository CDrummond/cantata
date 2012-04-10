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

#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "song.h"
#include "covers.h"
#include "config.h"
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

MusicLibraryItemAlbum::MusicLibraryItemAlbum(const QString &data, quint32 year, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(data, parent)
    , m_year(year)
    , m_coverIsDefault(false)
    , m_cover(0)
    , m_type(Song::Standard)
{
}

MusicLibraryItemAlbum::~MusicLibraryItemAlbum()
{
    delete m_cover;
}

bool MusicLibraryItemAlbum::setCover(const QImage &img) const
{
    if (m_coverIsDefault && !img.isNull()) {
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
            theDefaultIcon = new QPixmap(QIcon::fromTheme(DEFAULT_ALBUM_ICON).pixmap(cSize, cSize)
                                        .scaled(QSize(cSize, cSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        m_coverIsDefault = true;
        if (parentItem() && iSize && childCount()) {
            Song song;
            song.albumartist=parentItem()->data();
            song.album=m_itemData;
            song.file=static_cast<MusicLibraryItemSong*>(childItem(0))->file();
            Covers::Image img=Covers::self()->get(song, Song::SingleTracks==m_type);
            if (setCover(img.img)) {
                return *m_cover;
            }
        }
        return *theDefaultIcon;
    }

    return *m_cover;
}

QStringList MusicLibraryItemAlbum::sortedTracks() const
{
    QMap<int, QString> tracks;
    quint32 trackWithoutNumberIndex=0xFFFF; // *Very* unlikely to have tracks numbered greater than 65535!!!

    for (int i = 0; i < childCount(); i++) {
        MusicLibraryItemSong *trackItem = static_cast<MusicLibraryItemSong*>(childItem(i));
        tracks.insert(0==trackItem->track() || trackItem->track()>0xFFFF ? trackWithoutNumberIndex++ : trackItem->track(), trackItem->file());
    }

    return tracks.values();
}

quint32 MusicLibraryItemAlbum::totalTime()
{
    if (0==m_totalTime) {
        foreach (MusicLibraryItem *i, m_childItems) {
            m_totalTime+=static_cast<MusicLibraryItemSong *>(i)->time();
        }
    }
    return m_totalTime;
}

void MusicLibraryItemAlbum::addTracks(MusicLibraryItemAlbum *other)
{
    m_type=Song::SingleTracks;
    foreach (MusicLibraryItem *track, other->m_childItems) {
        static_cast<MusicLibraryItemSong*>(track)->song().type=Song::SingleTracks;
        m_singleTrackFiles.insert(static_cast<MusicLibraryItemSong*>(track)->song().file);
        track->setParent(this);
    }
}

void MusicLibraryItemAlbum::setIsSingleTracks()
{
    foreach (MusicLibraryItem *track, m_childItems) {
        static_cast<MusicLibraryItemSong*>(track)->song().type=Song::SingleTracks;
        m_singleTrackFiles.insert(static_cast<MusicLibraryItemSong*>(track)->song().file);
    }
    m_type=Song::SingleTracks;
}

void MusicLibraryItemAlbum::setIsMultipleArtists()
{
    foreach (MusicLibraryItem *track, m_childItems) {
        static_cast<MusicLibraryItemSong*>(track)->song().type=Song::MultipleArtists;
    }
    m_type=Song::MultipleArtists;
}

void MusicLibraryItemAlbum::append(MusicLibraryItem *i)
{
    MusicLibraryItemContainer::append(i);
    if (Song::SingleTracks==m_type) {
        static_cast<MusicLibraryItemSong*>(i)->song().type=Song::SingleTracks;
        m_singleTrackFiles.insert(static_cast<MusicLibraryItemSong*>(i)->song().file);
    } else if (Song::MultipleArtists==m_type) {
        static_cast<MusicLibraryItemSong*>(i)->song().type=Song::MultipleArtists;
    }
    m_totalTime=0;
}

void MusicLibraryItemAlbum::remove(int row)
{
    delete m_childItems.takeAt(row);
    m_totalTime=0;
}

bool MusicLibraryItemAlbum::detectIfIsMultipleArtists()
{
    if (m_childItems.count()<2) {
        return false;
    }

    if (Song::Standard==m_type) {
        QString a;
        foreach (MusicLibraryItem *track, m_childItems) {
            if (a.isEmpty()) {
                a=static_cast<MusicLibraryItemSong*>(track)->song().artist;
            } else if (static_cast<MusicLibraryItemSong*>(track)->song().artist!=a) {
                m_type=Song::MultipleArtists;
                break;
            }
        }
    }
    return Song::MultipleArtists==m_type;
}
