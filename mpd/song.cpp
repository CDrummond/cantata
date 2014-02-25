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

#include <cmath>
#include "config.h"
#include "song.h"
#include "musiclibraryitemalbum.h"
#include "localize.h"
#ifndef CANTATA_NO_SONG_TIME_FUNCTION
#include "utils.h"
#endif
#include <QMap>
#include <QStringList>
#include <QSet>
#include <QChar>
#include <QLatin1Char>
#include <QtAlgorithms>
#include <QUrl>

static const quint8 constOnlineDiscId=0xEE;

const QString Song::constCddaProtocol=QLatin1String("cantata-cdda:/");
const QString Song::constMopidyLocal=QLatin1String("local:track:");

static QString unknownStr;
static QString variousArtistsStr;
const QString & Song::unknown()
{
    return unknownStr;
}

const QString & Song::variousArtists()
{
    return variousArtistsStr;
}

void Song::initTranslations()
{
    unknownStr=i18n("Unknown");
    variousArtistsStr=i18n("Various Artists");
}

// When displaying albums, we use the 1st track's year as the year of the album.
// The map below stores the mapping from artist+album to year.
// This way the grouped view can find this quickly...
static QMap<QString, quint16> albumYears;

void Song::storeAlbumYear(const Song &s)
{
    albumYears.insert(s.albumKey(), s.year);
}

int Song::albumYear(const Song &s)
{
    QMap<QString, quint16>::ConstIterator it=albumYears.find(s.albumKey());
    return it==albumYears.end() ? s.year : it.value();
}

static int songType(const Song &s)
{
    static QStringList extensions=QStringList() << QLatin1String(".flac")
                                                << QLatin1String(".wav")
                                                << QLatin1String(".dff")
                                                << QLatin1String(".dsf")
                                                << QLatin1String(".aac")
                                                << QLatin1String(".m4a")
                                                << QLatin1String(".m4b")
                                                << QLatin1String(".m4p")
                                                << QLatin1String(".mp4")
                                                << QLatin1String(".ogg")
                                                << QLatin1String(".opus")
                                                << QLatin1String(".mp3")
                                                << QLatin1String(".wma");

    for (int i=0; i<extensions.count(); ++i) {
        if (s.file.endsWith(extensions.at(i), Qt::CaseInsensitive)) {
            return i;
        }
    }

    if (s.isCdda()) {
        return extensions.count()+2;
    }
    if (s.isStream()) {
        return extensions.count()+3;
    }
    return extensions.count()+1;
}

static bool songTypeSort(const Song &s1, const Song &s2)
{
    int t1=songType(s1);
    int t2=songType(s2);
    return t1<t2 || (t1==t2 && s1.id<s2.id);
}

void Song::sortViaType(QList<Song> &songs)
{
    qSort(songs.begin(), songs.end(), songTypeSort);
}

QString Song::decodePath(const QString &file)
{
    return file.startsWith(constMopidyLocal) ? QUrl::fromPercentEncoding(file.mid(constMopidyLocal.length()).toLatin1()) : file;
}

QString Song::encodePath(const QString &file)
{
    return constMopidyLocal+QString(QUrl::toPercentEncoding(file, "/"));
}

static bool useComposerIfSet=false;

bool Song::useComposer()
{
    return useComposerIfSet;
}

void Song::setUseComposer(bool u)
{
    useComposerIfSet=u;
}

const quint16 Song::constNullKey(0xFFFF);

Song::Song()
    : id(-1)
//       , pos(0)
      , disc(0)
      , priority(0)
      , time(0)
      , track(0)
      , year(0)
      , type(Standard)
      , guessed(false)
      , size(0)
      , key(constNullKey)
{
}

Song & Song::operator=(const Song &s)
{
    id = s.id;
    file = s.file;
    time = s.time;
    album = s.album;
    artist = s.artist;
    albumartist = s.albumartist;
    composer = s.composer;
    title = s.title;
    track = s.track;
//     pos = s.pos;
    disc = s.disc;
    priority = s.priority;
    year = s.year;
    genre = s.genre;
    name = s.name;
    size = s.size;
    key = s.key;
    type = s.type;
    guessed = s.guessed;
    return *this;
}

