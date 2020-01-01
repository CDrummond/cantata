/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "widgets/icons.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devices/device.h"
#include "support/utils.h"
#endif

static bool dateSort=false;

void MusicLibraryItemAlbum::setSortByDate(bool sd)
{
    dateSort=sd;
}

bool MusicLibraryItemAlbum::sortByDate()
{
    return dateSort;
}

bool MusicLibraryItemAlbum::lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b)
{
    const MusicLibraryItemAlbum *aa=static_cast<const MusicLibraryItemAlbum *>(a);
    const MusicLibraryItemAlbum *ab=static_cast<const MusicLibraryItemAlbum *>(b);

    if (!MusicLibraryItemAlbum::sortByDate() || aa->year()==ab->year()) {
        int compare=aa->sortString().localeAwareCompare(ab->sortString());
        return compare==0 ? aa->id().compare(ab->id())<0 : compare<0;
    }
    return aa->year()<ab->year();
}

static const QLatin1String constThe("The ");

MusicLibraryItemAlbum::MusicLibraryItemAlbum(const Song &song, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(song.album, parent)
    , m_year(song.year)
    , m_yearOfTrack(0xFFFF)
    , m_yearOfDisc(0xFFFF)
    , m_numTracks(0)
    , m_totalTime(0)
    , m_sortString(song.hasAlbumSort() ? song.albumSort() : QString())
    , m_id(song.hasMbAlbumId() ? song.mbAlbumId() : QString())
{
}

MusicLibraryItemAlbum::~MusicLibraryItemAlbum()
{
}

QString MusicLibraryItemAlbum::displayData(bool full) const
{
    return dateSort || full ? Song::displayAlbum(m_itemData, m_year) : m_itemData;
}

quint32 MusicLibraryItemAlbum::totalTime()
{
    updateStats();
    return m_totalTime;
}

quint32 MusicLibraryItemAlbum::trackCount()
{
    updateStats();
    return m_numTracks;
}

void MusicLibraryItemAlbum::updateStats()
{
    if (0==m_totalTime) {
        m_numTracks=0;
        for (MusicLibraryItem *i: m_childItems) {
            MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
            if (Song::Playlist!=song->song().type) {
                m_totalTime+=song->time();
                m_numTracks++;
            }
        }
    }
}

void MusicLibraryItemAlbum::append(MusicLibraryItem *i)
{
    MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
    setYear(song);
    MusicLibraryItemContainer::append(i);
    m_totalTime=0;
}

void MusicLibraryItemAlbum::remove(int row)
{
    MusicLibraryItem *i=m_childItems.takeAt(row);
    MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
    if (m_yearOfDisc==song->disc() && m_yearOfTrack==song->track()) {
        m_yearOfDisc=m_yearOfTrack=0xFFFF;
        for (MusicLibraryItem *itm: m_childItems) {
            setYear(static_cast<MusicLibraryItemSong *>(itm));
        }
    }
    delete i;
    m_totalTime=0;
    resetRows();
}

void MusicLibraryItemAlbum::remove(MusicLibraryItemSong *i)
{
    int idx=m_childItems.indexOf(i);
    if (-1!=idx) {
        remove(idx);
    }
    resetRows();
}

void MusicLibraryItemAlbum::removeAll(const QSet<QString> &fileNames)
{
    QSet<QString> fn=fileNames;
    for (int i=0; i<m_childItems.count() && !fn.isEmpty();) {
        MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(m_childItems.at(i));
        if (fn.contains(song->file())) {
            fn.remove(song->file());
            delete m_childItems.takeAt(i);
            m_totalTime=0;
        } else {
            ++i;
        }
    }
    resetRows();
}

QMap<QString, Song> MusicLibraryItemAlbum::getSongs(const QSet<QString> &fileNames) const
{
    QMap<QString, Song> map;
    for (const MusicLibraryItem *i: m_childItems) {
        const MusicLibraryItemSong *song=static_cast<const MusicLibraryItemSong *>(i);
        if (fileNames.contains(song->file())) {
            map.insert(song->file(), song->song());
            if (map.size()==fileNames.size()) {
                return map;
            }
        }
    }

    return map;
}

bool MusicLibraryItemAlbum::updateYear()
{
    quint32 currentYear=m_year;
    for (MusicLibraryItem *track: m_childItems) {
        MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong*>(track);
        if (Song::Playlist!=song->song().type) {
            m_year=song->song().year;
            // Store which track/disc we obtained the year from!
            m_yearOfTrack=song->track();
            m_yearOfDisc=song->disc();
            if (m_year==currentYear) {
                return false;
            }
        }
    }
    return true;
}

Song MusicLibraryItemAlbum::coverSong() const
{
    Song song;
    if (childCount()) {
        MusicLibraryItemSong *firstSong=static_cast<MusicLibraryItemSong*>(childItem(0));
        song.artist=firstSong->song().artist;
        song.albumartist=/*Song::useComposer() && !firstSong->song().composer().isEmpty() ? */firstSong->song().albumArtist() /*: parentItem()->data()*/;
        song.album=/*Song::useComposer() ? */firstSong->song().album /*: m_itemData*/;
        song.setMbAlbumId(firstSong->song().mbAlbumId());
        song.setComposer(firstSong->song().composer());
        song.year=m_year;
        song.file=firstSong->file();
        #if defined ENABLE_DEVICES_SUPPORT
        MusicLibraryItemRoot *root=parentItem() && parentItem()->parentItem() && MusicLibraryItem::Type_Root==parentItem()->parentItem()->itemType()
                                                ? static_cast<MusicLibraryItemRoot *>(parentItem()->parentItem()) : nullptr;
        if (root) {
            #ifdef ENABLE_DEVICES_SUPPORT
            if (root->isDevice()) {
                song.setIsFromDevice(static_cast<Device *>(root)->id());
            }
            #endif
        }
        #endif
    }
    return song;
}

void MusicLibraryItemAlbum::setYear(const MusicLibraryItemSong *song)
{
    if (Song::Playlist!=song->song().type &&
        (m_childItems.isEmpty() || (m_yearOfDisc>song->disc() || (m_yearOfDisc==song->disc() && m_yearOfTrack>song->track())))) {
        m_year=song->song().year;
        // Store which track/disc we obtained the year from!
        m_yearOfTrack=song->track();
        m_yearOfDisc=song->disc();
        Song::storeAlbumYear(song->song());
    }
}
