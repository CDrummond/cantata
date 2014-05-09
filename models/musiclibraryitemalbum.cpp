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
#include "song.h"
#include "covers.h"
#include "icons.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "device.h"
#include "utils.h"
#endif
#ifdef ENABLE_ONLINE_SERVICES
#include "onlineservice.h"
#include "onlineservicesmodel.h"
#endif
#include <QPixmap>
#include <QApplication>
#include <QFontMetrics>

static MusicLibraryItemAlbum::CoverSize coverSize=MusicLibraryItemAlbum::CoverNone;
#ifdef ENABLE_UBUNTU
static const QString constDefaultCover=QLatin1String("qrc:/album.svg");
#else
static QPixmap *theDefaultIcon=0;
#endif
static bool dateSort=false;

#ifndef ENABLE_UBUNTU
static QSize iconItemSize;

static inline int adjust(int v)
{
    if (v>48) {
        static const int constStep=4;
        return (((int)(v/constStep))*constStep)+((v%constStep) ? constStep : 0);
    } else {
        return Icon::stdSize(v);
    }
}

static int fontHeight=16;

void MusicLibraryItemAlbum::setup()
{
    fontHeight=QApplication::fontMetrics().height();
}

int MusicLibraryItemAlbum::iconSize(MusicLibraryItemAlbum::CoverSize sz, bool iconMode)
{
    if (CoverNone==sz) {
        return 0;
    }

    if (iconMode) {
        switch (sz) {
        case CoverSmall:      return qMax(76, adjust((4.75*fontHeight)+0.5));
        default:
        case CoverMedium:     return qMax(100, adjust((6.25*fontHeight)+0.5));
        case CoverLarge:      return qMax(128, adjust(8*fontHeight));
        case CoverExtraLarge: return qMax(160, adjust(10*fontHeight));
        }
    } else {
        switch (sz) {
        case CoverSmall:      return qMax(22, adjust((1.375*fontHeight)+0.5));
        default:
        case CoverMedium:     return qMax(32, adjust(2*fontHeight));
        case CoverLarge:      return qMax(48, adjust(3*fontHeight));
        case CoverExtraLarge: return qMax(64, adjust(4*fontHeight));
        }
    }
}

int MusicLibraryItemAlbum::iconSize(bool iconMode)
{
    return MusicLibraryItemAlbum::iconSize(coverSize, iconMode);
}

void MusicLibraryItemAlbum::setItemSize(const QSize &sz)
{
    iconItemSize=sz;
}

QSize MusicLibraryItemAlbum::itemSize()
{
    return iconItemSize;
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
        MusicLibraryItemArtist::clearDefaultCover();
        coverSize=size;
    }
}
#endif

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
        int compare=aa->data().localeAwareCompare(ab->data());
        return compare==0 ? aa->id().compare(ab->id())<0 : compare<0;
    }
    return aa->year()<ab->year();
}

MusicLibraryItemAlbum::MusicLibraryItemAlbum(const QString &data, const QString &original, const QString &mbId, quint32 year, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(data, parent)
    , m_year(year)
    , m_yearOfTrack(0xFFFF)
    , m_yearOfDisc(0xFFFF)
    , m_totalTime(0)
    , m_numTracks(0)
    , m_originalName(original!=data ? original : QString())
    , m_id(mbId)
    , m_coverRequested(false)
    , m_type(Song::Standard)
{
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
        MusicLibraryItemSong *firstSong=static_cast<MusicLibraryItemSong*>(childItem(0));
        Song song;
        song.artist=firstSong->song().artist;
        song.albumartist=Song::useComposer() && !firstSong->song().composer.isEmpty() ? firstSong->song().albumArtist() : parentItem()->data();
        song.album=Song::useComposer() ? firstSong->song().album : m_itemData;
        song.year=m_year;
        song.file=firstSong->file();
        song.type=m_type;
        song.composer=firstSong->song().composer;
        m_coverName=Covers::self()->requestImage(song).fileName;
        if (!m_coverName.isEmpty()) {
            m_coverRequested=false;
        }
    }

    return m_coverName.isEmpty() ? constDefaultCover : m_coverName;
}
#else
QPixmap *MusicLibraryItemAlbum::saveToCache(const QImage &img) const
{
    int size=MusicLibraryItemAlbum::iconSize(largeImages());
    return Covers::self()->saveScaledCover(img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation), coverSong(), size);
}

const QPixmap & MusicLibraryItemAlbum::cover() const
{
    int iSize=iconSize(largeImages());
    if (Song::SingleTracks!=m_type && parentItem() && iSize && childCount()) {
        Song song=coverSong();

        QPixmap *pix=Covers::self()->getScaledCover(song, iSize);
        if (pix) {
            return *pix;
        }

        if (!m_coverRequested) {
            MusicLibraryItemSong *firstSong=static_cast<MusicLibraryItemSong*>(childItem(0));
            song.year=m_year;
            song.file=firstSong->file();
            song.type=m_type;
            song.composer=firstSong->song().composer;
            Covers::Image img;
            MusicLibraryItemRoot *root=parentItem() && parentItem()->parentItem() && MusicLibraryItem::Type_Root==parentItem()->parentItem()->itemType()
                    ? static_cast<MusicLibraryItemRoot *>(parentItem()->parentItem()) : 0;
            m_coverRequested=true;
            if (root && !root->useAlbumImages()) {
                // Not showing album images in this model, so dont request any!
            }
            #ifdef ENABLE_DEVICES_SUPPORT
            else if (root && root->isDevice()) {
                song.id=firstSong->song().id;
                static_cast<Device *>(parentItem()->parentItem())->requestCover(song);
            }
            #endif
            #ifdef ENABLE_ONLINE_SERVICES
            else if (root && root->isOnlineService()) {
                img.img=OnlineServicesModel::self()->requestImage(static_cast<OnlineService *>(root)->id(), parentItem()->data(), data(), m_imageUrl);
            }
            #endif
            else {
                img=Covers::self()->requestImage(song);
            }

            if (!img.img.isNull()) {
                pix=saveToCache(img.img);
                if (pix) {
                    return *pix;
                }
            }
        }
    }

    int cSize=iSize;
    if (0==cSize) {
        cSize=22;
    }

    if (!theDefaultIcon || theDefaultIcon->width()!=cSize) {
        if (theDefaultIcon) {
            delete theDefaultIcon;
        }
        theDefaultIcon = new QPixmap(Icons::self()->albumIcon.pixmap(cSize, cSize)
                                     .scaled(QSize(cSize, cSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
    return *theDefaultIcon;
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
    MusicLibraryItemSong *firstSong=static_cast<MusicLibraryItemSong*>(childItem(0));
    Song song;
    song.artist=firstSong->song().artist;
    song.albumartist=Song::useComposer() && !firstSong->song().composer.isEmpty() ? firstSong->song().albumArtist() : parentItem()->data();
    song.album=Song::useComposer() ? firstSong->song().album : m_itemData;
    song.mbAlbumId=firstSong->song().mbAlbumId;
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

bool MusicLibraryItemAlbum::largeImages() const
{
    return m_parentItem && m_parentItem->parentItem() && Type_Root==m_parentItem->parentItem()->itemType() &&
           static_cast<MusicLibraryItemRoot *>(m_parentItem->parentItem())->useLargeImages();
}
