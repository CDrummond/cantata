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

#ifndef QT_PLURAL_H
#define QT_PLURAL_H

#ifndef ENABLE_KDE_SUPPORT

#define QTP_TRACKS_STR(C) \
    (1==(C) ? QObject::tr("1 Track", "Singular") : \
     QObject::tr("%1 Tracks", "Plural (N!=1)").arg((C)))

#define QTP_TRACKS_DURATION_STR(C, D) \
    (1==(C) ? QObject::tr("1 Track (%1)", "Singular").arg(D) : \
     QObject::tr("%1 Tracks (%2)", "Plural (N!=1)").arg((C)).arg(D))

#define QTP_ALBUMS_STR(C) \
    (1==(C) ? QObject::tr("1 Album", "Singular") : \
     QObject::tr("%1 Albums", "Plural (N!=1)").arg((C)))

#define QTP_ARTISTS_STR(C) \
    (1==(C) ? QObject::tr("1 Artist", "Singular") : \
     QObject::tr("%1 Artists", "Plural (N!=1)").arg((C)))

#define QTP_STREAMS_STR(C) \
    (1==(C) ? QObject::tr("1 Stream", "Singular") : \
     QObject::tr("%1 Streams", "Plural (N!=1)").arg((C)))

#define QTP_ENTRIES_STR(C) \
    (1==(C) ? QObject::tr("1 Entry", "Singular") : \
     QObject::tr("%1 Entries", "Plural (N!=1)").arg((C)))

#define QTP_RULES_STR(C) \
    (1==(C) ? QObject::tr("1 Rule", "Singular") : \
     QObject::tr("%1 Rules", "Plural (N!=1)").arg((C)))

#define QTP_PODCASTS_STR(C) \
    (1==(C) ? QObject::tr("1 Podcast", "Singular") : \
     QObject::tr("%1 Podcasts", "Plural (N!=1)").arg((C)))

#define QTP_EPISODES_STR(C) \
    (1==(C) ? QObject::tr("1 Episode", "Singular") : \
     QObject::tr("%1 Episodes", "Plural (N!=1)").arg((C)))

#endif

#endif

