/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "icons.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "device.h"
#include "utils.h"
#endif
#include "onlineservice.h"
#include <QPixmap>
#include <QApplication>
#include <QFontMetrics>
#include <QFile>

static MusicLibraryItemAlbum::CoverSize coverSize=MusicLibraryItemAlbum::CoverNone;
static QPixmap *theDefaultIcon=0;
static QPixmap *theDefaultLargeIcon=0;
static bool useDate=false;
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

#ifdef CACHE_SCALED_COVERS
static QString cacheCoverName(const QString &artist, const QString &album, int size, bool createDir=false)
{
    return Utils::cacheDir(Covers::constCoverDir+QString::number(size)+"/"+Covers::encodeName(artist), createDir)+Covers::encodeName(album)+".png";
}
#endif

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
        if (theDefaultLargeIcon) {
            delete theDefaultLargeIcon;
            theDefaultLargeIcon=0;
        }
        MusicLibraryItemArtist::clearDefaultCover();
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

bool MusicLibraryItemAlbum::lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b)
{
    const MusicLibraryItemAlbum *aa=static_cast<const MusicLibraryItemAlbum *>(a);
    const MusicLibraryItemAlbum *ab=static_cast<const MusicLibraryItemAlbum *>(b);

    if (aa->isSingleTracks() != ab->isSingleTracks()) {
        return aa->isSingleTracks() > ab->isSingleTracks();
    }

    if (!MusicLibraryItemAlbum::showDate() || aa->year()==ab->year()) {
        return aa->data().localeAwareCompare(ab->data())<0;
    }
    return aa->year()<ab->year();
}