bool Song::operator==(const Song &o) const
{
    return 0==compareTo(o);
}

bool Song::operator<(const Song &o) const
{
    return compareTo(o)<0;
}

int Song::compareTo(const Song &o) const
{
    if (type!=o.type) {
        return type<o.type ? -1 : 1;
    }
    if (SingleTracks==type) {
        int compare=artistSong().localeAwareCompare(artistSong());
        if (0!=compare) {
            return compare<0;
        }
    }

    int compare=artistOrComposer().localeAwareCompare(o.artistOrComposer());

    if (0!=compare) {
        return compare;
    }
    compare=album.localeAwareCompare(o.album);
    if (0!=compare) {
        return compare;
    }
    if (disc!=o.disc) {
        return disc<o.disc ? -1 : 1;
    }
    if (track!=o.track) {
        return track<o.track ? -1 : 1;
    }
    if (year!=o.year) {
        return year<o.year ? -1 : 1;
    }
    compare=title.localeAwareCompare(o.title);
    if (0!=compare) {
        return compare;
    }
    compare=name.compare(o.name);
    if (0!=compare) {
        return compare;
    }
    compare=genre.compare(o.genre);
    if (0!=compare) {
        return compare;
    }
    if (time!=o.time) {
        return time<o.time ? -1 : 1;
    }
    return file.compare(o.file);
}

bool Song::isEmpty() const
{
    return (artist.isEmpty() && album.isEmpty() && title.isEmpty() && name.isEmpty()) || file.isEmpty();
}

void Song::guessTags()
{
    if (isEmpty() && !isStream()) {
        static const QLatin1String constAlbumArtistSep(" - ");
        guessed=true;
        QStringList parts = file.split("/", QString::SkipEmptyParts);
        if (3==parts.length()) {
            title=parts.at(2);
            album=parts.at(1);
            artist=parts.at(0);
        } if (2==parts.length() && parts.at(0).contains(constAlbumArtistSep)) {
            title=parts.at(1);
            QStringList albumArtistParts = parts.at(0).split(constAlbumArtistSep, QString::SkipEmptyParts);
            if (2==albumArtistParts.length()) {
                album=albumArtistParts.at(1);
                artist=albumArtistParts.at(0);
            }
        } else if (!parts.isEmpty()) {
            title=parts.at(parts.length()-1);
        }

        if (!title.isEmpty()) {
            int dot=title.lastIndexOf('.');
            if (dot==title.length()-4) {
                title=title.left(dot);
            }
            static const QSet<QChar> constSeparators=QSet<QChar>() << QLatin1Char(' ') << QLatin1Char('-') << QLatin1Char('_') << QLatin1Char('.');
            int separator=0;

            foreach (const QChar &sep, constSeparators) {
                separator=title.indexOf(sep);
                if (1==separator || 2==separator) {
                    break;
                }
            }

            if ( (1==separator && title[separator-1].isDigit()) ||
                 (2==separator && title[separator-2].isDigit() && title[separator-1].isDigit()) ) {
                if (0==track) {
                    track=title.left(separator).toInt();
                }
                title=title.mid(separator+1);

                while (!title.isEmpty() && constSeparators.contains(title[0])) {
                    title=title.mid(1);
                }
            }
        }
    }
}

void Song::revertGuessedTags()
{
    title=artist=album=unknownStr;
}

void Song::fillEmptyFields()
{
    if (artist.isEmpty()) {
        artist = unknownStr;
    }
    if (album.isEmpty()) {
        album = unknownStr;
    }
    if (title.isEmpty()) {
        title = unknownStr;
    }
    if (genre.isEmpty()) {
        genre = unknownStr;
    }
}

struct KeyStore
{
    KeyStore() : currentKey(0) { }
    quint16 currentKey;
    QMap<QString, quint16> keys;
};

