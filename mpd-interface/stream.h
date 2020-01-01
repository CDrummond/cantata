/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef STREAM_H
#define STREAM_H

#include <QString>

struct Stream
{
    Stream() { }
    Stream(const QString &u, const QString &n) : url(u), name(n) { }
    Stream(const Stream &o) { *this=o; }

    bool operator==(const Stream &o) const
    {
        return url==o.url;
    }

    Stream & operator=(const Stream &o)
    {
        url=o.url;
        name=o.name;
        return *this;
    }

    virtual ~Stream() { }

    QString url;
    QString name;
};

#endif

