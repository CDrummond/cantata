/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef QT_PLURAL_H
#define QT_PLURAL_H

#define QTP_TRACKS_STR(C) \
     QObject::tr("Tracks: %1").arg((C))

#define QTP_TRACKS_DURATION_STR(C, D) \
     QObject::tr("Tracks: %1 (%2)").arg((C)).arg((D))

#define QTP_ALBUMS_STR(C) \
     QObject::tr("Albums: %1").arg((C))

#define QTP_ARTISTS_STR(C) \
     QObject::tr("Artists: %1").arg((C))

#define QTP_STREAMS_STR(C) \
     QObject::tr("Streams: %1").arg((C))

#define QTP_ENTRIES_STR(C) \
     QObject::tr("Entries: %1").arg((C))

#define QTP_RULES_STR(C) \
     QObject::tr("Rules: %1").arg((C))

#endif
