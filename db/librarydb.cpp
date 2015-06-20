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

#include "librarydb.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QRegExp>
#include <QDebug>

static const int constSchemaVersion=1;

bool LibraryDb::dbgEnabled=false;
#define DBUG if (dbgEnabled) qWarning() << metaObject()->className() << __FUNCTION__

const QLatin1String LibraryDb::constFileExt(".sql");
const QLatin1String LibraryDb::constNullGenre("-");

const QLatin1String LibraryDb::constArtistAlbumsSortYear("year");
const QLatin1String LibraryDb::constArtistAlbumsSortName("name");
const QLatin1String LibraryDb::constAlbumsSortAlArYr("al-ar-yr");
const QLatin1String LibraryDb::constAlbumsSortAlYrAr("al-yr-ar");
const QLatin1String LibraryDb::constAlbumsSortArAlYr("ar-al-yr");
const QLatin1String LibraryDb::constAlbumsSortArYrAl("ar-yr-al");
const QLatin1String LibraryDb::constAlbumsSortYrAlAr("yt-al-ar");
const QLatin1String LibraryDb::constAlbumsSortYrArAl("yr-ar-al");

static bool albumsSortAlArYr(const LibraryDb::Album &a, const LibraryDb::Album &b)
{
    const QString &an=a.sort.isEmpty() ? a.name : a.sort;
    const QString &bn=b.sort.isEmpty() ? b.name : b.sort;
    int cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    const QString &aa=a.artistSort.isEmpty() ? a.artist : a.artistSort;
    const QString &ba=b.artistSort.isEmpty() ? b.artist : b.artistSort;
    cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }
    return a.year<b.year;
}

static bool albumsSortArAlYr(const LibraryDb::Album &a, const LibraryDb::Album &b)
{
    const QString &aa=a.artistSort.isEmpty() ? a.artist : a.artistSort;
    const QString &ba=b.artistSort.isEmpty() ? b.artist : b.artistSort;
    int cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    const QString &an=a.sort.isEmpty() ? a.name : a.sort;
    const QString &bn=b.sort.isEmpty() ? b.name : b.sort;
    cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    return a.year<b.year;
}

static bool albumsSortAlYrAr(const LibraryDb::Album &a, const LibraryDb::Album &b)
{
    const QString &an=a.sort.isEmpty() ? a.name : a.sort;
    const QString &bn=b.sort.isEmpty() ? b.name : b.sort;
    int cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    if (a.year!=b.year) {
        return a.year<b.year;
    }

    const QString &aa=a.artistSort.isEmpty() ? a.artist : a.artistSort;
    const QString &ba=b.artistSort.isEmpty() ? b.artist : b.artistSort;
    return aa.localeAwareCompare(ba)<0;
}

static bool albumsSortArYrAl(const LibraryDb::Album &a, const LibraryDb::Album &b)
{
    const QString &aa=a.artistSort.isEmpty() ? a.artist : a.artistSort;
    const QString &ba=b.artistSort.isEmpty() ? b.artist : b.artistSort;
    int cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    if (a.year!=b.year) {
        return a.year<b.year;
    }

    const QString &an=a.sort.isEmpty() ? a.name : a.sort;
    const QString &bn=b.sort.isEmpty() ? b.name : b.sort;
    return an.localeAwareCompare(bn)<0;
}

static bool albumsSortYrAlAr(const LibraryDb::Album &a, const LibraryDb::Album &b)
{
    if (a.year!=b.year) {
        return a.year<b.year;
    }

    const QString &an=a.sort.isEmpty() ? a.name : a.sort;
    const QString &bn=b.sort.isEmpty() ? b.name : b.sort;
    int cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    const QString &aa=a.artistSort.isEmpty() ? a.artist : a.artistSort;
    const QString &ba=b.artistSort.isEmpty() ? b.artist : b.artistSort;
    return aa.localeAwareCompare(ba)<0;
}

static bool albumsSortYrArAl(const LibraryDb::Album &a, const LibraryDb::Album &b)
{
    if (a.year!=b.year) {
        return a.year<b.year;
    }

    const QString &aa=a.artistSort.isEmpty() ? a.artist : a.artistSort;
    const QString &ba=b.artistSort.isEmpty() ? b.artist : b.artistSort;
    int cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    const QString &an=a.sort.isEmpty() ? a.name : a.sort;
    const QString &bn=b.sort.isEmpty() ? b.name : b.sort;
    return an.localeAwareCompare(bn)<0;
}

