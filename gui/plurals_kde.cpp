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

#include <KDE/KLocalizedString>
#include "plurals.h"

QString Plurals::tracks(int count)
{
    return i18np("1 Track", "%1 Tracks", count);
}

QString Plurals::tracksWithDuration(int count, const QString &duration)
{
    return i18np("1 Track (%2)", "%1 Tracks (%2)", count, duration);
}
    
QString Plurals::albums(int count)
{
    return i18np("1 Album", "%1 Albums", count);
}

QString Plurals::artists(int count)
{
    return i18np("1 Artist", "%1 Artists", count);
}

QString Plurals::streams(int count)
{
    return i18np("1 Stream", "%1 Streams", count);
}

QString Plurals::entries(int count)
{
    return i18np("1 Entry", "%1 Entries", count);
}

QString Plurals::rules(int count)
{
    return i18np("1 Rule", "%1 Rules", count);
}

QString Plurals::podcasts(int count)
{
    return i18np("1 Podcast", "%1 Podcasts", count);
}

QString Plurals::episodes(int count)
{
    return i18np("1 Episode", "%1 Episodes", count);
}
