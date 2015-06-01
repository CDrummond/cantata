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

#include "librarydb.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

static const int constSchemaVersion=1;

bool LibraryDb::dbgEnabled=false;
#define DBUG if (dbgEnabled) qWarning() << metaObject()->className() << __FUNCTION__

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
    if (s.albumArtist().startsWith("The")) {
        return s.albumArtist().mid(4);
    }
    return QString();
}

static QString albumSort(const Song &s)
{
    if (!s.albumSort().isEmpty()) {
        return s.albumSort();
    }
    if (s.album.startsWith("The")) {
        return s.album.mid(4);
    }
    return QString();
}

LibraryDb::LibraryDb()
    : currentVersion(0)
    , newVersion(0)
    , db(0)
    , insertSongQuery(0)
    , genresQuery(0)
    , artistsQuery(0)
    , artistsGenreQuery(0)
    , albumsQuery(0)
    , albumsGenreQuery(0)
    , albumsNoArtistQuery(0)
    , albumsGenreNoArtistQuery(0)
    , albumsDetailsQuery(0)
    , albumsDetailsGenreQuery(0)
    , tracksQuery(0)
    , tracksGenreQuery(0)
    , tracksWildcardQuery(0)
{
    DBUG;
}

LibraryDb::~LibraryDb()
{
//    delete insertSongsQuery;
//    delete genresQuery;
//    delete artistsQuery;
//    delete artistsGenreQuery;
//    delete albumsQuery;
//    delete albumsGenreQuery;
//    delete albumsNoArtistQuery;
//    delete albumsGenreNoArtistQuery;
//    delete albumsDetailsQuery;
//    delete albumsDetailsGenreQuery;
//    delete tracksQuery;
//    delete tracksGenreQuery;
//    delete tracksWildcardQuery;
//    delete db;
}

bool LibraryDb::init(QString &dbName)
{
    if (db) {
        return true;
    }

    db=new QSqlDatabase(QSqlDatabase::addDatabase("QSQLITE"));
    db->setDatabaseName(dbName);
    if (!db->open()) {
        return false;
    }

    createTable("versions(collection integer, schema integer)");
    QSqlQuery query("select collection, schema from versions", *db);
    int schemaVersion=0;
    if (query.next()) {
        currentVersion=query.value(0).toUInt();
        schemaVersion=query.value(1).toUInt();
    }
    if (schemaVersion>0 && schemaVersion!=constSchemaVersion) {
        QSqlQuery(*db).exec("delete from songs");
    }
    if (0==currentVersion || (schemaVersion>0 && schemaVersion!=constSchemaVersion)) {
        QSqlQuery(*db).exec("delete from versions");
        QSqlQuery(*db).exec("insert into versions (collection, schema) values(0, "+QString::number(constSchemaVersion)+")");
    }
    DBUG << "Current version" << currentVersion;

    createTable("songs ("
                "version integer,"
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
                "primary key (version, file))");
    return true;
}

void LibraryDb::insertSong(const Song &s)
{
    if (!insertSongQuery) {
        insertSongQuery=new QSqlQuery(*db);
        insertSongQuery->prepare("insert into songs(version, file, artist, artistId, albumArtist, artistSort, composer, album, albumId, albumSort, title, genre, track, disc, time, year) "
                                 "values(:version, :file, :artist, :artistId, :albumArtist, :artistSort, :composer, :album, :albumId, :albumSort, :title, :genre, :track, :disc, :time, :year)");
    }
    insertSongQuery->bindValue(":version", (qulonglong)newVersion);
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
    if (!insertSongQuery->exec()) {
        qWarning() << "insert failed" << insertSongQuery->lastError() << newVersion << s.file;
    }
}