static bool sortSort(const Song &a, const Song &b)
{
    if (a.disc!=b.disc) {
        return a.disc<b.disc;
    }
    if (a.track!=b.track) {
        return a.track<b.track;
    }
    if (a.year!=b.year) {
        return a.year<b.year;
    }
    int cmp=a.title.localeAwareCompare(b.title);
    if (0!=cmp) {
        return cmp<0;
    }
    cmp=a.name().localeAwareCompare(b.name());
    if (0!=cmp) {
        return cmp<0;
    }
    cmp=a.genre.localeAwareCompare(b.genre);
    if (0!=cmp) {
        return cmp<0;
    }
    if (a.time!=b.time) {
        return a.time<b.time;
    }
    return a.file.compare(b.file)<0;
}

static bool songsSortAlArYr(const Song &a, const Song &b)
{
    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
    int cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
    cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    if (a.year!=b.year) {
        return a.year<b.year;
    }
    return sortSort(a, b);
}

static bool songsSortArAlYr(const Song &a, const Song &b)
{
    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
    int cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
    cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    if (a.year!=b.year) {
        return a.year<b.year;
    }
    return sortSort(a, b);
}

static bool songsSortAlYrAr(const Song &a, const Song &b)
{
    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
    int cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    if (a.year!=b.year) {
        return a.year<b.year;
    }

    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
    cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    return sortSort(a, b);
}

static bool songsSortArYrAl(const Song &a, const Song &b)
{
    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
    int cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    if (a.year!=b.year) {
        return a.year<b.year;
    }

    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
    cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    return sortSort(a, b);
}

static bool songsSortYrAlAr(const Song &a, const Song &b)
{
    if (a.year!=b.year) {
        return a.year<b.year;
    }

    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
    int cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
    cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    return sortSort(a, b);
}

static bool songsSortYrArAl(const Song &a, const Song &b)
{
    if (a.year!=b.year) {
        return a.year<b.year;
    }

    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
    int cmp=aa.localeAwareCompare(ba);
    if (cmp!=0) {
        return cmp<0;
    }

    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
    cmp=an.localeAwareCompare(bn);
    if (cmp!=0) {
        return cmp<0;
    }

    return sortSort(a, b);
}

static QString artistSort(const Song &s)
{
    if (!s.artistSortString().isEmpty()) {
        return s.artistSortString();
    }
    const QString &albumArtist=s.albumArtist();
    QString sort;
    if (albumArtist.startsWith("The")) {
        sort=albumArtist.mid(4);
    } else {
        sort=albumArtist;
    }
    sort=sort.remove('.');
    return sort==albumArtist ? QString() : sort;
}

static QString albumSort(const Song &s)
{
    if (!s.albumSort().isEmpty()) {
        return s.albumSort();
    }
    QString sort;
    if (s.album.startsWith("The")) {
        sort=s.album.mid(4);
    } else {
        sort=s.album;
    }
    sort=sort.remove('.');
    return sort==s.album ? QString() : sort;
}

// Code taken from Clementine's LibraryQuery
class SqlQuery
{
public:
    SqlQuery(const QString &colSpec, QSqlDatabase &database)
            : db(database)
            , fts(false)
            , columSpec(colSpec)
    {
    }

    void addWhere(const QString &column, const QVariant &value, const QString &op="=")
    {
        // ignore 'literal' for IN
        if (!op.compare("IN", Qt::CaseInsensitive)) {
            QStringList final;
            foreach(const QString &singleValue, value.toStringList()) {
                final.append("?");
                boundValues << singleValue;
            }
            whereClauses << QString("%1 IN (" + final.join(",") + ")").arg(column);
        } else {
            // Do integers inline - sqlite seems to get confused when you pass integers
            // to bound parameters
            if (QVariant::Int==value.type()) {
                whereClauses << QString("%1 %2 %3").arg(column, op, value.toString());
            } else {
                whereClauses << QString("%1 %2 ?").arg(column, op);
                boundValues << value;
            }
        }
    }

    void setFilter(const QString &filter)
    {
        if (!filter.isEmpty()) {
            whereClauses << "songs_fts match ?";
            boundValues << "\'"+filter+"\'";
            fts=true;
        }
    }

