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

#ifndef MUSIC_LIBRARY_ITEM_ALBUM_H
#define MUSIC_LIBRARY_ITEM_ALBUM_H

#include <QList>
#include <QVariant>
#include <QSet>
#include "musiclibraryitem.h"
#include "mpd-interface/song.h"

class QPixmap;
class MusicLibraryItemArtist;
class MusicLibraryItemSong;

class MusicLibraryItemAlbum : public MusicLibraryItemContainer
{
public:
    static void setSortByDate(bool sd);
    static bool sortByDate();

    static bool lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b);

    MusicLibraryItemAlbum(const QString &data, const QString &original, const QString &mbId, quint32 year, const QString &sort, MusicLibraryItemContainer *parent);
    virtual ~MusicLibraryItemAlbum();

    QString displayData(bool full=false) const;
    #ifdef ENABLE_UBUNTU
    const QString & cover() const;
    #endif
    quint32 year() const { return m_year; }
    quint32 totalTime();
    quint32 trackCount();
    void addTracks(MusicLibraryItemAlbum *other);
    bool isSingleTracks() const { return Song::SingleTracks==m_type; }
    void setIsSingleTracks();
    bool isSingleTrackFile(const Song &s) const { return m_singleTrackFiles.contains(s.file); }
    void append(MusicLibraryItem *i);
    void remove(int row);
    void remove(MusicLibraryItemSong *i);
    void removeAll(const QSet<QString> &fileNames);
    QMap<QString, Song> getSongs(const QSet<QString> &fileNames) const;
    Song::Type songType() const { return m_type; }
    Type itemType() const { return Type_Album; }
    const MusicLibraryItemSong * getCueFile() const;
    const QString & imageUrl() const { return m_imageUrl; }
    void setImageUrl(const QString &u) { m_imageUrl=u; }
    bool updateYear();
    bool containsArtist(const QString &a);
    // Return orignal album name. If we are grouping by composer, then album will appear as "Album (Artist)"
    const QString & originalName() const { return m_originalName; }
    const QString & id() const { return m_id; }
    const QString & albumId() const { return m_id.isEmpty() ? (m_originalName.isEmpty() ? m_itemData : m_originalName) : m_id; }
    const QString & sortString() const { return m_sortString.isEmpty() ? m_itemData : m_sortString; }
    bool hasSort() const { return !m_sortString.isEmpty(); }
    #ifdef ENABLE_UBUNTU
    void setCover(const QString &c) { m_coverName="file://"+c; m_coverRequested=false; }
    const QString & coverName() { return m_coverName; }
    #endif
    Song coverSong() const;

private:
    void setYear(const MusicLibraryItemSong *song);
    void updateStats();

private:
    quint32 m_year;
    quint16 m_yearOfTrack;
    quint16 m_yearOfDisc;
    quint32 m_totalTime;
    quint32 m_numTracks;
    QString m_originalName;
    QString m_sortString;
    QString m_id;
    mutable Song m_coverSong;
    #ifdef ENABLE_UBUNTU
    mutable bool m_coverRequested;
    mutable QString m_coverName;
    #endif
    Song::Type m_type;
    QSet<QString> m_singleTrackFiles;
    QString m_imageUrl;
    // m_artists is used to cache the list of artists in a various artists album
    // this is built when containsArtist() is called, and is mainly used by the
    // context view
    QSet<QString> m_artists;
};

#endif
