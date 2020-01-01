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

#include "onlinedb.h"
#include <QVariant>
#include <QSqlQuery>

static const QString subDir("online");

OnlineDb::OnlineDb(const QString &serviceName, QObject *p)
    : LibraryDb(p, serviceName)
    , insertCoverQuery(nullptr)
    , getCoverQuery(nullptr)
{
}

OnlineDb::~OnlineDb()
{
}

bool OnlineDb::init(const QString &dbFile)
{
    LibraryDb::init(dbFile);
    createTable("covers(artistId text, albumId text, url text)");
    createTable("stats(artists integer)");
    return true;
}

void OnlineDb::create()
{
    if (!db) {
        init(Utils::dataDir(subDir, true)+dbName+".sql");
    }
}

void OnlineDb::startUpdate()
{
    updateStarted(currentVersion+1);
    if (!db) {
        return;
    }
    QSqlQuery(*db).exec("delete from covers");
    QSqlQuery(*db).exec("drop index genre_idx");
}

void OnlineDb::endUpdate()
{
    updateFinished();
}

void OnlineDb::insertStats(int numArtists)
{
    if (!db) {
        return;
    }
    QSqlQuery(*db).exec("delete from stats");
    QSqlQuery(*db).exec("insert into stats(artists) values("+QString::number(numArtists)+")");
}

void OnlineDb::reset()
{
    delete insertCoverQuery;
    delete getCoverQuery;
    insertCoverQuery=nullptr;
    getCoverQuery=nullptr;
    LibraryDb::reset();
}

void OnlineDb::storeCoverUrl(const QString &artistId, const QString &albumId, const QString &url)
{
    if (!db) {
        return;
    }
    if (!insertCoverQuery) {
        insertCoverQuery=new QSqlQuery(*db);
        insertCoverQuery->prepare("insert into covers(artistId, albumId, url) "
                                  "values(:artistId, :albumId, :url)");
    }
    insertCoverQuery->bindValue(":artistId", artistId);
    insertCoverQuery->bindValue(":albumId", albumId);
    insertCoverQuery->bindValue(":url", url);
    insertCoverQuery->exec();
}

int OnlineDb::getStats()
{
    if (!db) {
        return -1;
    }
    QSqlQuery q(*db);
    q.exec("select artists from stats");
    if (q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}

QString OnlineDb::getCoverUrl(const QString &artistId, const QString &albumId)
{
    if (0!=currentVersion && db) {
        if (!getCoverQuery) {
            getCoverQuery=new QSqlQuery(*db);
            getCoverQuery->prepare("select url from covers where artistId=:artistId and albumId=:albumId limit 1;");
        }
        getCoverQuery->bindValue(":artistId", artistId);
        getCoverQuery->bindValue(":albumId", albumId);
        getCoverQuery->exec();
        while (getCoverQuery->next()) {
            return getCoverQuery->value(0).toString();
        }
    }
    return QString();
}

#include "moc_onlinedb.cpp"
