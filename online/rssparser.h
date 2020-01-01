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

#ifndef RSSPARSER_H
#define RSSPARSER_H

#include <QUrl>
#include <QString>
#include <QList>
#include <QList>
#include <QDateTime>

class QIODevice;

namespace RssParser
{

struct Episode
{
    Episode() : duration(0), video(false) { }
    QString name;
    QString description;
    QDateTime publicationDate;
    unsigned int duration;
    QUrl url;
    bool video;
};

struct Channel
{
    Channel() : video(false) { }
    QString name;
    QUrl image;
    QList<Episode> episodes;
    QString description;
    bool video;
    bool isValid() const { return !name.isEmpty(); }
};

Channel parse(QIODevice *dev, bool getEpisodes=true, bool getDescription=false);

}

#endif
