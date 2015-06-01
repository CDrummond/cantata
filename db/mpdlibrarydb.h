/*
 * Cantata Web
 *
 * Copyright (c) 2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef MPD_LIBRARY_DB_H
#define MPD_LIBRARY_DB_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QElapsedTimer>
#include "librarydb.h"
#include <time.h>

struct MPDStatsValues;
class QSqlDatabase;
class QSqlQuery;
class QSettings;

class MpdLibraryDb : public LibraryDb
{
    Q_OBJECT

public:
    static MpdLibraryDb * self();

    MpdLibraryDb();
    ~MpdLibraryDb();

    Song getCoverSong(const QString &artistId, const QString &albumId);

Q_SIGNALS:
    void loadLibrary();

private Q_SLOTS:
    void statsUpdated(const MPDStatsValues &stats);

private:
    QSqlQuery *coverQuery;
};

#endif
