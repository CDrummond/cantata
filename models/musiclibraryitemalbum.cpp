/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "gui/covers.h"
#include "widgets/icons.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devices/device.h"
#include "support/utils.h"
#endif
#include <QPixmap>
#include <QApplication>
#include <QFontMetrics>

#ifdef ENABLE_UBUNTU
static const QString constDefaultCover=QLatin1String("qrc:/album.svg");
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

    if (aa->isSingleTracks() != ab->isSingleTracks()) {
        return aa->isSingleTracks() > ab->isSingleTracks();
    }

    if (!MusicLibraryItemAlbum::sortByDate() || aa->year()==ab->year()) {
        int compare=aa->sortString().localeAwareCompare(ab->sortString());
        return compare==0 ? aa->id().compare(ab->id())<0 : compare<0;
    }
    return aa->year()<ab->year();
}

static const QLatin1String constThe("The ");

MusicLibraryItemAlbum::MusicLibraryItemAlbum(const QString &data, const QString &original, const QString &mbId, quint32 year, const QString &sort, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(data, parent)
    , m_year(year)
    , m_yearOfTrack(0xFFFF)
    , m_yearOfDisc(0xFFFF)
    , m_totalTime(0)
    , m_numTracks(0)
    , m_originalName(original!=data ? original : QString())
    , m_id(mbId)
    #ifdef ENABLE_UBUNTU
    , m_coverRequested(false)
    #endif
    , m_type(Song::Standard)
{
    if (!sort.isEmpty()) {
        m_sortString=sort;
    } else if (m_itemData.startsWith(constThe)) {
        m_sortString=m_itemData.mid(4);
    }
}

MusicLibraryItemAlbum::~MusicLibraryItemAlbum()
{
}

QString MusicLibraryItemAlbum::displayData(bool full) const
{
    return dateSort || full ? Song::displayAlbum(m_itemData, m_year) : m_itemData;
}

#ifdef ENABLE_UBUNTU
const QString & MusicLibraryItemAlbum::cover() const
{
    if (Song::SingleTracks!=m_type && m_coverName.isEmpty() && !m_coverRequested && childCount()) {
        m_coverName=Covers::self()->requestImage(coverSong()).fileName;
        if (!m_coverName.isEmpty()) {
            m_coverRequested=false;
        }
    }
    return m_coverName.isEmpty() ? constDefaultCover : m_coverName;
}
#endif

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
        foreach (MusicLibraryItem *i, m_childItems) {
            MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
            if (Song::Playlist!=song->song().type) {
                m_totalTime+=song->time();
                m_numTracks++;
            }
        }
    }
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

void MusicLibraryItemAlbum::append(MusicLibraryItem *i)
{
    MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
    setYear(song);
    MusicLibraryItemContainer::append(i);
    if (Song::SingleTracks==m_type) {
        song->song().type=Song::SingleTracks;
        m_singleTrackFiles.insert(song->song().file);
    }
    m_totalTime=0;
    m_artists.clear();
}

void MusicLibraryItemAlbum::remove(int row)
{
    MusicLibraryItem *i=m_childItems.takeAt(row);
    MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
    if (m_yearOfDisc==song->disc() && m_yearOfTrack==song->track()) {
        m_yearOfDisc=m_yearOfTrack=0xFFFF;
        foreach (MusicLibraryItem *itm, m_childItems) {
            setYear(static_cast<MusicLibraryItemSong *>(itm));
        }
    }
    delete i;
    m_totalTime=0;
    m_artists.clear();
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
            m_artists.clear();
        } else {
            ++i;
        }
    }
    resetRows();
}

QMap<QString, Song> MusicLibraryItemAlbum::getSongs(const QSet<QString> &fileNames) const
{
    QMap<QString, Song> map;
    foreach (const MusicLibraryItem *i, m_childItems) {
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

const MusicLibraryItemSong * MusicLibraryItemAlbum::getCueFile() const
{
    foreach (const MusicLibraryItem *s, m_childItems) {
        if (static_cast<const MusicLibraryItemSong *>(s)->song().isCueFile()) {
            return static_cast<const MusicLibraryItemSong *>(s);
        }
    }

    return 0;
}

bool MusicLibraryItemAlbum::updateYear()
{
    quint32 currentYear=m_year;
    foreach (MusicLibraryItem *track, m_childItems) {
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

bool MusicLibraryItemAlbum::containsArtist(const QString &a)
{
    if (m_artists.isEmpty()) {
        foreach (MusicLibraryItem *itm, m_childItems) {
            m_artists.insert(static_cast<MusicLibraryItemSong *>(itm)->song().basicArtist());
        }
    }

    return m_artists.contains(a);
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
        song.type=m_type;
        #if defined ENABLE_DEVICES_SUPPORT
        MusicLibraryItemRoot *root=parentItem() && parentItem()->parentItem() && MusicLibraryItem::Type_Root==parentItem()->parentItem()->itemType()
                                                ? static_cast<MusicLibraryItemRoot *>(parentItem()->parentItem()) : 0;
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
