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

#include <cmath>
#include "song.h"
#include "mpdparseutils.h"
#include "musiclibraryitemalbum.h"
#ifdef TAGLIB_FOUND
#include "httpserver.h"
#endif
#include "localize.h"
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

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
    if (SingleTracks==type) {
        int compare=artistSong().localeAwareCompare(artistSong());
        if (0!=compare) {
            return compare<0;
        }
    }

    int compare=albumArtist().localeAwareCompare(o.albumArtist());

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
    if (type!=o.type) {
        return type<o.type ? -1 : 1;
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
    return (artist.isEmpty() && album.isEmpty() && title.isEmpty()) || file.isEmpty();
}

void Song::guessTags()
{
    if (isEmpty() && !isStream()) {
        QStringList parts = file.split("/");
        if (3==parts.length()) {
            title=parts.at(2);
            album=parts.at(1);
            artist=parts.at(0);
        } else if (!parts.isEmpty()) {
            title=parts.at(parts.length()-1);
        }

        if (!title.isEmpty()) {
            int dot=title.lastIndexOf('.');
            if (dot==title.length()-4) {
                title=title.left(dot);
            }
            int space=title.indexOf(' ');
            if ( (1==space && title[space-1].isDigit()) ||
                 (2==space && title[space-2].isDigit() && title[space-1].isDigit()) ) {
                track=title.left(space).toInt();
                title=title.mid(space+1);

                while (!title.isEmpty() && (title[0]==' ' || title[0]=='-')) {
                    title=title.mid(1);
                }
            }
        }
    }
}

void Song::fillEmptyFields()
{
    QString unknown=i18n("Unknown");

    if (artist.isEmpty()) {
        artist = unknown;
    }
    if (album.isEmpty()) {
        album = unknown;
    }
    if (title.isEmpty()) {
        title = unknown;
    }
    if (genre.isEmpty()) {
        genre = unknown;
    }
}

void Song::setKey()
{
    static quint16 currentKey=0;
    static QMap<QString, quint16> keys;

    if (isStream() && !isCantataStream()) {
        key=0;
        return;
    }

    QString songKey(albumKey());
    QMap<QString, quint16>::ConstIterator it=keys.find(songKey);
    if (it!=keys.end()) {
        key=it.value();
    } else {
        currentKey++; // Key 0 is for streams, so we need to increment before setting...
        keys.insert(songKey, currentKey);
        key=currentKey;
    }
}

bool Song::isUnknown() const
{
    QString unknown=i18n("Unknown");

    return (artist.isEmpty() || artist==unknown) && (album.isEmpty() || album==unknown) && (title.isEmpty() || title==unknown);
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

QString Song::formattedTime(quint32 seconds)
{
    static const quint32 constHour=60*60;
    if (seconds>constHour) {
        return MPDParseUtils::formatDuration(seconds);
    }

    QString result(QString::number(floor(seconds / 60.0))+QChar(':'));
    if (seconds % 60 < 10) {
        result += "0";
    }
    return result+QString::number(seconds % 60);
}

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

    return i18nc("Song\nArtist\nAlbum", "%1\n%2\n%3").arg(title).arg(artist).arg(album);
}

QString Song::artistSong() const
{
    return artist+QLatin1String(" - ")+title;
}

QString Song::trackAndTitleStr(bool addArtist) const
{
    return (track>9 ? QString::number(track) : (QChar('0')+QString::number(track)))
           +QChar(' ')+(addArtist ? artistSong() : title);
}

void Song::updateSize(const QString &dir) const
{
    if (size<=0) {
        size=QFileInfo(dir+file).size();
    }
}

bool Song::isVariousArtists(const QString &str)
{
    return QLatin1String("Various Artists")==str || i18n("Various Artists")==str;
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
    if (!artist.isEmpty() && artist!=albumartist) {
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

bool Song::isCantataStream() const
{
    #ifdef TAGLIB_FOUND
    return !file.isEmpty() && file.startsWith("http") && HttpServer::self()->isOurs(file);
    #else
    return false;
    #endif
}