    bool exec()
    {
        QString sql=fts
                ? QString("SELECT %1 FROM songs INNER JOIN songs_fts AS fts ON songs.ROWID = fts.ROWID").arg(columSpec)
                : QString("SELECT %1 FROM songs").arg(columSpec);

        if (!whereClauses.isEmpty()) {
            sql += " WHERE " + whereClauses.join(" AND ");
        }
        query=QSqlQuery(sql, db);
        foreach (const QVariant &value, boundValues) {
            query.addBindValue(value);
        }
        return query.exec();
    }

    bool next() { return query.next(); }
    QString executedQuery() const { return query.executedQuery(); }
    int size() const { return query.size(); }
    QVariant value(int col) const { return query.value(col); }
    const QSqlQuery & realQuery() const { return query; }

private:
    QSqlDatabase &db;
    QSqlQuery query;
    bool fts;
    QString columSpec;
    QStringList whereClauses;
    QVariantList boundValues;
};

LibraryDb::LibraryDb(QObject *p, const QString &name)
    : QObject(p)
    , dbName(name)
    , currentVersion(0)
    , newVersion(0)
    , db(0)
    , insertSongQuery(0)
{
    DBUG;
}

LibraryDb::~LibraryDb()
{
    reset();
}

void LibraryDb::clear()
{
    if (db) {
        clearSongs();
        currentVersion=0;
        emit libraryUpdated();
    }
}

void LibraryDb::erase()
{
    reset();
    if (!dbFileName.isEmpty() && QFile::exists(dbFileName)) {
        QFile::remove(dbFileName);
    }
}

bool LibraryDb::init(const QString &dbFile)
{
    if (dbFile!=dbFileName) {
        reset();
        dbFileName=dbFile;
    }
    if (db) {
        return true;
    }

    DBUG << dbFile << dbName;
    db=new QSqlDatabase(QSqlDatabase::addDatabase("QSQLITE", dbName.isEmpty() ? QLatin1String(QSqlDatabase::defaultConnection) : dbName));
    db->setDatabaseName(dbFile);
    if (!db->open()) {
        delete db;
        db=0;
        return false;
    }

    if (!createTable("versions(collection integer, schema integer)")) {
        return false;
    }
    QSqlQuery query("select collection, schema from versions", *db);
    int schemaVersion=0;
    if (query.next()) {
        currentVersion=query.value(0).toUInt();
        schemaVersion=query.value(1).toUInt();
    }
    if (schemaVersion>0 && schemaVersion!=constSchemaVersion) {
        clearSongs();
    }
    if (0==currentVersion || (schemaVersion>0 && schemaVersion!=constSchemaVersion)) {
        QSqlQuery(*db).exec("delete from versions");
        QSqlQuery(*db).exec("insert into versions (collection, schema) values(0, "+QString::number(constSchemaVersion)+")");
    }
    DBUG << "Current version" << currentVersion;

    // NOTE: If this table is modified, update getSong()!
    if (createTable("songs ("
                    "file string, "
                    "artist string, "
                    "artistId string, "
                    "albumArtist string, "
                    "artistSort string, "
                    "composer string, "
                    "album string, "
                    "albumId string, "
                    "albumSort string, "
                    "title string, "
                    "genre string, "
                    "track integer, "
                    "disc integer, "
                    "time integer, "
                    "year integer, "
                    "type integer, "
                    "primary key (file))")) {
        #ifndef CANTATA_WEB
        QSqlQuery(*db).exec("create virtual table songs_fts using fts4(fts_artist, fts_artistId, fts_album, fts_title, tokenize=unicode61)");
        #endif
    } else {
        return false;
    }
    emit libraryUpdated();
    return true;
}