QList<LibraryDb::Genre> LibraryDb::getGenres()
{
    DBUG;
    QMap<QString, int> map;
    if (0!=currentVersion) {
        if (!genresQuery) {
            genresQuery=new QSqlQuery(*db);
            genresQuery->prepare("select distinct genre, artistId from songs");
        }
        genresQuery->exec();
        DBUG << "genresquery" << genresQuery->executedQuery() << genresQuery->size();
        while (genresQuery->next()) {
            map[genresQuery->value(0).toString()]++;
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
        QSqlQuery *query=0;
        if (genre.isEmpty()) {
            if (!artistsQuery) {
                artistsQuery=new QSqlQuery(*db);
                artistsQuery->prepare("select distinct artistId, albumId, artistSort from songs");
            }
            query=artistsQuery;
        } else {
            if (!artistsGenreQuery) {
                artistsGenreQuery=new QSqlQuery(*db);
                artistsGenreQuery->prepare("select distinct artistId, albumId, artistSort from songs where genre=:genre");
            }
            query=artistsGenreQuery;
            artistsGenreQuery->bindValue(":genre", genre);
        }
        query->exec();
        DBUG << "artistsquery" << query->executedQuery() << query->size();
        while (query->next()) {
            QString artist=query->value(0).toString();
            albumMap[artist]++;
            sortMap[artist]=query->value(2).toString();
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
    DBUG << artistId << genre;
    QList<LibraryDb::Album> albums;
    if (0!=currentVersion) {
        QSqlQuery *query=0;
        if (artistId.isEmpty()) {
            if (genre.isEmpty()) {
                if (!albumsNoArtistQuery) {
                    albumsNoArtistQuery=new QSqlQuery(*db);
                    albumsNoArtistQuery->prepare("select distinct album, albumId, albumSort, artistId, artistSort from songs");
                }
                query=albumsNoArtistQuery;
            } else {
                if (!albumsGenreNoArtistQuery) {
                    albumsGenreNoArtistQuery=new QSqlQuery(*db);
                    albumsGenreNoArtistQuery->prepare("select distinct album, albumId, albumSort, artistId, artistSort from songs where genre=:genre");
                }
                query=albumsGenreNoArtistQuery;
                albumsGenreNoArtistQuery->bindValue(":genre", genre);
            }
        } else {
            if (genre.isEmpty()) {
                if (!albumsQuery) {
                    albumsQuery=new QSqlQuery(*db);
                    albumsQuery->prepare("select distinct album, albumId, albumSort from songs where artistId=:artistId");
                }
                query=albumsQuery;
            } else {
                if (!albumsGenreQuery) {
                    albumsGenreQuery=new QSqlQuery(*db);
                    albumsGenreQuery->prepare("select distinct album, albumId, albumSort from songs where artistId=:artistId and genre=:genre");
                }
                query=albumsGenreQuery;
                albumsGenreQuery->bindValue(":genre", genre);
            }
            query->bindValue(":artistId", artistId);
        }
        query->exec();
        DBUG << "albumsquery" << query->executedQuery() << query->size();
        while (query->next()) {
            QSqlQuery *detailsQuery=0;
            if (genre.isEmpty()) {
                if (!albumsDetailsQuery) {
                    albumsDetailsQuery=new QSqlQuery(*db);
                    albumsDetailsQuery->prepare("select avg(year), count(track), sum(time) from songs where artistId=:artistId and albumId=:albumId");
                }
                detailsQuery=albumsDetailsQuery;
            } else {
                if (!albumsDetailsGenreQuery) {
                    albumsDetailsGenreQuery=new QSqlQuery(*db);
                    albumsDetailsGenreQuery->prepare("select avg(year), count(track), sum(time) from songs where artistId=:artistId and albumId=:albumId and genre=:genre");
                }
                detailsQuery=albumsDetailsGenreQuery;
                albumsDetailsGenreQuery->bindValue(":genre", genre );
            }
            QString album=query->value("album").toString();
            QString albumId=query->value("albumId").toString();
            QString albumSort=query->value("albumSort").toString();
            QString artist=artistId.isEmpty() ? query->value("artistId").toString() : QString();
            QString artistSort=artistId.isEmpty() ? query->value("artistSort").toString() : QString();

            detailsQuery->bindValue(":artistId", artistId.isEmpty() ? artist : artistId);
            detailsQuery->bindValue(":albumId", albumId);
            detailsQuery->exec();
            DBUG << "albumdetailsquery" << detailsQuery->size();
            if (detailsQuery->next()) {
                albums.append(Album(album, albumId, albumSort, artist, artistSort, detailsQuery->value(0).toInt(), detailsQuery->value(1).toInt(), detailsQuery->value(2).toInt()));
            }
        }
    }

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

    return albums;
}

QList<Song> LibraryDb::getTracks(const QString &artistId, const QString &albumId, const QString &genre, const QString &sort)
{
    DBUG << artistId << albumId << genre << sort;
    QList<Song> songs;
    if (0!=currentVersion) {
        QSqlQuery *query=0;
        if (artistId.isEmpty() || albumId.isEmpty()) {
            if (!tracksWildcardQuery) {
                tracksWildcardQuery=new QSqlQuery(*db);
                tracksWildcardQuery->prepare("select * from songs where artistId like :artistId and albumId like :albumId and genre like :genre");
            }
            query=tracksWildcardQuery;
            tracksWildcardQuery->bindValue(":genre", genre.isEmpty() ? "%" : genre);
        } else if (genre.isEmpty()) {
            if (!tracksQuery) {
                tracksQuery=new QSqlQuery(*db);
                tracksQuery->prepare("select * from songs where artistId=:artistId and albumId=:albumId");
            }
            query=tracksQuery;
        } else {
            if (!tracksGenreQuery) {
                tracksGenreQuery=new QSqlQuery(*db);
                tracksGenreQuery->prepare("select * from songs where artistId=:artistId and albumId=:albumId and genre=:genre");
            }
            query=tracksGenreQuery;
            tracksGenreQuery->bindValue(":genre", genre);
        }

        query->bindValue(":artistId", artistId.isEmpty() ? "%" : artistId);
        query->bindValue(":albumId", albumId.isEmpty() ? "%" : albumId);
        query->exec();
        DBUG << "tracksquery" << query->executedQuery() << query->size();
        while (query->next()) {
            songs.append(getSong(query));
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

void LibraryDb::updateStarted(time_t ver)
{
    newVersion=ver;
    timer.start();
    db->transaction();
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
    QSqlQuery("update versions set collection ="+QString::number(newVersion)).exec();
    if (currentVersion>0) {
        QSqlQuery("delete from songs where version = "+QString::number(currentVersion)).exec();
    }
    db->commit();
    currentVersion=newVersion;
    DBUG << timer.elapsed();
}

void LibraryDb::createTable(const QString &q)
{
    QSqlQuery query("create table if not exists "+q);

    if (!query.exec()) {
        qWarning() << "Failed to create table" << query.lastError();
        ::exit(-1);
    }
}

Song LibraryDb::getSong(QSqlQuery *query)
{
    Song s;
    s.file=query->value("file").toString();
    s.artist=query->value("artist").toString();
    s.albumartist=query->value("albumArtist").toString();
    s.setComposer(query->value("composer").toString());
    s.album=query->value("album").toString();
    QString val=query->value("albumId").toString();
    if (!val.isEmpty() && val!=s.album) {
        s.setMbAlbumId(val);
    }
    s.title=query->value("title").toString();
    s.genre=query->value("genre").toString();
    s.track=query->value("track").toUInt();
    s.disc=query->value("disc").toUInt();
    s.time=query->value("time").toUInt();
    s.year=query->value("year").toUInt();
    val=query->value("artistSort").toString();
    if (!val.isEmpty() && val!=s.albumArtist()) {
        s.setArtistSort(val);
    }
    val=query->value("albumSort").toString();
    if (!val.isEmpty() && val!=s.album) {
        s.setAlbumSort(val);
    }
    return s;
}
