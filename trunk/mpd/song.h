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

#ifndef SONG_H
#define SONG_H

#include <QString>
#include <QSet>
#include <QMetaType>
#include "config.h"
#include "utils.h"

struct Song
{
    static const quint16 constNullKey;

    static bool useComposer();
    static void setUseComposer(bool u);

    enum Type {
        Standard        = 0,
        MultipleArtists = 1,
        SingleTracks    = 2,
        Playlist        = 3,
        Stream          = 4,
        CantataStream   = 5,
        Cdda            = 6,
        OnlineSvrTrack  = 7
    };

    qint32 id;
    QString file;
    QString album;
    QString artist;
    QString albumartist;
    QString composer;
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
    qint32 size;

    // Only used in PlayQueue/PlayLists...
    quint16 key;

    static const QString constCddaProtocol;
    static const QString constMopidyLocal;
    static void storeAlbumYear(const Song &s);
    static int albumYear(const Song &s);
    static void sortViaType(QList<Song> &songs);
    static QString decodePath(const QString &file);
    static QString encodePath(const QString &file);
    static void clearKeyStore(int location);

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
    void setKey(int location);
    virtual void clear();
    #ifndef CANTATA_NO_SONG_TIME_FUNCTION
    static QString formattedTime(quint32 seconds, bool zeroIsUnknown=false);
    #endif
    QString format();
    QString entryName() const;
    QString artistOrComposer() const;
    QString albumName() const;
    QString artistSong() const;
    const QString & albumArtist() const { return albumartist.isEmpty() ? artist : albumartist; }
    QString displayTitle() const { return !albumartist.isEmpty() && albumartist!=artist ? artistSong() : title; }
    QString trackAndTitleStr(bool addArtist=false) const;
    static bool isVariousArtists(const QString &str);
    bool isVariousArtists() const { return isVariousArtists(albumArtist()); }
    bool isUnknown() const;
    bool fixVariousArtists();
    bool revertVariousArtists();
    bool setAlbumArtist();
    static QString capitalize(const QString &s);
    bool capitalise();
    bool isStream() const { return Stream==type || CantataStream==type; }
    bool isNonMPD() const { return isStream() || OnlineSvrTrack==type || Cdda==type || (!file.isEmpty() && file.startsWith(Utils::constDirSep)); }
    bool isCantataStream() const { return CantataStream==type; }
    bool isCdda() const { return Cdda==type; }
    QString albumKey() const { return albumArtist()+QLatin1Char(':')+album+QLatin1Char(':')+QString::number(disc); }
    bool isCueFile() const { return Playlist==type && file.endsWith(QLatin1String(".cue"), Qt::CaseInsensitive); }
    QString basicArtist() const;
    QString filePath() const { return decodePath(file); }

    // We pass 'Song' around to cover requester. When we want the artist image, and not album image,
    // then we blank certain fields to indicate this!
    void setArtistImageRequest() {
        albumartist=albumArtist();
        album=artist=QString();
        size=track=0;
    }
    bool isArtistImageRequest() const { return album.isEmpty() && artist.isEmpty() && !albumartist.isEmpty() && 0==size && 0==track; }

    //
    // The following sections contain various 'hacks' - where fields of Song are abused for other
    // purposes. This is to kee the overall size of Song lower, as its used all over the place...
    //

    // podcast functions...
    bool hasBeenPlayed() const { return 0!=id; }
    void setPlayed(bool p) { id=p ? 1 : 0; }
    void setPodcastImage(const QString &i) { genre=i; }
    const QString & podcastImage() const { return genre; }
    void setPodcastPublishedDate(const QString &pd) { composer=pd; }
    const QString & podcastPublishedDate() const { return composer; }
    const QString & podcastLocalPath() const { return name; }
    void setPodcastLocalPath(const QString &l) { name=l; }

    // podcast/soundcloud functions...
    void setIsFromOnlineService(const QString &service);
    bool isFromOnlineService() const;
    const QString & onlineService() const { return album; }
};

Q_DECLARE_METATYPE(Song)

#ifdef ENABLE_EXTERNAL_TAGS
QDataStream & operator<<(QDataStream &stream, const Song &song);
QDataStream & operator>>(QDataStream &stream, Song &song);
#endif

inline uint qHash(const Song &key)
{
    return qHash(key.albumArtist()+key.album+key.title+key.file);
}

#endif
