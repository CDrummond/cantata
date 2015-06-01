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

#include "mpdlibrarydb.h"
#include "support/globalstatic.h"
#include "mpd-interface/mpdconnection.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

#define DBUG if (LibraryDb::debugEnabled()) qWarning() << metaObject()->className() << __FUNCTION__

GLOBAL_STATIC(MpdLibraryDb, instance)

MpdLibraryDb::MpdLibraryDb()
    : coverQuery(0)
{
    connect(MPDConnection::self(), SIGNAL(updatingLibrary(time_t)), this, SLOT(updateStarted(time_t)));
    connect(MPDConnection::self(), SIGNAL(librarySongs(QList<Song>*)), this, SLOT(insertSongs(QList<Song>*)));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), this, SLOT(updateFinished()));
    connect(MPDConnection::self(), SIGNAL(statsUpdated(MPDStatsValues)), this, SLOT(statsUpdated(MPDStatsValues)));
    connect(this, SIGNAL(loadLibrary()), MPDConnection::self(), SLOT(loadLibrary()));
    DBUG;
}

MpdLibraryDb::~MpdLibraryDb()
{
//    delete coverQuery;
}

Song MpdLibraryDb::getCoverSong(const QString &artistId, const QString &albumId)
{
    DBUG << artistId << albumId;
    if (0!=currentVersion) {
        if (!coverQuery) {
            coverQuery=new QSqlQuery(*db);
            coverQuery->prepare("select * from songs where artistId=:artistId and albumId=:albumId limit 1;");
        }
        coverQuery->bindValue(":artistId", artistId);
        coverQuery->bindValue(":albumId", albumId);
        coverQuery->exec();
        DBUG << "coverquery" << coverQuery->executedQuery() << coverQuery->size();
        while (coverQuery->next()) {
            return getSong(coverQuery);
        }
    }
    return Song();
}

void MpdLibraryDb::statsUpdated(const MPDStatsValues &stats)
{
    if (stats.dbUpdate>currentVersion) {
        emit loadLibrary();
    }
}
