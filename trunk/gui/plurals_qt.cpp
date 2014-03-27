/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QObject>
#include "plurals.h"

QString Plurals::tracks(int count)
{
    return 1==count ? QObject::tr("1 Track", "Singular") : QObject::tr("%1 Tracks", "Plural (N!=1)").arg(count);
}

QString Plurals::tracksWithDuration(int count, const QString &duration)
{
    return 1==count ? QObject::tr("1 Track (%1)", "Singular").arg(duration)
                    : QObject::tr("%1 Tracks (%2)", "Plural (N!=1)").arg(count).arg(duration);
}
    
QString Plurals::albums(int count)
{
    return 1==count ? QObject::tr("1 Album", "Singular") : QObject::tr("%1 Albums", "Plural (N!=1)").arg(count);
}

QString Plurals::artists(int count)
{
    return 1==count ? QObject::tr("1 Artist", "Singular") : QObject::tr("%1 Artists", "Plural (N!=1)").arg(count);
}

QString Plurals::streams(int count)
{
    return 1==count ? QObject::tr("1 Stream", "Singular") : QObject::tr("%1 Streams", "Plural (N!=1)").arg(count);
}

QString Plurals::entries(int count)
{
    return 1==count ? QObject::tr("1 Entry", "Singular") : QObject::tr("%1 Entries", "Plural (N!=1)").arg(count);
}

QString Plurals::rules(int count)
{
    return 1==count ? QObject::tr("1 Rule", "Singular") : QObject::tr("%1 Rules", "Plural (N!=1)").arg(count);
}

QString Plurals::podcasts(int count)
{
    return 1==count ? QObject::tr("1 Podcast", "Singular") : QObject::tr("%1 Podcasts", "Plural (N!=1)").arg(count);
}

QString Plurals::episodes(int count)
{
    return 1==count ? QObject::tr("1 Episode", "Singular") : QObject::tr("%1 Episodes", "Plural (N!=1)").arg(count);
}