MusicLibraryItemAlbum::MusicLibraryItemAlbum(const QString &data, quint32 year, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(data, parent)
    , m_year(year)
    , m_yearOfTrack(0xFFFF)
    , m_yearOfDisc(0xFFFF)
    , m_totalTime(0)
    , m_coverIsDefault(false)
    , m_cover(0)
    , m_type(Song::Standard)
{
}

MusicLibraryItemAlbum::~MusicLibraryItemAlbum()
{
    delete m_cover;
}

void MusicLibraryItemAlbum::setCoverImage(const QImage &img) const
{
    int size=iconSize(largeImages());
    QImage scaled=img.scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_cover = new QPixmap(QPixmap::fromImage(scaled));
    m_coverIsDefault=false;
    #ifdef CACHE_SCALED_COVERS
    scaled.save(cacheCoverName(parentItem()->data(), data(), size, true));
    #endif
}

bool MusicLibraryItemAlbum::setCover(const QImage &img, bool update) const
{
    if ((update || m_coverIsDefault) && !img.isNull()) {
        setCoverImage(img);
        return true;
    }

    return false;
}

const QPixmap & MusicLibraryItemAlbum::cover()
{
    if (m_coverIsDefault) {
        if (largeImages()) {
            if (theDefaultLargeIcon) {
                return *theDefaultLargeIcon;
            }
        } else if (theDefaultIcon) {
            return *theDefaultIcon;
        }
    }

    if (!m_cover) {
        bool useLarge=largeImages();
        int iSize=iconSize(useLarge);

        if ((useLarge && !theDefaultLargeIcon) || (!useLarge && !theDefaultIcon)) {
            int cSize=iSize;
            if (0==cSize) {
                cSize=22;
            }
            if (useLarge) {
                theDefaultLargeIcon = new QPixmap(Icons::self()->albumIcon.pixmap(cSize, cSize)
                                                 .scaled(QSize(cSize, cSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            } else {
                theDefaultIcon = new QPixmap(Icons::self()->albumIcon.pixmap(cSize, cSize)
                                            .scaled(QSize(cSize, cSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            }
        }
        m_coverIsDefault = true;
        if (Song::SingleTracks!=m_type && parentItem() && iSize && childCount()) {
            #ifdef CACHE_SCALED_COVERS
            QString cache=cacheCoverName(parentItem()->data(), data(), iSize);

            if (QFile::exists(cache)) {
                QImage img(cache);
                if (!img.isNull()) {
                    m_cover=new QPixmap(QPixmap::fromImage(img));
                    m_coverIsDefault=false;
                    return *m_cover;
                }
            }
            #endif

            MusicLibraryItemSong *firstSong=static_cast<MusicLibraryItemSong*>(childItem(0));
            Song song;
            if (Song::MultipleArtists==m_type) { // Then Cantata has placed this album under 'Various Artists' but the actual album as a different AlbumArtist tag
                song.artist=firstSong->song().albumArtist();
            } else {
                song.artist=firstSong->song().artist;
                song.albumartist=parentItem()->data();
            }
            song.album=m_itemData;
            song.year=m_year;
            song.file=firstSong->file();
            song.type=m_type;
            Covers::Image img;
            #ifdef ENABLE_DEVICES_SUPPORT
            if (parentItem() && parentItem()->parentItem() && dynamic_cast<Device *>(parentItem()->parentItem()) &&
                static_cast<MusicLibraryItemRoot *>(parentItem()->parentItem())->useAlbumImages()) {
                // This item is in the devices model, so get cover from device...
                song.id=firstSong->song().id;
                static_cast<Device *>(parentItem()->parentItem())->requestCover(song);
            } else
            #endif
            if (parentItem() && parentItem()->parentItem() && dynamic_cast<OnlineService *>(parentItem()->parentItem()) &&
                static_cast<MusicLibraryItemRoot *>(parentItem()->parentItem())->useAlbumImages()) {
                // ONLINE: Image URL is encoded in song.name...
                if (!m_imageUrl.isEmpty()) {
                    song.name=m_imageUrl;
                    song.title=parentItem()->parentItem()->data().toLower();
                    img=Covers::self()->requestImage(song);
                }
            } else if (parentItem() && parentItem()->parentItem() && !static_cast<MusicLibraryItemRoot *>(parentItem()->parentItem())->useAlbumImages()) {
                // Not showing album images in this model, so dont request any!
            } else {
                img=Covers::self()->requestImage(song);
            }

            if (!img.img.isNull()) {
                setCoverImage(img.img);
                return *m_cover;
            }
        }
        return useLarge ? *theDefaultLargeIcon : *theDefaultIcon;
    }

    return *m_cover;
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

void MusicLibraryItemAlbum::setIsMultipleArtists()
{
    foreach (MusicLibraryItem *track, m_childItems) {
        static_cast<MusicLibraryItemSong*>(track)->song().type=Song::MultipleArtists;
    }
    m_type=Song::MultipleArtists;
}

void MusicLibraryItemAlbum::append(MusicLibraryItem *i)
{
    MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
    setYear(song);
    MusicLibraryItemContainer::append(i);
    if (Song::SingleTracks==m_type) {
        song->song().type=Song::SingleTracks;
        m_singleTrackFiles.insert(song->song().file);
    } else if (Song::MultipleArtists==m_type) {
        song->song().type=Song::MultipleArtists;
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

const MusicLibraryItemSong * MusicLibraryItemAlbum::getCueFile() const
{
    foreach (const MusicLibraryItem *s, m_childItems) {
        if (Song::Playlist==static_cast<const MusicLibraryItemSong *>(s)->song().type &&
            static_cast<const MusicLibraryItemSong *>(s)->song().file.endsWith(".cue", Qt::CaseInsensitive)) {
            return static_cast<const MusicLibraryItemSong *>(s);
        }
    }

    return 0;
}

bool MusicLibraryItemAlbum::updateYear()
{
    quint32 currentYear=m_year;
    foreach (MusicLibraryItem *track, m_childItems) {
        m_year=static_cast<MusicLibraryItemSong*>(track)->song().year;
        if (m_year==currentYear) {
            return false;
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

void MusicLibraryItemAlbum::clearImage()
{
    if (!m_coverIsDefault) {
        m_coverIsDefault=true;
        delete m_cover;
        m_cover=0;
        if (theDefaultIcon) {
            m_cover=theDefaultIcon;
        }
    }
}

void MusicLibraryItemAlbum::setYear(const MusicLibraryItemSong *song)
{
    if (m_childItems.isEmpty() || (m_yearOfDisc>song->disc() || (m_yearOfDisc==song->disc() && m_yearOfTrack>song->track()))) {
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
