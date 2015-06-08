/*
 * Cantata
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

#ifndef LIBRARY_DB_H
#define LIBRARY_DB_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QElapsedTimer>
#include "mpd-interface/song.h"
#include <time.h>

class QSqlDatabase;
class QSqlQuery;
class QSettings;

class LibraryDb : public QObject
{
    Q_OBJECT

public:
    static void enableDebug() { dbgEnabled=true; }
    static bool debugEnabled() { return dbgEnabled; }

    static const QLatin1String constFileExt;
    static const QLatin1String constNullGenre;
    static const QLatin1String constArtistAlbumsSortYear;
    static const QLatin1String constArtistAlbumsSortName;
    static const QLatin1String constAlbumsSortAlArYr;
    static const QLatin1String constAlbumsSortAlYrAr;
    static const QLatin1String constAlbumsSortArAlYr;
    static const QLatin1String constAlbumsSortArYrAl;
    static const QLatin1String constAlbumsSortYrAlAr;
    static const QLatin1String constAlbumsSortYrArAl;

    struct Genre
    {
        Genre(const QString &n=QString(), int ac=0)
            : name(n), artistCount(ac) { }
        QString name;
        int artistCount;

        bool operator<(const Genre &o) const
        {
            if (constNullGenre==name) {
                return constNullGenre!=o.name;
            }
            return name.localeAwareCompare(o.name)<0;
        }
    };

    struct Artist
    {
        Artist(const QString &n=QString(), const QString &s=QString(), int ac=0)
            : name(n), sort(s), albumCount(ac) { }
        QString name;
        QString sort;
        int albumCount;

        bool operator<(const Artist &o) const
        {
            const QString &field=sort.isEmpty() ? name : sort;
            const QString &ofield=o.sort.isEmpty() ? o.name : o.sort;

            return field.localeAwareCompare(ofield)<0;
        }
    };

    struct Album
    {
        Album(const QString &n=QString(), const QString &i=QString(), const QString &s=QString(),
              const QString &a=QString(), const QString &as=QString(),
              int y=0, int tc=0, int d=0)
            : name(n), id(i), sort(s), artist(a), artistSort(as), year(y), trackCount(tc), duration(d) { }
        QString name;
        QString id;
        QString sort;
        QString artist;
        QString artistSort;
        int year;
        int trackCount;
        int duration;
    };

    LibraryDb(QObject *p, const QString &name);
    ~LibraryDb();

    void clear();
    void erase();
    bool init(const QString &dbFile);
    void insertSong(const Song &s);
    QList<Genre> getGenres();
    QList<Artist> getArtists(const QString &genre=QString());
    QList<Album> getAlbums(const QString &artistId=QString(), const QString &genre=QString(), const QString &sort=QString());
    QList<Song> getTracks(const QString &artistId, const QString &albumId, const QString &genre=QString(), const QString &sort=QString());
    QList<Album> getAlbumsWithArtist(const QString &artist);
    #ifndef CANTATA_WEB
    bool setFilter(const QString &f);
    const QString & getFilter() const { return filter; }
    #endif

Q_SIGNALS:
    void libraryUpdated();

protected Q_SLOTS:
    void updateStarted(time_t ver);
    void insertSongs(QList<Song> *songs);
    void updateFinished();

protected:
    bool createTable(const QString &q);
    static Song getSong(const QSqlQuery *query);

protected:
    virtual void reset();

protected:
    static bool dbgEnabled;

    QString dbName;
    QString dbFileName;
    time_t currentVersion;
    time_t newVersion;
    QSqlDatabase *db;
    QSqlQuery *insertSongQuery;
    QElapsedTimer timer;
    QString filter;
};

#endif