static QMap<int, KeyStore> storeMap;

void Song::clearKeyStore(int location)
{
    storeMap.remove(location);
}

void Song::setKey(int location)
{
    if (isStandardStream()) {
        key=0;
        return;
    }

    KeyStore &store=storeMap[location];
    QString songKey(albumKey());
    QMap<QString, quint16>::ConstIterator it=store.keys.find(songKey);
    if (it!=store.keys.end()) {
        key=it.value();
    } else {
        store.currentKey++; // Key 0 is for streams, so we need to increment before setting...
        store.keys.insert(songKey, store.currentKey);
        key=store.currentKey;
    }
}

bool Song::isUnknown() const
{
    return (artist.isEmpty() || artist==unknownStr) && (album.isEmpty() || album==unknownStr) && (title.isEmpty() || title==unknownStr);
}

void Song::clear()
{
    id = -1;
    file.clear();
    time = 0;
    album.clear();
    artist.clear();
    title.clear();
    track = 0;
//     pos = 0;
    disc = 0;
    year = 0;
    genre.clear();
    name.clear();
    size = 0;
    type = Standard;
}

#ifndef CANTATA_NO_SONG_TIME_FUNCTION
QString Song::formattedTime(quint32 seconds, bool zeroIsUnknown)
{
    if (0==seconds && zeroIsUnknown) {
        return unknownStr;
    }

    static const quint32 constHour=60*60;
    if (seconds>constHour) {
        return Utils::formatDuration(seconds);
    }

    QString result(QString::number(floor(seconds / 60.0))+QChar(':'));
    if (seconds % 60 < 10) {
        result += "0";
    }
    return result+QString::number(seconds % 60);
}
#endif

/*
 * Genarate a string with song info.
 * Currently in this format:
 * artist - [album -][#.] song
 */
QString Song::format()
{
    QString s = artist + " - ";

    if (!album.isEmpty()) {
        s += album + " - ";
    }

    if (track != 0) {
        s += QString::number(track) + ". ";
    }

    s += title;

    return s;
}

QString Song::entryName() const
{
    if (title.isEmpty()) {
        return file;
    }

    return i18nc("Song\nArtist\nAlbum", "%1\n%2\n%3", title, artist, album);
}

QString Song::artistOrComposer() const
{
    return useComposerIfSet && !composer.isEmpty() ? composer : albumArtist();
}

QString Song::albumName() const
{
    return useComposerIfSet && !composer.isEmpty() && composer!=albumArtist()
            ? album+QLatin1String(" (")+albumArtist()+QLatin1Char(')')
            : album;
}

QString Song::artistSong() const
{
    return artist+QLatin1String(" - ")+title;
}

QString Song::trackAndTitleStr(bool addArtist) const
{
//    if (isFromOnlineService()) {
//        return (disc>0 && disc!=constOnlineDiscId ? (QString::number(disc)+QLatin1Char('.')) : QString())+
//               (track>0 ? (track>9 ? QString::number(track) : (QLatin1Char('0')+QString::number(track))) : QString())+
//               QLatin1Char(' ')+(addArtist ? artistSong() : title);
//    }
    return //(disc>0 ? (QString::number(disc)+QLatin1Char('.')) : QString())+
           (track>0 ? (track>9 ? QString::number(track) : (QLatin1Char('0')+QString::number(track))) : QString())+
           QLatin1Char(' ')+(addArtist ? artistSong() : title);
}

bool Song::isVariousArtists(const QString &str)
{
    return QLatin1String("Various Artists")==str || variousArtistsStr==str;
}

bool Song::fixVariousArtists()
{
    if (isVariousArtists()) {
        artist.replace(" - ", ", ");
        title=artistSong();
        artist=albumartist;
        return true;
    }
    return false;
}

bool Song::revertVariousArtists()
{
    if (artist==albumartist) { // Then real artist is embedded in track title...
        int sepPos=title.indexOf(QLatin1String(" - "));
        if (sepPos>0 && sepPos<title.length()-3) {
            artist=title.left(sepPos);
            title=title.mid(sepPos+3);
            return true;
        }
    }

    return false;
}

