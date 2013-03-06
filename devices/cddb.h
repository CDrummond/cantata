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

#ifndef CANTATA_CDDB_H
#define CANTATA_CDDB_H

#include <QObject>
#include <QString>
#include "song.h"
#include "config.h"

struct CddbAlbum {
    QString name;
    QString artist;
    QString genre;
    int year;
    QList<Song> tracks;
};

#ifdef CDDB_FOUND
class QThread;
typedef struct cddb_disc_s cddb_disc_t;

class Cddb : public QObject
{
    Q_OBJECT

public:
    Cddb(const QString &device);
    ~Cddb();

public Q_SLOTS:
    void readDisc();
    void lookup();

Q_SIGNALS:
    void error(const QString &error);
    void initialDetails(const CddbAlbum &);
    void matches(const QList<CddbAlbum> &);

private:
    QThread *thread;
    QString dev;
    cddb_disc_t *disc;
};
#endif

#endif

