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

#ifndef SONG_H
#define SONG_H

#include <QString>
#include <QSet>
#include <QHash>
#include <QMetaType>
#include "config.h"
#include "support/utils.h"
#include "cuefile.h"

struct Song
{
    enum Constants {
        Null_Key = 0xFFFF,

        Rating_Step = 2,
        Rating_Max = 10,
        Rating_Requested = 0xFE,
        Rating_Null = 0xFF
    };

    static const int constNumGenres = 4;
    static const QLatin1Char constFieldSep;
    static const QString constSep;

    static const QSet<QString> & composerGenres();
    static void setComposerGenres(const QSet<QString> &g);

    enum ExtraTags {
        Composer             = 0x0001,
        Performer            = 0x0002,
        Comment              = 0x0004,
        MusicBrainzAlbumId   = 0x0008,
        Name                 = 0x0010,

        AlbumSort            = 0x0020,
        ArtistSort           = 0x0040,
        AlbumArtistSort      = 0x0080,

        // These are not real tags - but fields used elsewhere in the code...
        PodcastPublishedDate = 0x0100,
        LocalPath            = 0x0200, // Podcasts and HTTP files
        PodcastImage         = 0x0400,
        OnlineServiceName    = 0x0800,
        OnlineImageUrl       = 0x1000,
        OnlineImageCacheName = 0x2000,
        DeviceId             = 0x4000,
        DecodedPath          = 0x8000
    };

    enum Type {
        Standard        = 0,
        SingleTracks    = 1,
        Playlist        = 2,
        Stream          = 3,
        CantataStream   = 4,
        Cdda            = 5,
        OnlineSvrTrack  = 6,
        LocalFile       = 7
    };

    enum Blank {
        BlankTitle  = 0x01,
        BlankArtist = 0x02,
        BlankAlbum  = 0x04
    };

    QString file;
    QString album;
    QString artist;
    QString albumartist;
    QString title;
    QString genres[constNumGenres];
    QHash<quint16, QString> extra;
    quint16 extraFields;
    mutable quint8 priority;
    quint8 disc:5;
    quint8 blank:3; // Which field were blank, and Cantata set to Unknown
    quint16 time;
    quint16 track;
    quint16 origYear;
    quint16 year;
    mutable Type type : 7;
    mutable bool guessed : 1;
    qint32 id;
    qint32 size;
    mutable quint8 rating;
    uint lastModified;

    // Only used in PlayQueue/PlayLists...
    quint16 key;

    static const QString & unknown();
    static const QString & variousArtists();
    static const QString & singleTracks();
    static void initTranslations();
    static const QString constCddaProtocol;
    static const QString constMopidyLocal;
    static const QString constForkedDaapdLocal;
    static void storeAlbumYear(const Song &s);
    static int albumYear(const Song &s);
    static void sortViaType(QList<Song> &songs);
    static QString decodePath(const QString &file, bool cdda=false);
    static QString encodePath(const QString &file);
    static void clearKeyStore(int location);
    static QString displayAlbum(const QString &albumName, quint16 albumYear);
    static bool isComposerGenre(const QString &genre) { return composerGenres().contains(genre); }
    static QSet<QString> ignorePrefixes();
    static void setIgnorePrefixes(const QSet<QString> &prefixes);
    static QString sortString(const QString &str);
    static bool useOriginalYear();
    static void setUseOriginalYear(bool u);

