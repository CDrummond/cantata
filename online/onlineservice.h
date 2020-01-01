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

#ifndef ONLINE_SERVICE_H
#define ONLINE_SERVICE_H

#include "support/icon.h"

class QString;
struct Song;

class OnlineService
{
public:
    virtual ~OnlineService() { }

    static QString iconPath(const QString &srv);
    static bool showLogoAsCover(const Song &s);
    static bool isPodcasts(const QString &srv);
    static Song & encode(Song &s);
    static bool decode(Song &song);

    virtual QString name() const =0;
    virtual QString title() const =0;
    virtual QString descr() const =0;
    const QIcon & icon() const { return icn; }

protected:
    static void useCovers(const QString &name, bool onlyIfCache=false);

protected:
    QIcon icn;
};

#endif
