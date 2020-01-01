/*
 * Cantata
 *
 * Copyright (c) 2017-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/utils.h"
#include "mpd-interface/mpdconnection.h"
#include "gui/settings.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

#define DBUG if (LibraryDb::debugEnabled()) qWarning() << metaObject()->className() << __FUNCTION__

static const QLatin1String constDirName("library");
static QString databaseName(const MPDConnectionDetails &details)
{
    QString fileName=(!details.isLocal() ? details.hostname+'_'+QString::number(details.port) : details.hostname)+LibraryDb::constFileExt;
    fileName.replace('/', '_');
    fileName.replace('~', '_');
    return Utils::dataDir(constDirName, true)+fileName;
}

void MpdLibraryDb::removeUnusedDbs()
{
    QSet<QString> existing;
    QList<MPDConnectionDetails> connections=Settings::self()->allConnections();
    QString dirPath=Utils::dataDir(constDirName, false);
    if (dirPath.isEmpty()) {
        return;
    }

    for (const MPDConnectionDetails &conn: connections) {
        existing.insert(databaseName(conn).mid(dirPath.length()));
    }

    QFileInfoList files=QDir(dirPath).entryInfoList(QStringList() << "*"+LibraryDb::constFileExt, QDir::Files);
    for (const QFileInfo &file: files) {
        if (!existing.contains(file.fileName())) {
            QFile::remove(file.absoluteFilePath());
        }
    }
}

MpdLibraryDb::MpdLibraryDb(QObject *p)
    : LibraryDb(p, "MPD")
    , loading(false)
    , coverQuery(nullptr)
    , albumIdOnlyCoverQuery(nullptr)
    , artistImageQuery(nullptr)
{
    connect(MPDConnection::self(), SIGNAL(updatingLibrary(time_t)), this, SLOT(updateStarted(time_t)));
    connect(MPDConnection::self(), SIGNAL(librarySongs(QList<Song>*)), this, SLOT(insertSongs(QList<Song>*)));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), this, SLOT(updateFinished()));
    connect(MPDConnection::self(), SIGNAL(statsUpdated(MPDStatsValues)), this, SLOT(statsUpdated(MPDStatsValues)));
    connect(this, SIGNAL(loadLibrary()), MPDConnection::self(), SLOT(loadLibrary()));
    connect(MPDConnection::self(), SIGNAL(connectionChanged(MPDConnectionDetails)), this, SLOT(connectionChanged(MPDConnectionDetails)));
    DBUG;
}

MpdLibraryDb::~MpdLibraryDb()
{
}

Song MpdLibraryDb::getCoverSong(const QString &artistId, const QString &albumId)
{
    DBUG << artistId << albumId;
    if (0!=currentVersion && db) {
        QSqlQuery *query=nullptr;
        if (albumId.isEmpty()) {
            if (!artistImageQuery) {
                artistImageQuery=new QSqlQuery(*db);
                artistImageQuery->prepare("select * from songs where artistId=:artistId limit 1;");
            }
            query=artistImageQuery;
            query->bindValue(":artistId", artistId);
        } else if (artistId.isEmpty()) {
            if (!albumIdOnlyCoverQuery) {
                albumIdOnlyCoverQuery=new QSqlQuery(*db);
                albumIdOnlyCoverQuery->prepare("select * from songs where albumId=:albumId limit 1;");
            }
            query=albumIdOnlyCoverQuery;
            query->bindValue(":albumId", albumId);
        } else {
            if (!coverQuery) {
                coverQuery=new QSqlQuery(*db);
                coverQuery->prepare("select * from songs where artistId=:artistId and albumId=:albumId limit 1;");
            }
            query=coverQuery;
            query->bindValue(":albumId", albumId);
            query->bindValue(":artistId", artistId);
        }
        query->exec();
        DBUG << "coverquery" << query->executedQuery() << query->size();
        while (query->next()) {
            return getSong(*query);
        }
    }
    return Song();
}

void MpdLibraryDb::connectionChanged(const MPDConnectionDetails &details)
{
    QString dbFile=databaseName(details);
    DBUG << dbFileName << dbFile;
    if (dbFile!=dbFileName) {
        init(dbFile);
    } else {
        emit libraryUpdated();
    }
}

void MpdLibraryDb::reset()
{
    delete coverQuery;
    delete artistImageQuery;
    delete albumIdOnlyCoverQuery;
    coverQuery=nullptr;
    artistImageQuery=nullptr;
    albumIdOnlyCoverQuery=nullptr;
    LibraryDb::reset();
}

void MpdLibraryDb::updateFinished()
{
    loading=false;
    LibraryDb::updateFinished();
}

void MpdLibraryDb::statsUpdated(const MPDStatsValues &stats)
{
    if (!loading && stats.dbUpdate>currentVersion) {
        DBUG << stats.dbUpdate << currentVersion;
        loading=true;
        emit loadLibrary();
    }
}

#include "moc_mpdlibrarydb.cpp"
