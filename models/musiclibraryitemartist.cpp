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
#include "song.h"
#include "mpdparseutils.h"
#include "localize.h"
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

bool MusicLibraryItemArtist::lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b)
{
    const MusicLibraryItemArtist *aa=static_cast<const MusicLibraryItemArtist *>(a);
    const MusicLibraryItemArtist *ab=static_cast<const MusicLibraryItemArtist *>(b);

    if (aa->isVarious() != ab->isVarious()) {
        return aa->isVarious() > ab->isVarious();
    }
    return aa->baseArtist().localeAwareCompare(ab->baseArtist())<0;
}

static QPixmap *theDefaultIcon=0;
static QPixmap *theVariousArtistsIcon=0;

void MusicLibraryItemArtist::clearDefaultCover()
{
    if (theDefaultIcon) {
        delete theDefaultIcon;
        theDefaultIcon=0;
    }
    if (theVariousArtistsIcon) {
        delete theVariousArtistsIcon;
        theVariousArtistsIcon=0;
    }
}

MusicLibraryItemArtist::MusicLibraryItemArtist(const QString &data, const QString &artistName, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(data, parent)
    , m_coverRequested(false)
    , m_various(false)
    , m_actualArtist(artistName==data ? QString() : artistName)
{
    if (m_itemData.startsWith(QLatin1String("The "))) {
        m_nonTheArtist=m_itemData.mid(4);
    } else if (Song::isVariousArtists(m_itemData)) {
        m_various=true;
    }
}

bool MusicLibraryItemArtist::setCover(const QImage &img, bool update) const
{
    if ((m_coverRequested || update) && !img.isNull()) {
        int size=MusicLibraryItemAlbum::iconSize(largeImages());
        QImage scaled=img.scaled(QSize(size, size), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        if (scaled.width()>size || scaled.height()>size) {
            scaled=scaled.copy((scaled.width()-size)/2, 0, size, size);
        }
        m_coverRequested=false;
        Covers::self()->saveScaledCover(scaled, data(), QString(), size);
        return true;
    }

    return false;
}

const QPixmap & MusicLibraryItemArtist::cover() const
{
    int iSize=MusicLibraryItemAlbum::iconSize(largeImages());
    int cSize=iSize;
    if (0==cSize) {
        cSize=22;
    }
    if (m_various) {
        if (!theVariousArtistsIcon || theVariousArtistsIcon->width()!=cSize) {
            if (theVariousArtistsIcon) {
                delete theVariousArtistsIcon;
            }
            theVariousArtistsIcon = new QPixmap(Icons::self()->variousArtistsIcon.pixmap(cSize, cSize)
                                         .scaled(QSize(cSize, cSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        return *theVariousArtistsIcon;
    }

    QPixmap *pix=Covers::self()->getScaledCover(data(), QString(), iSize);
    if (pix) {
        return *pix;
    }

    if (!m_coverRequested) {
        Covers::Image img;
        Song song;
        song.albumartist=m_itemData;

        MusicLibraryItemAlbum *firstAlbum=static_cast<MusicLibraryItemAlbum *>(childItem(0));
        MusicLibraryItemSong *firstSong=firstAlbum ? static_cast<MusicLibraryItemSong *>(firstAlbum->childItem(0)) : 0;

        if (firstSong) {
            song.file=firstSong->file();
            if (Song::useComposer() && !firstSong->song().composer.isEmpty()) {
                song.albumartist=firstSong->song().albumArtist();
            }
        }
        MusicLibraryItemRoot *root=parentItem() && MusicLibraryItem::Type_Root==parentItem()->itemType()
                ? static_cast<MusicLibraryItemRoot *>(parentItem()) : 0;
        // NO ARTIST IMAGES FOR DEVICES!
        //#ifdef ENABLE_DEVICES_SUPPORT
        //if (root && root->isDevice()) {
        //    // This item is in the devices model, so get cover from device...
        //    song.id=firstSong->song().id;
        //    static_cast<Device *>(root)->requestArtistImage(song);
        //} else
        //#endif
        m_coverRequested=true;
        if (root && !root->useArtistImages()) {
            // Not showing artist images in this model, so dont request any!
        }
        #ifdef ENABLE_ONLINE_SERVICES
        else if (root && root->isOnlineService()) {
            img.img=OnlineServicesModel::self()->requestImage(static_cast<OnlineService *>(parentItem())->id(), data(), QString(), m_imageUrl);
        }
        #endif
        else {
            img=Covers::self()->requestImage(song);
        }

        if (!img.img.isNull()) {
            setCover(img.img);
            pix=Covers::self()->getScaledCover(data(), QString(), iSize);
            if (pix) {
                return *pix;
            }
        }
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

MusicLibraryItemAlbum * MusicLibraryItemArtist::album(const Song &s, bool create)
{
    QHash<QString, int>::ConstIterator it=m_indexes.find(s.albumName());

    if (m_indexes.end()==it) {
        return create ? createAlbum(s) : 0;
    }
    return static_cast<MusicLibraryItemAlbum *>(m_childItems.at(*it));
}

MusicLibraryItemAlbum * MusicLibraryItemArtist::createAlbum(const Song &s)
{
    // If grouping via composer - then album name *might* need to include artist name (if this is different to composer)
    // So, when creating an album entry we need to use the "Album (Artist)" value for display/sort, and still store just
    // "Album" (for saving to cache, tag editing, etc.)
    QString albumName=s.albumName();
    MusicLibraryItemAlbum *item=new MusicLibraryItemAlbum(albumName, s.album, s.year, this);
    m_indexes.insert(albumName, m_childItems.count());
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

    QHash<QString, int>::ConstIterator it=m_indexes.find(i18n("Single Tracks"));

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
    resetRows();
}

QList<MusicLibraryItem *> MusicLibraryItemArtist::mutipleArtistAlbums()
{
    if (isVarious()) {
        return m_childItems;
    }

    QList<MusicLibraryItem *> ma;
    QList<MusicLibraryItem *>::iterator it=m_childItems.begin();
    QList<MusicLibraryItem *>::iterator end=m_childItems.end();
    for(; it!=end; ++it) {
        if (static_cast<MusicLibraryItemAlbum *>(*it)->detectIfIsMultipleArtists()) {
            ma.append(*it);
        }
    }
    return ma;
}

void MusicLibraryItemArtist::updateIndexes()
{
    m_indexes.clear();
    QList<MusicLibraryItem *>::iterator it=m_childItems.begin();
    QList<MusicLibraryItem *>::iterator end=m_childItems.end();
    for (int i=0; it!=end; ++it, ++i) {
        m_indexes.insert((*it)->data(), i);
    }
}

bool MusicLibraryItemArtist::largeImages() const
{
    return m_parentItem && Type_Root==m_parentItem->itemType() &&
           static_cast<MusicLibraryItemRoot *>(m_parentItem)->useLargeImages();
}