bool Song::setAlbumArtist()
{
    if (!artist.isEmpty() && albumartist.isEmpty()) {
        albumartist=artist;
        return true;
    }
    return false;
}

QString Song::capitalize(const QString &s)
{
    if (s.isEmpty()) {
        return s;
    }

    QStringList words = s.split(' ', QString::SkipEmptyParts);
    for (int i = 0; i < words.count(); i++) {
        QString word = words[i]; //.toLower();
        int j = 0;
        while ( ('('==word[j] || '['==word[j] || '{'==word[j]) && j < word.length()) {
            j++;
        }
        word[j] = word[j].toUpper();
        words[i] = word;
    }
    return words.join(" ");
}

bool Song::capitalise()
{
    QString origArtist=artist;
    QString origAlbumArtist=albumartist;
    QString origAlbum=album;
    QString origTitle=title;

    artist=capitalize(artist);
    albumartist=capitalize(albumartist);
    album=capitalize(album);
    title=capitalize(title);

    return artist!=origArtist || albumartist!=origAlbumArtist || album!=origAlbum || title!=origTitle;
}

QString Song::basicArtist() const
{
    if (!albumartist.isEmpty() && !artist.isEmpty() && albumartist.length()<artist.length() && artist.startsWith(albumartist)) {
        return albumartist;
    }

    QStringList toStrip=QStringList() << QLatin1String("ft. ") << QLatin1String("feat. ") << QLatin1String("featuring ") << QLatin1String("f. ");
    QStringList prefixes=QStringList() << QLatin1String(" ") << QLatin1String(" (") << QLatin1String(" [");

    foreach (const QString s, toStrip) {
        foreach (const QString p, prefixes) {
            int strip = artist.toLower().indexOf(p+s);
            if (-1!=strip) {
                return artist.mid(0, strip);
            }
        }
    }
    return artist;
}

QString Song::describe(bool withMarkup) const
{
    QString albumText=album.isEmpty() ? name : album;
    if (!album.isEmpty()) {
        quint16 y=Song::albumYear(*this);
        if (y>0) {
            albumText+=QString(" (%1)").arg(y);
        }
    }

    return withMarkup
            ?   artist.isEmpty()
                ?  i18nc("Song on Album", "<b>%1</b> on <b>%2</b>", title, albumText)
                :  i18nc("Song by Artist on Album", "<b>%1</b> by <b>%2</b> on <b>%3</b>", title, artist, albumText)
            : artist.isEmpty()
                ?  i18nc("Song on Album", "%1 on %2", title, album)
                :  i18nc("Song by Artist on Album", "%1 by %2 on %3", title, artist, albumText);
}

bool Song::isFromOnlineService() const
{
    return constOnlineDiscId==disc && (isCantataStream() || file.startsWith("http://")) && albumartist==album;
}

void Song::setIsFromOnlineService(const QString &service)
{
    disc=constOnlineDiscId;
    album=service;
    albumartist=service;
}

#ifdef ENABLE_EXTERNAL_TAGS
QDataStream & operator<<(QDataStream &stream, const Song &song)
{
    stream << song.id << song.file << song.album << song.artist << song.albumartist << song.composer << song.title
           << song.genre << song.name << song.disc << song.priority << song.time << song.track << (quint16)song.year
           << (quint16)song.type << (bool)song.guessed << song.size;
    return stream;
}

QDataStream & operator>>(QDataStream &stream, Song &song)
{
    quint16 type;
    quint16 year;
    bool guessed;
    stream >> song.id >> song.file >> song.album >> song.artist >> song.albumartist >> song.composer >> song.title
           >> song.genre >> song.name >> song.disc >> song.priority >> song.time >> song.track >> year
           >> type >> guessed >> song.size;
    song.type=(Song::Type)type;
    song.year=year;
    song.guessed=guessed;
    return stream;
}
#endif
