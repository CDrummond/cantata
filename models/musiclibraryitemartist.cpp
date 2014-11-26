/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "musiclibrarymodel.h"
#include "mpd/mpdparseutils.h"
#include "support/localize.h"
#include "gui/covers.h"
#include "widgets/icons.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devices/device.h"
#include "support/utils.h"
#endif
#ifdef ENABLE_ONLINE_SERVICES
#include "online/onlineservice.h"
#include "onlineservicesmodel.h"
#endif

bool MusicLibraryItemArtist::lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b)
{
    const MusicLibraryItemArtist *aa=static_cast<const MusicLibraryItemArtist *>(a);
    const MusicLibraryItemArtist *ab=static_cast<const MusicLibraryItemArtist *>(b);

//    if (aa->isVarious() != ab->isVarious()) {
//        return aa->isVarious() > ab->isVarious();
//    }
    return aa->sortString().localeAwareCompare(ab->sortString())<0;
}

#ifdef ENABLE_UBUNTU
static const QString constDefaultCover=QLatin1String("qrc:/artist.svg");
static const QString constDefaultVariousCover=QLatin1String("qrc:/variousartists.svg");
#endif

static const QLatin1String constThe("The ");

MusicLibraryItemArtist::MusicLibraryItemArtist(const QString &data, const QString &artistName, const QString &artistSort, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(data, parent)
    #ifdef ENABLE_UBUNTU
    , m_coverRequested(false)
    #endif
    , m_various(false)
    , m_haveSort(false)
    , m_actualArtist(artistName==data ? QString() : artistName)
{
    if (!artistSort.isEmpty()) {
        m_sortString=artistSort;
        m_haveSort=true;
    } else if (m_itemData.startsWith(constThe)) {
        m_sortString=m_itemData.mid(4);
    }

    if (Song::isVariousArtists(m_itemData)) {
        m_various=true;
    }
}

#ifdef ENABLE_UBUNTU
const QString & MusicLibraryItemArtist::cover() const
{
    if (m_various) {
        return constDefaultVariousCover;
    }
    
    if (m_coverName.isEmpty() && !m_coverRequested && childCount()) {
        m_coverRequested=true;
        m_coverName=Covers::self()->requestImage(coverSong()).fileName;
        if (!m_coverName.isEmpty()) {
            m_coverRequested=false;
        }
    }

    return m_coverName.isEmpty() ? constDefaultCover : m_coverName;
}

void MusicLibraryItemArtist::setCover(const QString &c)
{
    if (!c.isEmpty()) {
        m_coverName="file://"+c;
    }
    m_coverRequested=false;
}
#endif

MusicLibraryItemAlbum * MusicLibraryItemArtist::album(const Song &s, bool create)
{
    MusicLibraryItemAlbum *albumItem=getAlbum(s.albumId());
    return albumItem ? albumItem : (create ? createAlbum(s) : 0);
}

MusicLibraryItemAlbum * MusicLibraryItemArtist::createAlbum(const Song &s)
{
    // If grouping via composer - then album name *might* need to include artist name (if this is different to composer)
    // So, when creating an album entry we need to use the "Album (Artist)" value for display/sort, and still store just
    // "Album" (for saving to cache, tag editing, etc.)
    QString albumId=s.albumId();
    MusicLibraryItemAlbum *item=new MusicLibraryItemAlbum(s.albumName(), s.album, s.mbAlbumId(), s.year, s.albumSort(), this);
    m_indexes.insert(albumId, m_childItems.count());
    m_childItems.append(item);
    return item;
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
    s.album=i18n("Single Tracks");
    MusicLibraryItemAlbum *single=album(s);
    foreach (MusicLibraryItem *album, other->childItems()) {
        single->addTracks(static_cast<MusicLibraryItemAlbum *>(album));
    }
}

bool MusicLibraryItemArtist::isFromSingleTracks(const Song &s) const
{
    if (Song::SingleTracks==s.type) {
        return true;
    }

    MusicLibraryItemAlbum *st=getAlbum(i18n("Single Tracks"));
    return st && st->isSingleTrackFile(s);
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
    m_indexes.remove(album->albumId());
    delete m_childItems.takeAt(index);
    resetRows();
}

Song MusicLibraryItemArtist::coverSong() const
{
    Song song;
    song.albumartist=song.title=m_itemData; // If title is empty, then Song::isUnknown() will be true!!!

    if (childCount()) {
        MusicLibraryItemAlbum *firstAlbum=static_cast<MusicLibraryItemAlbum *>(childItem(0));
        MusicLibraryItemSong *firstSong=firstAlbum ? static_cast<MusicLibraryItemSong *>(firstAlbum->childItem(0)) : 0;

        if (firstSong) {
            song.file=firstSong->file();
            if (Song::useComposer() && !firstSong->song().composer().isEmpty()) {
                song.albumartist=firstSong->song().albumArtist();
            }
        }
    }
    song.setArtistImageRequest();
    return song;
}

MusicLibraryItemAlbum * MusicLibraryItemArtist::getAlbum(const QString &key) const
{
    if (m_indexes.count()==m_childItems.count()) {
        if (m_childItems.isEmpty()) {
            return 0;
        }

        QHash<QString, int>::ConstIterator idx=m_indexes.find(key);

        if (m_indexes.end()==idx) {
            return 0;
        }

        // Check index value is within range
        if (*idx>=0 && *idx<m_childItems.count()) {
            MusicLibraryItemAlbum *a=static_cast<MusicLibraryItemAlbum *>(m_childItems.at(*idx));
            // Check id actually matches!
            if (a->albumId()==key) {
                return a;
            }
        }
    }

    // Something wrong with m_indexes??? So, refresh them...
    MusicLibraryItemAlbum *al=0;
    m_indexes.clear();
    QList<MusicLibraryItem *>::ConstIterator it=m_childItems.constBegin();
    QList<MusicLibraryItem *>::ConstIterator end=m_childItems.constEnd();
    for (int i=0; it!=end; ++it, ++i) {
        MusicLibraryItemAlbum *currenAlbum=static_cast<MusicLibraryItemAlbum *>(*it);
        if (!al && currenAlbum->albumId()==key) {
            al=currenAlbum;
        }
        m_indexes.insert(currenAlbum->albumId(), i);
    }
    return al;
}
