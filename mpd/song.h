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

#ifndef SONG_H
#define SONG_H

#include <QString>

struct Song
{
    qint32 id;
    QString file;
    quint32 time;
    QString album;
    QString artist;
    QString albumartist;
    QString title;
    QString modifiedtitle;
    qint32 track;
    quint32 pos;
    quint32 disc;
    quint32 year;
    QString genre;
    QString name;

    Song();
    Song(const Song &o) { *this=o; }
    Song & operator=(const Song &o);
    bool operator==(const Song &o) const;
    virtual ~Song() { }
    bool isEmpty() const;
    void fillEmptyFields();
    virtual void clear();
    static QString formattedTime(const quint32 &seconds);
    QString format();
    QString entryName() const;
    const QString & albumArtist() const { return albumartist.isEmpty() ? artist : albumartist; }
    const QString & displayTitle() const { return modifiedtitle.isEmpty() ? title : modifiedtitle; }
};

#endif
