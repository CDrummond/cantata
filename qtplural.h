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
    (0==(C) ? QObject::tr("0 Tracks") : \
     1==(C) ? QObject::tr("1 Track") : \
     QObject::tr("%1 Tracks").arg((C)))

#define QTP_TRACKS_DURATION_STR(C, D) \
    (0==(C) ? QObject::tr("0 Tracks (%1)").arg(D) : \
     1==(C) ? QObject::tr("1 Track (%1)").arg(D) : \
     QObject::tr("%1 Tracks (%2)").arg((C)).arg(D))

#define QTP_ALBUMS_STR(C) \
    (0==(C) ? QObject::tr("0 Albums") : \
     1==(C) ? QObject::tr("1 Album") : \
     QObject::tr("%1 Albums").arg((C)))

#define QTP_ARTISTS_STR(C) \
    (0==(C) ? QObject::tr("0 Artists") : \
     1==(C) ? QObject::tr("1 Artist") : \
     QObject::tr("%1 Artists").arg((C)))

#define QTP_STREAMS_STR(C) \
    (0==(C) ? QObject::tr("0 Streams") : \
     1==(C) ? QObject::tr("1 Stream") : \
     QObject::tr("%1 Streams").arg((C)))

#define QTP_ENTRIES_STR(C) \
    (0==(C) ? QObject::tr("0 Entries") : \
     1==(C) ? QObject::tr("1 Entry") : \
     QObject::tr("%1 Entries").arg((C)))

#define QTP_RULES_STR(C) \
    (0==(C) ? QObject::tr("0 Rules") : \
     1==(C) ? QObject::tr("1 Rule") : \
     QObject::tr("%1 Rules").arg((C)))

#endif
