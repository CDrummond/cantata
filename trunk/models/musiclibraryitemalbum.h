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

#ifndef MUSIC_LIBRARY_ITEM_ALBUM_H
#define MUSIC_LIBRARY_ITEM_ALBUM_H

#include <QList>
#include <QVariant>
#include <QPixmap>
#include "musiclibraryitem.h"
#include "song.h"

class MusicLibraryItemArtist;
class MusicLibraryItemSong;

class MusicLibraryItemAlbum : public MusicLibraryItemContainer
{
public:
    enum CoverSize
    {
        CoverNone       = 0,
        CoverSmall      = 1,
        CoverMedium     = 2,
        CoverLarge      = 3,
        CoverExtraLarge = 4
    };

    static void setup();
    static CoverSize currentCoverSize();
    static void setCoverSize(CoverSize size);
    static int iconSize(MusicLibraryItemAlbum::CoverSize sz, bool iconMode=false);
    static int iconSize(bool iconMode=false);
    static void setItemSize(const QSize &sz);
    static QSize itemSize();
    static void setShowDate(bool sd);
    static bool showDate();

    static bool lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b);

    MusicLibraryItemAlbum(const QString &data, quint32 year, MusicLibraryItemContainer *parent);
    virtual ~MusicLibraryItemAlbum();

    bool setCover(const QImage &img, bool update=false) const;
    const QPixmap & cover();
    bool hasRealCover() const { return !m_coverIsDefault; }
    quint32 year() const { return m_year; }
    quint32 totalTime();
    void addTracks(MusicLibraryItemAlbum *other);
    bool isSingleTracks() const { return Song::SingleTracks==m_type; }
    void setIsSingleTracks();
    bool isSingleTrackFile(const Song &s) const { return m_singleTrackFiles.contains(s.file); }
    void append(MusicLibraryItem *i);
    void remove(int row);
    void remove(MusicLibraryItemSong *i);
    bool detectIfIsMultipleArtists();
    bool isMultipleArtists() const { return Song::MultipleArtists==m_type; }
    Song::Type songType() const { return m_type; }
    void setIsMultipleArtists();
    Type itemType() const { return Type_Album; }
    const MusicLibraryItemSong * getCueFile() const;
    const QString & imageUrl() const { return m_imageUrl; }
    void setImageUrl(const QString &u) { m_imageUrl=u; }
    bool updateYear();

private:
    void setYear(const MusicLibraryItemSong *song);
    bool largeImages() const;

private:
    quint32 m_year;
    quint16 m_yearOfTrack;
    quint16 m_yearOfDisc;
    quint32 m_totalTime;
    mutable bool m_coverIsDefault;
    mutable QPixmap *m_cover;
    Song::Type m_type;
    QSet<QString> m_singleTrackFiles;
    QString m_imageUrl;
};

#endif
