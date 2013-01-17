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

#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QtCore/QDateTime>
#include <QtCore/QString>

struct Playlist
{
    Playlist() { }
    Playlist(const QString &n) : name(n) { }
    Playlist(const Playlist &o) { *this=o; }

    bool operator==(const Playlist &o) const
    {
        return name==o.name;
    }

    Playlist & operator=(const Playlist &o)
    {
        name=o.name;
        lastModified=o.lastModified;
        return *this;
    }

    virtual ~Playlist() { }

    QString name;
    QDateTime lastModified;
};

#endif