void LibraryDb::insertSong(const Song &s)
{
    if (!insertSongQuery) {
        insertSongQuery=new QSqlQuery(*db);
        insertSongQuery->prepare("insert into songs(file, artist, artistId, albumArtist, artistSort, composer, album, albumId, albumSort, title, genre, track, disc, time, year, type) "
                                 "values(:file, :artist, :artistId, :albumArtist, :artistSort, :composer, :album, :albumId, :albumSort, :title, :genre, :track, :disc, :time, :year, :type)");
    }
    insertSongQuery->bindValue(":file", s.file);
    insertSongQuery->bindValue(":artist", s.artist);
    insertSongQuery->bindValue(":artistId", s.artistOrComposer());
    insertSongQuery->bindValue(":albumArtist", s.albumartist);
    insertSongQuery->bindValue(":artistSort", artistSort(s));
    insertSongQuery->bindValue(":composer", s.composer());
    insertSongQuery->bindValue(":album", s.album);
    insertSongQuery->bindValue(":albumId", s.albumId());
    insertSongQuery->bindValue(":albumSort", albumSort(s));
    insertSongQuery->bindValue(":title", s.displayTitle());
    insertSongQuery->bindValue(":genre", s.genre.isEmpty() ? constNullGenre : s.genre);
    insertSongQuery->bindValue(":track", s.track);
    insertSongQuery->bindValue(":disc", s.disc);
    insertSongQuery->bindValue(":time", s.time);
    insertSongQuery->bindValue(":year", s.year);
    insertSongQuery->bindValue(":type", s.type);
    if (!insertSongQuery->exec()) {
        qWarning() << "insert failed" << insertSongQuery->lastError().text() << newVersion << s.file;
    }
}

QList<LibraryDb::Genre> LibraryDb::getGenres()
{
    DBUG;
    QMap<QString, int> map;
    if (0!=currentVersion) {
        SqlQuery query("distinct genre, artistId", *db);
        query.setFilter(filter);

        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            map[query.value(0).toString()]++;
        }
    }

    QList<LibraryDb::Genre> genres;
    QMap<QString, int>::ConstIterator it=map.constBegin();
    QMap<QString, int>::ConstIterator end=map.constEnd();
    for (; it!=end; ++it) {
        DBUG << it.key();
        genres.append(Genre(it.key(), it.value()));
    }
    qSort(genres);
    return genres;
}

QList<LibraryDb::Artist> LibraryDb::getArtists(const QString &genre)
{
    DBUG << genre;
    QMap<QString, QString> sortMap;
    QMap<QString, int> albumMap;
    if (0!=currentVersion) {
        SqlQuery query("distinct artistId, albumId, artistSort", *db);
        query.setFilter(filter);
        if (!genre.isEmpty()) {
            query.addWhere("genre", genre);
        }
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            QString artist=query.value(0).toString();
            albumMap[artist]++;
            sortMap[artist]=query.value(2).toString();
        }
    }

    QList<LibraryDb::Artist> artists;
    QMap<QString, int>::ConstIterator it=albumMap.constBegin();
    QMap<QString, int>::ConstIterator end=albumMap.constEnd();
    for (; it!=end; ++it) {
        DBUG << it.key();
        artists.append(Artist(it.key(), sortMap[it.key()], it.value()));
    }
    qSort(artists);
    return artists;
}

QList<LibraryDb::Album> LibraryDb::getAlbums(const QString &artistId, const QString &genre, const QString &sort)
{
    timer.start();
    DBUG << artistId << genre;
    QList<LibraryDb::Album> albums;
    if (0!=currentVersion) {
        SqlQuery query("distinct album, albumId, albumSort, artistId, artistSort", *db);
        query.setFilter(filter);
        if (!artistId.isEmpty()) {
            query.addWhere("artistId", artistId);
        }
        if (!genre.isEmpty()) {
            query.addWhere("genre", genre);
        }
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            QString album=query.value(0).toString();
            QString albumId=query.value(1).toString();
            QString albumSort=query.value(2).toString();
            QString artist=artistId.isEmpty() ? query.value(3).toString() : QString();
            QString artistSort=artistId.isEmpty() ? query.value(4).toString() : QString();
            SqlQuery detailsQuery("avg(year), count(track), sum(time)", *db);
            detailsQuery.setFilter(filter);

            if (!genre.isEmpty()) {
                detailsQuery.addWhere("genre", genre);
            }

            detailsQuery.addWhere("artistId", artistId.isEmpty() ? artist : artistId);
            detailsQuery.addWhere("albumId", albumId);
            detailsQuery.exec();
            DBUG << detailsQuery.executedQuery();
            if (detailsQuery.next()) {
                albums.append(Album(album, albumId, albumSort, artist, artistSort, detailsQuery.value(0).toInt(), detailsQuery.value(1).toInt(), detailsQuery.value(2).toInt()));
            }
        }
    }

    DBUG << "After select" << timer.elapsed();
    if (sort==constAlbumsSortAlYrAr) {
        qSort(albums.begin(), albums.end(), albumsSortAlYrAr);
    } else if (sort==constAlbumsSortArAlYr) {
        qSort(albums.begin(), albums.end(), albumsSortArAlYr);
    } else if (sort==constAlbumsSortArYrAl) {
        qSort(albums.begin(), albums.end(), albumsSortArYrAl);
    } else if (sort==constAlbumsSortYrAlAr) {
        qSort(albums.begin(), albums.end(), albumsSortYrAlAr);
    } else if (sort==constAlbumsSortYrArAl) {
        qSort(albums.begin(), albums.end(), albumsSortYrArAl);
    } else if (sort==constAlbumsSortAlArYr) {
        qSort(albums.begin(), albums.end(), albumsSortAlArYr);
    } else if (constArtistAlbumsSortName==sort) {
        qSort(albums.begin(), albums.end(), albumsSortArAlYr);
    } else {
        qSort(albums.begin(), albums.end(), albumsSortArYrAl);
    }
    DBUG << "After sort" << timer.elapsed();
    return albums;
}

