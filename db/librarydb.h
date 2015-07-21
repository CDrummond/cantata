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

    enum AlbumSort {
        AS_Album,
        AS_Artist,
        AS_Year,
        AS_Modified,

        AS_Count
    };

    static AlbumSort toAlbumSort(const QString &str);
    static QString albumSortStr(AlbumSort m);

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
              int y=0, int tc=0, int d=0, int lm=0)
            : name(n), id(i), sort(s), artist(a), artistSort(as), year(y), trackCount(tc), duration(d), lastModified(lm) { }
        QString name;
        QString id;
        QString sort;
        QString artist;
        QString artistSort;
        int year;
        int trackCount;
        int duration;
        int lastModified;
    };

    LibraryDb(QObject *p, const QString &name);
    ~LibraryDb();

    void clear();
    void erase();
    bool init(const QString &dbFile);
    void insertSong(const Song &s);
    QList<Genre> getGenres();
    QList<Artist> getArtists(const QString &genre=QString());
    QList<Album> getAlbums(const QString &artistId=QString(), const QString &genre=QString(), AlbumSort sort=AS_Year);
    QList<Song> getTracks(const QString &artistId, const QString &albumId, const QString &genre=QString(), AlbumSort sort=AS_Year, bool useFilter=true);
    #ifndef CANTATA_WEB
    QList<Song> songs(const QStringList &files, bool allowPlaylists=false) const;
    QList<Album> getAlbumsWithArtist(const QString &artist);
    QSet<QString> get(const QString &type);
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres);
    bool songExists(const Song &song);
    bool setFilter(const QString &f);
    const QString & getFilter() const { return filter; }
    int getCurrentVersion() const { return currentVersion; }
    #endif

Q_SIGNALS:
    void libraryUpdated();

public Q_SLOTS:
    void updateStarted(time_t ver);
    void insertSongs(QList<Song> *songs);
    void updateFinished();
    void abortUpdate();

protected:
    bool createTable(const QString &q);
    static Song getSong(const QSqlQuery &query);

protected:
    virtual void reset();
    void clearSongs(bool startTransaction=true);

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
    #ifndef CANTATA_WEB
    QMap<QString, QSet<QString> > detailsCache;
    #endif
};

#endif
