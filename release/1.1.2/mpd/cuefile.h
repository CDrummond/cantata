/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CUEFILE_H
#define CUEFILE_H

#include <QList>
#include <QSet>
#include "song.h"

// This parser will try to detect the real encoding of a .cue file but there's
// a great chance it will fail so it's probably best to assume that the parser
// is UTF compatible only.
namespace CueFile
{
    extern bool isCue(const QString &str);
    extern QByteArray getLoadLine(const QString &str);
    extern bool parse(const QString &fileName, const QString &dir, QList<Song> &songList, QSet<QString> &files);
}

#endif  // CUEFILE_H
