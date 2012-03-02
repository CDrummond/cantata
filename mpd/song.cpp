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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#else
#include <QtCore/QObject>
#endif
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>

const quint16 Song::constNullKey(0xFFFF);

Song::Song()
    : id(-1),
      time(0),
      track(0),
      pos(0),
      disc(0),
      year(0),
      size(0),
      key(constNullKey)
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
    modifiedtitle = s.modifiedtitle;
    track = s.track;
    pos = s.pos;
    disc = s.disc;
    year = s.year;
    genre = s.genre;
    name = s.name;
    size = s.size;
    key = s.key;
    return *this;
}

bool Song::operator==(const Song &o) const
{
    // When transfferring off an MPT device we will not have AlbumArtist, as libMTP does not handle this.
    // But, MPD does, so if the labum artist is different - then treat as a new file.
    // TODO: ALBUMARTIST: This check can be removed when libMTP supports album artist.
    return file == o.file && albumartist==o.albumartist;
}

bool Song::operator<(const Song &o) const
{
    bool sortDateBeforeAlbum=MusicLibraryItemAlbum::showDate();
    int compare=albumArtist().localeAwareCompare(o.albumArtist());

    if (0!=compare) {
        return compare<0;
    }
    if (sortDateBeforeAlbum && year!=o.year) {
        return year<o.year;
    }
    compare=album.localeAwareCompare(o.album);
    if (0!=compare) {
        return compare<0;
    }
    if (!sortDateBeforeAlbum && year!=o.year) {
        return year<o.year;
    }
    if (track!=o.track) {
        return track<o.track;
    }
    compare=file.compare(o.file);
    if (0!=compare) {
        return compare<0;
    }
    return time<o.time;
}

bool Song::isEmpty() const
{
    return (artist.isEmpty() && album.isEmpty() && title.isEmpty()) || file.isEmpty();
}

void Song::fillEmptyFields()
{
    #ifdef ENABLE_KDE_SUPPORT
    if (artist.isEmpty()) {
        artist = i18n("Unknown");
    }

    if (album.isEmpty()) {
        album = i18n("Unknown");
    }

    if (title.isEmpty()) {
        title = i18n("Unknown");
    }
    #else
    if (artist.isEmpty()) {
        artist = QObject::tr("Unknown");
    }

    if (album.isEmpty()) {
        album = QObject::tr("Unknown");
    }

    if (title.isEmpty()) {
        title = QObject::tr("Unknown");
    }
    #endif
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
    pos = 0;
    disc = 0;
    year = 0;
    genre.clear();
    name.clear();
    size = 0;
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
    QString text=title.isEmpty() ? file : title;

    if (!title.isEmpty()) {
        text=artist+QLatin1String(" - ")+text;
    }
    return text;
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
    return QLatin1String("Various Artists")==str ||
            #ifdef ENABLE_KDE_SUPPORT
            i18n("Various Artists")==str
            #else
            QObject::tr("Various Artists")==str
            #endif
            ;
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