    Song();
    Song(const Song &o) { *this=o; }
    Song & operator=(const Song &o);
    bool operator==(const Song &o) const;
    bool operator!=(const Song &o) const { return !(*this==o); }
    bool operator<(const Song &o) const;
    int compareTo(const Song &o) const;
    virtual ~Song() { }
    bool isEmpty() const;
    bool isDifferent(const Song &s) const { return file!=s.file || year!=s.year || artist!=s.artist || album!=s.album || title!=s.title || name()!=s.name(); }
    bool sameMetadata(const Song &o) const;
    void guessTags();
    void revertGuessedTags();
    void fillEmptyFields();
    quint16 setKey(int location);
    virtual void clear();
    void addGenre(const QString &g);
    quint16 displayYear() const;
    QString entryName() const;
    QString albumArtistOrComposer() const;
    QString trackArtistOrComposer() const;
    QString albumName() const;
    QString albumId() const;
    QString artistSong() const;
    const QString & albumArtist() const { return albumartist.isEmpty() ? artist : albumartist; }
    QString displayTitle() const { return !albumartist.isEmpty() && albumartist!=artist ? artistSong() : title; }
    QString trackAndTitleStr(bool showArtistIfDifferent=true) const;
    QString toolTip() const;
    QString displayGenre() const;
    const QString & firstGenre() const { return genres[0]; }
    int compareGenres(const Song &o) const;

    QString extraField(quint16 f) const { return hasExtraField(f) ? extra[f] : QString(); }
    bool hasExtraField(quint16 f) const { return extraFields&f; }
    void setExtraField(quint16 f, const QString &v);
    QString name() const { return extraField(Name); }
    void setName(const QString &v) { setExtraField(Name, v); }
    bool hasName() const { return hasExtraField(Name); }

    QString mbAlbumId() const { return extraField(MusicBrainzAlbumId); }
    void setMbAlbumId(const QString &v) { setExtraField(MusicBrainzAlbumId, v); }
    bool hasMbAlbumId() const { return hasExtraField(MusicBrainzAlbumId); }
    QString composer() const { return extraField(Composer); }
    void setComposer(const QString &v) { setExtraField(Composer, v); }
    bool hasComposer() const { return hasExtraField(Composer); }
    QString performer() const { return extraField(Performer); }
    void setPerformer(const QString &v) { setExtraField(Performer, v); }
    bool hasPerformer() const { return hasExtraField(Performer); }
    QString comment() const { return extraField(Comment); }
    void setComment(const QString &v) { setExtraField(Comment, v); }
    bool hasComment() const { return hasExtraField(Comment); }
    QString albumSort() const { return extraField(AlbumSort); }
    void setAlbumSort(const QString &v) { setExtraField(AlbumSort, v); }
    bool hasAlbumSort() const { return hasExtraField(AlbumSort); }
    QString artistSort() const { return extraField(ArtistSort); }
    void setArtistSort(const QString &v) { setExtraField(ArtistSort, v); }
    bool hasArtistSort() const { return hasExtraField(ArtistSort); }
    QString albumArtistSort() const { return extraField(AlbumArtistSort); }
    void setAlbumArtistSort(const QString &v) { setExtraField(AlbumArtistSort, v); }
    bool hasAlbumArtistSort() const { return hasExtraField(AlbumArtistSort); }

    QString artistSortString() const { return hasAlbumArtistSort() ? albumArtistSort() : hasArtistSort() ? artistSort() : QString(); }

    void clearExtra() { extra.clear(); }