QList<Song> LibraryDb::getTracks(const QString &artistId, const QString &albumId, const QString &genre, const QString &sort, bool useFilter)
{
    DBUG << artistId << albumId << genre << sort;
    QList<Song> songs;
    if (0!=currentVersion) {
        SqlQuery query("*", *db);
        if (useFilter) {
            query.setFilter(filter);
        }
        if (!artistId.isEmpty()) {
            query.addWhere("artistId", artistId);
        }
        if (!albumId.isEmpty()) {
            query.addWhere("albumId", albumId);
        }
        if (!genre.isEmpty()) {
            query.addWhere("genre", genre);
        }
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            songs.append(getSong(query.realQuery()));
        }
    }

    if (sort==constAlbumsSortAlYrAr) {
        qSort(songs.begin(), songs.end(), songsSortAlYrAr);
    } else if (sort==constAlbumsSortArAlYr) {
        qSort(songs.begin(), songs.end(), songsSortArAlYr);
    } else if (sort==constAlbumsSortArYrAl) {
        qSort(songs.begin(), songs.end(), songsSortArYrAl);
    } else if (sort==constAlbumsSortYrAlAr) {
        qSort(songs.begin(), songs.end(), songsSortYrAlAr);
    } else if (sort==constAlbumsSortYrArAl) {
        qSort(songs.begin(), songs.end(), songsSortYrArAl);
    } else if (sort==constAlbumsSortAlArYr) {
        qSort(songs.begin(), songs.end(), songsSortAlArYr);
    } else if (constArtistAlbumsSortName==sort) {
        qSort(songs.begin(), songs.end(), songsSortArAlYr);
    } else {
        qSort(songs.begin(), songs.end(), songsSortArYrAl);
    }
    return songs;
}

#ifndef CANTATA_WEB
QList<Song> LibraryDb::songs(const QStringList &files, bool allowPlaylists) const
{
    QList<Song> songList;
    foreach (const QString &f, files) {
        SqlQuery query("*", *db);
        query.addWhere("file", f);
        query.exec();
        DBUG << query.executedQuery();
        if (query.next()) {
            Song song=getSong(query.realQuery());
            if (allowPlaylists || Song::Playlist!=song.type) {
                songList.append(song);
            }
        }
    }

    return songList;
}

QList<LibraryDb::Album> LibraryDb::getAlbumsWithArtist(const QString &artist)
{
    QList<LibraryDb::Album> albums;
    if (0!=currentVersion) {
        SqlQuery query("distinct album, albumId, albumSort", *db);
        query.addWhere("artist", artist);
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            albums.append(Album(query.value(0).toString(), query.value(1).toString(), query.value(2).toString(), artist));
        }
    }

    qSort(albums.begin(), albums.end(), albumsSortArYrAl);

    return albums;
}

