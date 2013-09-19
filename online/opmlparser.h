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

#ifndef OPMLPARSER_H
#define OPMLPARSER_H

#include <QUrl>
#include <QString>
#include <QList>

class QIODevice;

namespace OpmlParser
{

struct Podcast
{
    Podcast() { }
    QString name;
    QString description;
    QUrl url;
};

struct Category
{
    QString name;
    QList<Podcast> podcasts;
    QList<Category> categories;
    bool isValid() const { return !name.isEmpty(); }
};

Category parse(QIODevice *dev);

}

#endif
