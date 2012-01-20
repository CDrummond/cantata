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
#include "mpdparseutils.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

MusicLibraryItemArtist::MusicLibraryItemArtist(const QString &data, MusicLibraryItem *parent)
    : MusicLibraryItem(data, MusicLibraryItem::Type_Artist, parent)
    , m_various(false)
{
    if (m_itemData.startsWith(QLatin1String("The "))) {
        m_nonTheArtist=m_itemData.mid(4);
    } else if (Song::isVariousArtists(m_itemData)) {
        m_various=true;
    }
}

MusicLibraryItemAlbum * MusicLibraryItemArtist::album(const Song &s, bool create)
{
    QHash<QString, int>::ConstIterator it=m_indexes.find(s.album);

    if (m_indexes.end()==it) {
        return create ? createAlbum(s) : 0;
    }
    return static_cast<MusicLibraryItemAlbum *>(m_childItems.at(*it));
}

MusicLibraryItemAlbum * MusicLibraryItemArtist::createAlbum(const Song &s)
{
    MusicLibraryItemAlbum *item=new MusicLibraryItemAlbum(s.album, s.year, this);
    m_indexes.insert(s.album, m_childItems.count());
    m_childItems.append(item);
    return item;
}

const QString & MusicLibraryItemArtist::baseArtist() const
{
    return m_nonTheArtist.isEmpty() ? m_itemData : m_nonTheArtist;
}

bool MusicLibraryItemArtist::allSingleTrack() const
{
    foreach (MusicLibraryItem *a, m_childItems) {
        if (a->childCount()>1) {
            return false;
        }
    }
    return true;
}

void MusicLibraryItemArtist::addToSingleTracks(MusicLibraryItemArtist *other)
{
    Song s;
    #ifdef ENABLE_KDE_SUPPORT
    s.album=i18n("Single Tracks");
    #else
    s.album=QObject::tr("Single Tracks");
    #endif
    MusicLibraryItemAlbum *single=album(s);
    foreach (MusicLibraryItem *album, other->children()) {
        single->addTracks(static_cast<MusicLibraryItemAlbum *>(album));
    }
}

bool MusicLibraryItemArtist::isFromSingleTracks(const Song &s) const
{
    #ifdef ENABLE_KDE_SUPPORT
    QHash<QString, int>::ConstIterator it=m_indexes.find(i18n("Single Tracks"));
    #else
    QHash<QString, int>::ConstIterator it=m_indexes.find(QObject::tr("Single Tracks"));
    #endif

    if (m_indexes.end()!=it) {
        return static_cast<MusicLibraryItemAlbum *>(m_childItems.at(*it))->isSingleTrackFile(s);
    }
    return false;
}

void MusicLibraryItemArtist::remove(MusicLibraryItemAlbum *album)
{
    int index=m_childItems.indexOf(album);

    if (index<0 || index>=m_childItems.count()) {
        return;
    }

    QHash<QString, int>::Iterator it=m_indexes.begin();
    QHash<QString, int>::Iterator end=m_indexes.end();

    for (; it!=end; ++it) {
        if ((*it)>index) {
            (*it)--;
        }
    }
    m_indexes.remove(album->data());
    delete m_childItems.takeAt(index);
}
