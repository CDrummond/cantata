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

#ifndef SONG_H
#define SONG_H

#include <QString>
#include <QSet>
#include <QMetaType>

struct Song
{
    static const quint16 constNullKey;

    enum Type {
        Standard        = 0,
        MultipleArtists = 1,
        SingleTracks    = 2,
        Playlist        = 3,
        Stream          = 4,
        CantataStream   = 5,
        Cdda            = 6
    };

    qint32 id;
    QString file;
    QString album;
    QString artist;
    QString albumartist;
    QString title;
    QString genre;
    QString name;
//     quint32 pos;
    quint8 disc;
    mutable quint8 priority;
    quint16 time;
    quint16 track;
    quint16 year : 12;
    mutable Type type : 3;
    mutable bool guessed : 1;
    mutable qint32 size;

    // Only used in PlayQueue/PlayLists...
    quint16 key;

    static const QString constCddaProtocol;
    static void storeAlbumYear(const Song &s);
    static int albumYear(const Song &s);

    Song();
    Song(const Song &o) { *this=o; }
    Song & operator=(const Song &o);
    bool operator==(const Song &o) const;
    bool operator!=(const Song &o) const { return !(*this==o); }
    bool operator<(const Song &o) const;
    int compareTo(const Song &o) const;
    virtual ~Song() { }
    bool isEmpty() const;
    void guessTags();
    void revertGuessedTags();
    void fillEmptyFields();
    void setKey();
    virtual void clear();
    static QString formattedTime(quint32 seconds, bool zeroIsUnknown=false);
    QString format();
    QString entryName() const;
    QString artistSong() const;
    const QString & albumArtist() const { return albumartist.isEmpty() ? artist : albumartist; }
    QString displayTitle() const { return !albumartist.isEmpty() && albumartist!=artist ? artistSong() : title; }
    QString trackAndTitleStr(bool addArtist=false) const;
    void updateSize(const QString &dir) const;
    static bool isVariousArtists(const QString &str);
    bool isVariousArtists() const { return isVariousArtists(albumArtist()); }
    bool isUnknown() const;
    bool fixVariousArtists();
    bool revertVariousArtists();
    bool setAlbumArtist();
    static QString capitalize(const QString &s);
    bool capitalise();
    bool isStream() const { return Stream==type || CantataStream==type; }
    bool isCantataStream() const { return CantataStream==type; }
    bool isCdda() const { return Cdda==type; }
    QString albumKey() const { return albumArtist()+QChar(':')+album; }

    // We pass 'Song' around to cover requester. When we want the artist image, and not album image,
    // then we blank certain fields to indicate this!
    void setArtistImageRequest() {
        albumartist=albumArtist();
        album=artist=QString();
        size=track=0;
    }

    bool isArtistImageRequest() const { return album.isEmpty() && artist.isEmpty() && !albumartist.isEmpty() && 0==size && 0==track; }

    QString basicArtist() const;

    // podcast functions...
    bool hasbeenPlayed() const { return 0!=id; }
    void setPlayed(bool p) { id=p ? 1 : 0; }
    void setPodcastImage(const QString &i) { genre=i; }
    const QString & podcastImage() const { return genre; }

    // podcast/soundcloud functions...
    void setIsFromOnlineService(const QString &service);
    bool isFromOnlineService() const;
    const QString & onlineService() const { return album; }
};

Q_DECLARE_METATYPE(Song)

inline uint qHash(const Song &key)
{
    return qHash(key.albumArtist()+key.album+key.title+key.file);
}

#endif
