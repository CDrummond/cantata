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

#ifndef ONLINE_DB_H
#define ONLINE_DB_H

#include "librarydb.h"

class OnlineDb : public LibraryDb
{
    Q_OBJECT
public:

    OnlineDb(const QString &serviceName, QObject *p=nullptr);
    ~OnlineDb() override;

    bool init(const QString &dbFile) override;
    void create();
    QString getCoverUrl(const QString &artistId, const QString &albumId);
    int getStats();

public Q_SLOTS:
    void startUpdate();
    void endUpdate();
    void storeCoverUrl(const QString &artistId, const QString &albumId, const QString &url);
    void insertStats(int numArtists);

private:
    void reset() override;

private:
    QSqlQuery *insertCoverQuery;
    QSqlQuery *getCoverQuery;
};

#endif
