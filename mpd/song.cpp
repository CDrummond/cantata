/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef ENABLE_KDE_SUPPORT
#include <KLocale>
#else
#include <QtCore/QObject>
#endif

Song::Song()
    : id(-1),
      time(0),
      track(0),
      pos(0),
      disc(0),
      year(0)
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
    return *this;
}

bool Song::operator==(const Song &o) const
{
    return id==o.id;
}

bool Song::isEmpty() const
{
    return
        (
            (artist.isEmpty() && album.isEmpty() && title.isEmpty())
            || file.isEmpty()
        );
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
}

QString Song::formattedTime(const quint32 &seconds)
{
    QString result;

    result += QString::number(floor(seconds / 60.0));
    result += ":";
    if (seconds % 60 < 10)
        result += "0";
    result += QString::number(seconds % 60);

    return result;
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

