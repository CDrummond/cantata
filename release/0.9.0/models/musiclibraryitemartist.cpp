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

void MusicLibraryItemArtist::clearDefaultCover()
{
    if (theDefaultIcon) {
        delete theDefaultIcon;
        theDefaultIcon=0;
    }
}

MusicLibraryItemArtist::MusicLibraryItemArtist(const QString &data, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(data, parent)
    , m_coverIsDefault(false)
    , m_cover(0)
    , m_various(false)
{
    if (m_itemData.startsWith(QLatin1String("The "))) {
        m_nonTheArtist=m_itemData.mid(4);
    } else if (Song::isVariousArtists(m_itemData)) {
        m_various=true;
    }
}

bool MusicLibraryItemArtist::setCover(const QImage &img) const
{
    if (m_coverIsDefault && !img.isNull()) {
        int size=MusicLibraryItemAlbum::iconSize(!MusicLibraryItemAlbum::itemSize().isNull());
        QImage scaled=img.scaled(QSize(size, size), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        m_cover = new QPixmap(QPixmap::fromImage(scaled.width()>size || scaled.height()>size ? scaled.copy((scaled.width()-size)/2, (scaled.height()-size)/2, size, size) : scaled));
        m_coverIsDefault=false;
        return true;
    }

    return false;
}

const QPixmap & MusicLibraryItemArtist::cover()
{
    if (m_coverIsDefault && theDefaultIcon) {
        return *theDefaultIcon;
    }

    if (!m_cover) {
        int iSize=MusicLibraryItemAlbum::iconSize(!MusicLibraryItemAlbum::itemSize().isNull());
        int cSize=iSize;
        if (0==cSize) {
            cSize=22;
        }

        if (m_various) {
            m_cover = new QPixmap(Icons::variousArtistsIcon.pixmap(cSize, cSize).scaled(QSize(cSize, cSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_coverIsDefault=false;
        } else {
            if (!theDefaultIcon) {
                theDefaultIcon = new QPixmap(Icons::artistIcon.pixmap(cSize, cSize)
                                            .scaled(QSize(cSize, cSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            m_coverIsDefault = true;
            Song song;
            song.albumartist=m_itemData;

            MusicLibraryItemAlbum *firstAlbum=static_cast<MusicLibraryItemAlbum *>(childItem(0));
            MusicLibraryItemSong *firstSong=firstAlbum ? static_cast<MusicLibraryItemSong *>(firstAlbum->childItem(0)) : 0;

            if (firstSong) {
                song.file=firstSong->file();
                // NO ARTIST IMAGES FOR DEVICES!
                //#ifdef ENABLE_DEVICES_SUPPORT
                //if (!song.file.startsWith("/") && parent() && qobject_cast<Device *>(parent())) {
                //    QString root=static_cast<Device *>(parent())->path();
                //    if (!root.isEmpty()) {
                //        song.file=Utils::fixPath(root)+song.file;
                //    }
                //}
                //#endif
            }
            Covers::self()->requestCover(song, true);
            return *theDefaultIcon;
        }
    }

    return *m_cover;
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
