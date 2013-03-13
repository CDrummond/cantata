/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
  Copyright (C) 2005 Richard Lärkäng <nouseforaname@home.se>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#ifndef MUSICBRAINZ_H
#define MUSICBRAINZ_H

#include "cddb.h"
#include <QString>

class QThread;

class MusicBrainz : public QObject
{
    Q_OBJECT
public:
    MusicBrainz(const QString &device);
    ~MusicBrainz();

public Q_SLOTS:
    void readDisc();
    void lookup();

Q_SIGNALS:
    void error(const QString &error);
    void initialDetails(const CdAlbum &);
    void matches(const QList<CdAlbum> &);

private:
    QThread *thread;
    QString dev;
    QString discId;
    CdAlbum initial;
};
#endif // MUSICBRAINZ_H
