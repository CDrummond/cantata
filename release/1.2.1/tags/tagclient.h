/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef TAG_CLIENT_H
#define TAG_CLIENT_H

#include "song.h"
#include <QImage>
#include <QString>

namespace Tags
{
    struct ReplayGain;
}

namespace TagClient
{
    extern void enableDebug();
    extern Song read(const QString &fileName);
    extern QImage readImage(const QString &fileName);
    extern QString readLyrics(const QString &fileName);
    extern int updateArtistAndTitle(const QString &fileName, const Song &song);
    extern int update(const QString &fileName, const Song &from, const Song &to, int id3Ver);
    extern Tags::ReplayGain readReplaygain(const QString &fileName);
    extern int updateReplaygain(const QString &fileName, const Tags::ReplayGain &rg);
    extern int embedImage(const QString &fileName, const QByteArray &cover);
}

#endif
