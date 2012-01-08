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

#ifndef MUSIC_LIBRARY_ITEM_ALBUM_H
#define MUSIC_LIBRARY_ITEM_ALBUM_H

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtGui/QPixmap>
#include "musiclibraryitem.h"
#include "song.h"

class MusicLibraryItemArtist;
class MusicLibraryItemSong;

class MusicLibraryItemAlbum : public MusicLibraryItem
{
public:
    enum CoverSize
    {
        CoverNone   = 0,
        CoverSmall  = 1,
        CoverMedium = 2,
        CoverLarge  = 3
    };

    static CoverSize currentCoverSize();
    static void setCoverSize(CoverSize size);
    static int iconSize(MusicLibraryItemAlbum::CoverSize sz);
    static int iconSize();
    static void setShowDate(bool sd);
    static bool showDate();

    MusicLibraryItemAlbum(const QString &data, const QString &dir, quint32 year, MusicLibraryItem *parent);
    virtual ~MusicLibraryItemAlbum();

    bool setCover(const QImage &img);
    const QPixmap & cover();
    bool hasRealCover() const { return !m_coverIsDefault; }
    const QString & dir() const { return m_dir; }
    QStringList sortedTracks();
    quint32 year() const { return m_year; }
    void addTracks(MusicLibraryItemAlbum *other);
    bool isSingleTracks() const { return m_singleTracks; }
    void setIsSingleTracks();
    bool isSingleTrackFile(const Song &s) const { return m_singleTrackFiles.contains(s.file); }
    void append(MusicLibraryItem *i);

private:
    QString m_dir;
    quint32 m_year;
    bool m_coverIsDefault;
    QPixmap *m_cover;
    bool m_singleTracks;
    QSet<QString> m_singleTrackFiles;
};

#endif
