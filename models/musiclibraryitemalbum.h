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

    MusicLibraryItemAlbum(const Song &song, MusicLibraryItemContainer *parent);
    ~MusicLibraryItemAlbum() override;

    QString displayData(bool full=false) const override;
    quint32 year() const { return m_year; }
    quint32 totalTime();
    quint32 trackCount();
    void append(MusicLibraryItem *i) override;
    void remove(int row);
    void remove(MusicLibraryItemSong *i);
    void removeAll(const QSet<QString> &fileNames);
    QMap<QString, Song> getSongs(const QSet<QString> &fileNames) const;
    Type itemType() const override { return Type_Album; }
    bool updateYear();
    const QString & id() const { return m_id; }
    const QString & albumId() const { return m_id.isEmpty() ? m_itemData : m_id; }
    const QString & sortString() const { return m_sortString.isEmpty() ? m_itemData : m_sortString; }
    bool hasSort() const { return !m_sortString.isEmpty(); }
    Song coverSong() const;

private:
    void setYear(const MusicLibraryItemSong *song);
    void updateStats();

private:
    quint16 m_year;
    quint16 m_yearOfTrack;
    quint16 m_yearOfDisc;
    quint16 m_numTracks;
    quint32 m_totalTime;
    QString m_sortString;
    QString m_id;
    mutable Song m_coverSong;
};

#endif