    static bool isVariousArtists(const QString &str);
    bool isVariousArtists() const { return isVariousArtists(albumArtist()); }
    bool diffArtist() const;
    bool isUnknownAlbum() const;
    bool isInvalid() const;
    bool fixVariousArtists();
    bool revertVariousArtists();
    bool setAlbumArtist();
    static QString capitalize(const QString &s);
    bool capitalise();
    bool isLocalFile() const { return LocalFile==type; }
    bool isStream() const { return Stream==type || CantataStream==type; }
    bool isStandardStream() const { return Stream==type && !isDlnaStream(); }
    bool isDlnaStream() const { return Stream==type && !albumArtist().isEmpty() && !album.isEmpty() && track>0; }
    bool isNonMPD() const { return isStream() || OnlineSvrTrack==type || Cdda==type || (!file.isEmpty() && file.startsWith(Utils::constDirSep)); }
    bool hasProtocolOrIsAbsolute() const { return !file.isEmpty() && (file.startsWith(Utils::constDirSep) || (!file.startsWith(constForkedDaapdLocal) && file.contains(":/")));}
    bool isCantataStream() const { return CantataStream==type; }
    bool isCdda() const { return Cdda==type; }
    QString albumKey() const;
    bool isCueFile() const { return Playlist==type && file.endsWith(QLatin1String(".cue"), Qt::CaseInsensitive); }
    bool isFromCue() const { return CueFile::isCue(file); }
    QString basicArtist(bool orComposer=false) const;
    QString basicTitle() const;
    QString filePath(const QString &base=QString()) const;
    QString displayAlbum(bool useComp=true) const { return displayAlbum(useComp ? albumName() : album, displayYear()); }
    QString describe() const;
    // Main text + sub text for now-playing and notifications
    QString mainText() const;
    QString subText() const;
    bool useComposer() const;
    void populateSorts();
    void setFromSingleTracks();
//    QString basicDescription() const;

    //
    // The following sections contain various 'hacks' - where fields of Song are abused for other
    // purposes. This is to keep the overall size of Song lower, as its used all over the place...
    //

    // We pass 'Song' around to cover requester. When we want the artist image, and not album image,
    // then we blank certain fields to indicate this!
    void setArtistImageRequest() {
        album=QString();
        priority=0xFF;
        disc=0x1F;
        key=0xFFFF;
    }
    bool isArtistImageRequest() const { return 0x1F==disc && 0xFF==priority && 0xFFFF==key && album.isEmpty(); }

    void setComposerImageRequest() {
        album=QString();
        priority=0xFE;
        disc=0x1E;
        key=0xFEFE;
    }
    bool isComposerImageRequest() const { return 0x1E==disc && 0xFE==priority && 0xFEFE==key && album.isEmpty(); }

    // In Covers, the following is used to indicate that a specfic size is requested...
    void setSpecificSizeRequest(int sz) {
        size=track=id=sz;
        time=0xFFFF;
    }
    bool isSpecificSizeRequest() const { return size>4 && size<1024 && track==size && id==size && 0xFFFF==time; }

    // podcast functions...
    bool hasBeenPlayed() const { return 0!=id; }
    void setPlayed(bool p) { id=p ? 1 : 0; }
    void setPodcastImage(const QString &i) { setExtraField(PodcastImage, i); }
    QString podcastImage() const { return extraField(PodcastImage); }
    void setPodcastPublishedDate(const QString &pd) { setExtraField(PodcastPublishedDate, pd); }
    QString podcastPublishedDate() const { return extraField(PodcastPublishedDate); }
    QString localPath() const { return extraField(LocalPath); }
    void setLocalPath(const QString &l) { setExtraField(LocalPath, l); }

    QString decodedPath() const { return extraField(DecodedPath); }
    void setDecodedPath(const QString &v) { setExtraField(DecodedPath, v); }
    bool hasDecodedPath() const { return hasExtraField(DecodedPath); }

    // podcast/soundcloud functions...
    void setIsFromOnlineService(const QString &service) { setExtraField(OnlineServiceName, service); }
    bool isFromOnlineService() const { return hasExtraField(OnlineServiceName); }
    QString onlineService() const { return extraField(OnlineServiceName); }

    // device functions...
    void setIsFromDevice(const QString &id) { setExtraField(DeviceId, id); }
    bool isFromDevice() const { return hasExtraField(DeviceId); }
    QString deviceId() const { return extraField(DeviceId); }
};

Q_DECLARE_METATYPE(Song)

QDataStream & operator<<(QDataStream &stream, const Song &song);
QDataStream & operator>>(QDataStream &stream, Song &song);

inline uint qHash(const Song &key)
{
    return qHash(key.albumArtist()+key.album+key.title+key.file);
}

#endif