QSet<QString> LibraryDb::get(const QString &type)
{
    if (detailsCache.contains(type)) {
        return detailsCache[type];
    }
    QSet<QString> set;
    SqlQuery query("distinct "+type, *db);
    query.exec();
    DBUG << query.executedQuery();
    while (query.next()) {
        QString val=query.value(0).toString();
        if (!val.isEmpty()) {
            set.insert(val);
        }
    }
    detailsCache[type]=set;
    return set;
}

void LibraryDb::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres)
{
    artists=get("artist");
    albumArtists=get("albumArtist");
    composers=get("composer");
    albums=get("album");
    genres=get("genre");
}

bool LibraryDb::songExists(const Song &song)
{
    SqlQuery query("file", *db);
    query.addWhere("artistId", song.artistOrComposer());
    query.addWhere("albumId", song.albumId());
    query.addWhere("title", song.title);
    query.addWhere("track", song.track);
    query.exec();
    return query.next();
}

bool LibraryDb::setFilter(const QString &f)
{
    QString newFilter=f.trimmed().toLower();

    if (!f.isEmpty()) {
        QStringList strings(newFilter.split(QRegExp("\\s+")));
        QStringList tokens;
        foreach (QString str, strings) {
            str.remove('(');
            str.remove(')');
            str.remove('"');
            str.remove(':');
            str.remove('*');
            tokens.append(str+"* ");
        }
        newFilter=tokens.join(" OR ");
    }
    if (newFilter!=filter) {
        filter=newFilter;
        return true;
    }
    return false;
}
#endif

void LibraryDb::updateStarted(time_t ver)
{
    newVersion=ver;
    timer.start();
    db->transaction();
    if (currentVersion>0) {
        clearSongs(false);
    }
    DBUG;
}

void LibraryDb::insertSongs(QList<Song> *songs)
{
    //    DBUG << (void *)songs;
    if (!songs) {
        return;
    }

    foreach (const Song &s, *songs) {
        insertSong(s);
    }
    delete songs;
}

void LibraryDb::updateFinished()
{
    #ifndef CANTATA_WEB
    QSqlQuery(*db).exec("insert into songs_fts(fts_artist, fts_artistId, fts_album, fts_title) "
                        "select artist, artistId, album, title from songs");
    #endif
    QSqlQuery(*db).exec("update versions set collection ="+QString::number(newVersion));
    db->commit();
    currentVersion=newVersion;
    DBUG << timer.elapsed();
    emit libraryUpdated();
}

void LibraryDb::abortUpdate()
{
    db->rollback();
}

bool LibraryDb::createTable(const QString &q)
{
    QSqlQuery query(*db);
    if (!query.exec("create table if not exists "+q)) {
        qWarning() << "Failed to create table" << query.lastError().text();
        return false;
    }
    return true;
}

Song LibraryDb::getSong(const QSqlQuery &query)
{
    Song s;
    s.file=query.value(0).toString();
    s.artist=query.value(1).toString();
    s.albumartist=query.value(3).toString();
    s.setComposer(query.value(5).toString());
    s.album=query.value(6).toString();
    QString val=query.value(7).toString();
    if (!val.isEmpty() && val!=s.album) {
        s.setMbAlbumId(val);
    }
    s.title=query.value(9).toString();
    s.genre=query.value(10).toString();
    s.track=query.value(11).toUInt();
    s.disc=query.value(12).toUInt();
    s.time=query.value(13).toUInt();
    s.year=query.value(14).toUInt();
    s.type=(Song::Type)query.value(15).toUInt();
    val=query.value(4).toString();
    if (!val.isEmpty() && val!=s.albumArtist()) {
        s.setArtistSort(val);
    }
    val=query.value(8).toString();
    if (!val.isEmpty() && val!=s.album) {
        s.setAlbumSort(val);
    }

    return s;
}

void LibraryDb::reset()
{
    bool removeDb=0!=db;
    delete insertSongQuery;
    delete db;

    insertSongQuery=0;
    db=0;
    if (removeDb) {
        QSqlDatabase::removeDatabase(dbName);
    }
}

void LibraryDb::clearSongs(bool startTransaction)
{
    if (!db) {
        return;
    }
    if (startTransaction) {
        db->transaction();
    }
    QSqlQuery(*db).exec("delete from songs");
    #ifndef CANTATA_WEB
    QSqlQuery(*db).exec("delete from songs_fts");
    detailsCache.clear();
    #endif
    if (startTransaction) {
        db->commit();
    }
}
