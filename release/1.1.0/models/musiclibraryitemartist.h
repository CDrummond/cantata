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

#ifndef MUSIC_LIBRARY_ITEM_ARTIST_H
#define MUSIC_LIBRARY_ITEM_ARTIST_H

#include <QList>
#include <QVariant>
#include <QHash>
#include "musiclibraryitem.h"

class MusicLibraryItemRoot;
class MusicLibraryItemAlbum;
struct Song;

class MusicLibraryItemArtist : public MusicLibraryItemContainer
{
public:
    static bool lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b);

    MusicLibraryItemArtist(const QString &data, MusicLibraryItemContainer *parent = 0);
    virtual ~MusicLibraryItemArtist() { }

    MusicLibraryItemAlbum * album(const Song &s, bool create=true);
    MusicLibraryItemAlbum * createAlbum(const Song &s);
    const QString & baseArtist() const;
    bool isVarious() const { return m_various; }
    bool allSingleTrack() const;
    void addToSingleTracks(MusicLibraryItemArtist *other);
    bool isFromSingleTracks(const Song &s) const;
    void remove(MusicLibraryItemAlbum *album);
    QList<MusicLibraryItem *> mutipleArtistAlbums();
    void updateIndexes();
    Type itemType() const { return Type_Artist; }
    static void clearDefaultCover();
    bool setCover(const QImage &img, bool update=false) const;
    const QPixmap & cover();
    bool hasRealCover() const { return !m_coverIsDefault; }
    const QString & imageUrl() const { return m_imageUrl; }
    void setImageUrl(const QString &u) { m_imageUrl=u; }
    void clearImages();

private:
    bool largeImages() const;

private:
    mutable bool m_coverIsDefault;
    mutable QPixmap *m_cover;
    bool m_various;
    QString m_nonTheArtist;
    QHash<QString, int> m_indexes;
    QString m_imageUrl;
};

#endif
