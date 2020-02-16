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

#include "librarydb.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QRegExp>
#include <QDebug>
#include <algorithm>

static const int constSchemaVersion=5;

bool LibraryDb::dbgEnabled=false;
#define DBUG if (dbgEnabled) qWarning() << metaObject()->className() << __FUNCTION__ << (void *)this

const QLatin1String LibraryDb::constFileExt(".sql");
const QLatin1String LibraryDb::constNullGenre("-");

LibraryDb::AlbumSort LibraryDb::toAlbumSort(const QString &str)
{
    for (int i=0; i<AS_Count; ++i) {
        if (albumSortStr((AlbumSort)i)==str) {
            return (AlbumSort)i;
        }
    }
    return AS_AlArYr;
}

QString LibraryDb::albumSortStr(AlbumSort m)
{
    switch(m) {
    case AS_ArAlYr:   return "artist";
    default:
    case AS_AlArYr:   return "album";
    case AS_YrAlAr:   return "year";
    case AS_Modified: return "modified";

    // Re-added in 2.1
    case AS_ArYrAl:   return "aryral";
    case AS_AlYrAr:   return "alyral";
    case AS_YrArAl:   return "yraral";
    }
}

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

static bool albumsSortModified(const LibraryDb::Album &a, const LibraryDb::Album &b)
{
    if (a.lastModified==b.lastModified) {
        return albumsSortAlArYr(a, b);
    }
    return a.lastModified>b.lastModified;
}

static bool songSort(const Song &a, const Song &b)
{
    if (Song::SingleTracks==a.type && Song::SingleTracks==b.type) {
        int cmp=a.title.localeAwareCompare(b.title);
        if (0!=cmp) {
            return cmp<0;
        }
        cmp=a.artist.localeAwareCompare(b.artist);
        if (0!=cmp) {
            return cmp<0;
        }
    }

    if (a.disc!=b.disc) {
        return a.disc<b.disc;
    }
    if (a.track!=b.track) {
        return a.track<b.track;
    }
    if (a.displayYear()!=b.displayYear()) {
        return a.displayYear()<b.displayYear();
    }
    int cmp=a.title.localeAwareCompare(b.title);
    if (0!=cmp) {
        return cmp<0;
    }
    cmp=a.name().localeAwareCompare(b.name());
    if (0!=cmp) {
        return cmp<0;
    }
    cmp=a.displayGenre().localeAwareCompare(b.displayGenre());
    if (0!=cmp) {
        return cmp<0;
    }
    if (a.time!=b.time) {
        return a.time<b.time;
    }
    return a.file.compare(b.file)<0;
}

static bool songsSortAlAr(const Song &a, const Song &b)
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

//    if (a.displayYear()!=b.displayYear()) {
//        return a.displayYear()<b.displayYear();
//    }
    return songSort(a, b);
}

static bool songsSortArAl(const Song &a, const Song &b)
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

//    if (a.displayYear()!=b.displayYear()) {
//        return a.displayYear()<b.displayYear();
//    }
    return songSort(a, b);
}

//static bool songsSortAlYrAr(const Song &a, const Song &b)
//{
//    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
//    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
//    int cmp=an.localeAwareCompare(bn);
//    if (cmp!=0) {
//        return cmp<0;
//    }

//    if (a.displayYear()!=b.displayYear()) {
//        return a.displayYear()<b.displayYear();
//    }

//    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
//    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
//    cmp=aa.localeAwareCompare(ba);
//    if (cmp!=0) {
//        return cmp<0;
//    }

//    return songSort(a, b);
//}

//static bool songsSortArYrAl(const Song &a, const Song &b)
//{
//    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
//    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
//    int cmp=aa.localeAwareCompare(ba);
//    if (cmp!=0) {
//        return cmp<0;
//    }

//    if (a.displayYear()!=b.displayYear()) {
//        return a.displayYear()<b.displayYear();
//    }

//    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
//    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
//    cmp=an.localeAwareCompare(bn);
//    if (cmp!=0) {
//        return cmp<0;
//    }

//    return songSort(a, b);
//}

//static bool songsSortYrAlAr(const Song &a, const Song &b)
//{
//    if (a.displayYear()!=b.displayYear()) {
//        return a.displayYear()<b.displayYear();
//    }

//    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
//    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
//    int cmp=an.localeAwareCompare(bn);
//    if (cmp!=0) {
//        return cmp<0;
//    }

//    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
//    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
//    cmp=aa.localeAwareCompare(ba);
//    if (cmp!=0) {
//        return cmp<0;
//    }

//    return songSort(a, b);
//}

//static bool songsSortYrArAl(const Song &a, const Song &b)
//{
//    if (a.displayYear()!=b.displayYear()) {
//        return a.displayYear()<b.displayYear();
//    }

//    const QString aa=a.hasArtistSort() ? a.artistSort() : a.albumArtist();
//    const QString ba=b.hasArtistSort() ? b.artistSort() : b.albumArtist();
//    int cmp=aa.localeAwareCompare(ba);
//    if (cmp!=0) {
//        return cmp<0;
//    }

//    const QString an=a.hasAlbumSort() ? a.albumSort() : a.album;
//    const QString bn=b.hasAlbumSort() ? b.albumSort() : b.album;
//    cmp=an.localeAwareCompare(bn);
//    if (cmp!=0) {
//        return cmp<0;
//    }

//    return songSort(a, b);
//}

//static bool songsSortModified(const Song &a, const Song &b)
//{
//    if (a.lastModified==b.lastModified) {
//        return songsSortAlArYr(a, b);
//    }
//    return a.lastModified>b.lastModified;
//}

static QString artistSort(const Song &s)
{
    if (s.useComposer() && !s.composer().isEmpty()) {
        return s.composer();

    }
    if (!s.artistSortString().isEmpty()) {
        return s.artistSortString();
    }
    return Song::sortString(s.albumArtist());
}

static QString albumSort(const Song &s)
{
    if (!s.albumSort().isEmpty()) {
        return s.albumSort();
    }
    return Song::sortString(s.album);
}

// Code taken from Clementine's LibraryQuery
class SqlQuery
{
public:
    SqlQuery(const QString &colSpec, QSqlDatabase &database)
            : db(database)
            , fts(false)
            , columSpec(colSpec)
            , limit(0)
    {
    }

    void addWhere(const QString &column, const QVariant &value, const QString &op="=")
    {
        // ignore 'literal' for IN
        if (!op.compare("IN", Qt::CaseInsensitive)) {
            QStringList final;
            for(const QString &singleValue: value.toStringList()) {
                final.append("?");
                boundValues << singleValue;
            }
            whereClauses << QString("%1 IN (" + final.join(",") + ")").arg(column);
        } else {
            // Do integers inline - sqlite seems to get confused when you pass integers
            // to bound parameters
            if (QVariant::Int==value.type()) {
                whereClauses << QString("%1 %2 %3").arg(column, op, value.toString());
            } else if ("genre"==column) {
                QString clause("(");
                for (int i=0; i<Song::constNumGenres; ++i) {
                    clause+=QString("%1 %2 ?").arg(column+QString::number(i+1), op);
                    if (i<(Song::constNumGenres-1)) {
                        clause+=" OR ";
                    }
                    boundValues << value;
                }
                clause+=")";
                whereClauses << clause;
            } else {
                whereClauses << QString("%1 %2 ?").arg(column, op);
                boundValues << value;
            }
        }
    }

    void setFilter(const QString &filter, const QString yearFilter)
    {
        if (!filter.isEmpty()) {
            whereClauses << "songs_fts match ?";
            boundValues << "\'"+filter+"\'";
            fts=true;
        }
        if (!yearFilter.isEmpty()) {
            whereClauses << yearFilter;
        }
    }

    void setOrder(const QString &o)
    {
        order=o;
    }

    void setLimit(int l)
    {
        limit=l;
    }

    bool exec()
    {
        QString sql=fts
                ? QString("SELECT %1 FROM songs INNER JOIN songs_fts AS fts ON songs.ROWID = fts.ROWID").arg(columSpec)
                : QString("SELECT %1 FROM songs").arg(columSpec);

        if (!whereClauses.isEmpty()) {
            sql+=" WHERE " + whereClauses.join(" AND ");
        }
        if (!order.isEmpty()) {
            sql+=" ORDER by "+order;
        }
        if (limit>0) {
            sql+=" LIMIT "+QString::number(limit);
        }
        query = QSqlQuery(db);
        query.prepare(sql);

        for (const QVariant &value: boundValues) {
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
    QString order;
    int limit;
};

LibraryDb::LibraryDb(QObject *p, const QString &name)
    : QObject(p)
    , dbName(name)
    , currentVersion(0)
    , newVersion(0)
    , db(nullptr)
    , insertSongQuery(nullptr)
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
        DBUG;
        erase();
        currentVersion=0;
        init(dbFileName);
    }
}

void LibraryDb::erase()
{
    reset();
    if (!dbFileName.isEmpty() && QFile::exists(dbFileName)) {
        QFile::remove(dbFileName);
    }
}

enum SongFields {
    SF_file,
    SF_artist ,
    SF_artistId,
    SF_albumArtist,
    SF_artistSort,
    SF_composer,
    SF_album,
    SF_albumId,
    SF_albumSort,
    SF_title,
    SF_genre1,
    SF_genre2,
    SF_genre3,
    SF_genre4,
    SF_track,
    SF_disc,
    SF_time,
    SF_year,
    SF_origYear,
    SF_type,
    SF_lastModified
};

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
    currentVersion=0;
    db=new QSqlDatabase(QSqlDatabase::addDatabase("QSQLITE", dbName.isEmpty() ? QLatin1String(QSqlDatabase::defaultConnection) : dbName));
    if (!db || !db->isValid()) {
        emit error(tr("Database error - please check Qt SQLite driver is installed"));
        return false;
    }
    db->setDatabaseName(dbFile);
    DBUG << (void *)db;
    if (!db->open()) {
        delete db;
        db=nullptr;
        DBUG << "Failed to open";
        return false;
    }

    if (!createTable("versions(collection integer, schema integer)")) {
        DBUG << "Failed to create versions table";
        return false;
    }
    QSqlQuery query("select collection, schema from versions", *db);
    int schemaVersion=0;
    if (query.next()) {
        currentVersion=query.value(0).toUInt();
        schemaVersion=query.value(1).toUInt();
    }
    if (schemaVersion>0 && schemaVersion!=constSchemaVersion) {
        DBUG << "Scheme version changed";
        currentVersion=0;
        erase();
        return init(dbFile);
    }
    if (0==currentVersion || (schemaVersion>0 && schemaVersion!=constSchemaVersion)) {
        QSqlQuery(*db).exec("delete from versions");
        QSqlQuery(*db).exec("insert into versions (collection, schema) values(0, "+QString::number(constSchemaVersion)+")");
    }
    DBUG << "Current version" << currentVersion;

    // NOTE: The order here MUST match SongFields enum above!!!
    if (createTable("songs ("
                    "file text, "
                    "artist text, "
                    "artistId text, "
                    "albumArtist text, "
                    "artistSort text, "
                    "composer text, "
                    "album text, "
                    "albumId text, "
                    "albumSort text, "
                    "title text, "
                    "genre1 text, "
                    "genre2 text, "
                    "genre3 text, "
                    "genre4 text, "
                    "track integer, "
                    "disc integer, "
                    "time integer, "
                    "year integer, "
                    "origYear integer, "
                    "type integer, "
                    "lastModified integer, "
                    "primary key (file))")) {
        QSqlQuery fts(*db);
        if (!fts.exec("create virtual table if not exists songs_fts using fts4(fts_artist, fts_artistId, fts_album, fts_albumId, fts_title, tokenize=unicode61)")) {
            DBUG << "Failed to create FTS table" << fts.lastError().text() << "trying again with simple tokenizer";
            if (!fts.exec("create virtual table if not exists songs_fts using fts4(fts_artist, fts_artistId, fts_album, fts_albumId, fts_title, tokenize=simple)")) {
                DBUG << "Failed to create FTS table" << fts.lastError().text();
            }
        }
    } else {
        DBUG << "Failed to create songs table";
        return false;
    }
    emit libraryUpdated();
    DBUG << "Created";
    return true;
}

void LibraryDb::insertSong(const Song &s)
{
    if (!insertSongQuery) {
        insertSongQuery=new QSqlQuery(*db);
        insertSongQuery->prepare("insert into songs(file, artist, artistId, albumArtist, artistSort, composer, album, albumId, albumSort, title, genre1, genre2, genre3, genre4, track, disc, time, year, origYear, type, lastModified) "
                                 "values(:file, :artist, :artistId, :albumArtist, :artistSort, :composer, :album, :albumId, :albumSort, :title, :genre1, :genre2, :genre3, :genre4, :track, :disc, :time, :year, :origYear, :type, :lastModified)");
    }
    QString albumId=s.albumId();
    insertSongQuery->bindValue(":file", s.file);
    insertSongQuery->bindValue(":artist", s.artist);
    insertSongQuery->bindValue(":artistId", s.albumArtistOrComposer());
    insertSongQuery->bindValue(":albumArtist", s.albumartist);
    insertSongQuery->bindValue(":artistSort", artistSort(s));
    insertSongQuery->bindValue(":composer", s.composer());
    insertSongQuery->bindValue(":album", s.album==albumId ? QString() : s.album);
    insertSongQuery->bindValue(":albumId", albumId);
    insertSongQuery->bindValue(":albumSort", albumSort(s));
    insertSongQuery->bindValue(":title", s.title);
    for (int i=0; i<Song::constNumGenres; ++i) {
        insertSongQuery->bindValue(":genre"+QString::number(i+1), s.genres[i].isEmpty() ? constNullGenre : s.genres[i]);
    }
    insertSongQuery->bindValue(":track", s.track);
    insertSongQuery->bindValue(":disc", s.disc);
    insertSongQuery->bindValue(":time", s.time);
    insertSongQuery->bindValue(":year", s.year);
    insertSongQuery->bindValue(":origYear", s.origYear);
    insertSongQuery->bindValue(":type", s.type);
    insertSongQuery->bindValue(":lastModified", s.lastModified);
    if (!insertSongQuery->exec()) {
        qWarning() << "insert failed" << insertSongQuery->lastError().text() << newVersion << s.file;
    }
}

QList<LibraryDb::Genre> LibraryDb::getGenres()
{
    DBUG;
    QMap<QString, QSet<QString> > map;
    if (0!=currentVersion && db) {
        QString queryStr("distinct ");
        for (int i=0; i<Song::constNumGenres; ++i) {
            queryStr+="genre"+QString::number(i+1)+", ";
        }
        queryStr+="artistId";
        SqlQuery query(queryStr, *db);
        query.setFilter(filter, yearFilter);

        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            for (int i=0; i<Song::constNumGenres; ++i) {
                QString genre=query.value(i).toString();
                if (!genre.isEmpty() && genre!=constNullGenre) {
                    map[genre].insert(query.value(Song::constNumGenres).toString());
                }
            }
        }
    }

    QList<LibraryDb::Genre> genres;
    QMap<QString, QSet<QString> >::ConstIterator it=map.constBegin();
    QMap<QString, QSet<QString> >::ConstIterator end=map.constEnd();
    for (; it!=end; ++it) {
        DBUG << it.key();
        genres.append(Genre(it.key(), it.value().size()));
    }
    std::sort(genres.begin(), genres.end());
    return genres;
}

QList<LibraryDb::Artist> LibraryDb::getArtists(const QString &genre)
{
    DBUG << genre;
    QMap<QString, QString> sortMap;
    QMap<QString, int> albumMap;
    if (0!=currentVersion && db) {
        SqlQuery query("distinct artistId, albumId, artistSort", *db);
        query.setFilter(filter, yearFilter);
        if (!genre.isEmpty()) {
            query.addWhere("genre", genre);
        } else if (!genreFilter.isEmpty()) {
            query.addWhere("genre", genreFilter);
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
//        DBUG << it.key();
        artists.append(Artist(it.key(), sortMap[it.key()], it.value()));
    }
    std::sort(artists.begin(), artists.end());
    return artists;
}

QList<LibraryDb::Album> LibraryDb::getAlbums(const QString &artistId, const QString &genre, AlbumSort sort)
{
    timer.start();
    DBUG << artistId << genre;
    QList<Album> albums;
    if (0!=currentVersion && db) {
        bool wantModified=AS_Modified==sort;
        bool wantArtist=artistId.isEmpty();
        QString queryString="album, albumId, albumSort, artist, albumArtist, composer";
        for (int i=0; i<Song::constNumGenres; ++i) {
            queryString+=", genre"+QString::number(i+1);
        }
        queryString+=", type, year, origYear, time";
        if (wantModified) {
            queryString+=", lastModified";
        }
        if (wantArtist) {
            queryString+=", artistId, artistSort";
        }
        SqlQuery query(queryString, *db);
        query.setFilter(filter, yearFilter);
        if (!artistId.isEmpty()) {
            query.addWhere("artistId", artistId);
        }
        if (!genre.isEmpty()) {
            query.addWhere("genre", genre);
        } else if (!genreFilter.isEmpty()) {
            query.addWhere("genre", genreFilter);
        }
        query.exec();
        int count=0;
        QMap<QString, Album> entries;
        QMap<QString, QSet<QString> > albumIdArtists; // Map of albumId -> albumartists/composers
        while (query.next()) {
            count++;
            int col=0;
            QString album=query.value(col++).toString();
            QString albumId=query.value(col++).toString();
            QString albumSort=query.value(col++).toString();

            Song s;
            s.artist=query.value(col++).toString();
            s.albumartist=query.value(col++).toString();
            s.setComposer(query.value(col++).toString());
            s.album=album.isEmpty() ? albumId : album;
            for (int i=0; i<Song::constNumGenres; ++i) {
                QString genre=query.value(col++).toString();
                if (genre!=constNullGenre) {
                    s.addGenre(genre);
                }
            }
            s.type=(Song::Type)query.value(col++).toInt();
            s.year=query.value(col++).toInt();
            s.origYear=query.value(col++).toInt();
            if (Song::SingleTracks==s.type) {
                s.album=Song::singleTracks();
                s.albumartist=Song::variousArtists();
                s.year = s.origYear = 0;
            }
            album=s.albumName();
            int time=query.value(col++).toInt();
            int lastModified=wantModified ? query.value(col++).toInt() : 0;
            QString artist=wantArtist ? query.value(col++).toString() : QString();
            QString artistSort=wantArtist ? query.value(col++).toString() : QString();
            // If listing albums not filtered on artist, then if we have a unqique id for the album use that.
            // This will allow us to grouup albums with different composers when the composer tweak is set
            // Issue #1025
            bool haveUniqueId=wantArtist && !albumId.isEmpty() && !album.isEmpty() && albumId!=album;
            QString key=haveUniqueId ? albumId : ('{'+albumId+"}{"+(wantArtist ? artist : artistId)+'}');
            QMap<QString, Album>::iterator it=entries.find(key);

            if (it==entries.end()) {
                entries.insert(key, Album(album.isEmpty() ? albumId : album, albumId, albumSort, artist, artistSort, s.displayYear(), 1, time, lastModified, haveUniqueId));
            } else {
                Album &al=it.value();
                if (wantModified) {
                    al.lastModified=qMax(al.lastModified, lastModified);
                }
                al.year=qMax(al.year, (int)s.displayYear());
                al.duration+=time;
                al.trackCount++;
            }
            if (haveUniqueId) {
                QMap<QString, QSet<QString> >::iterator aIt = albumIdArtists.find(key);
                if (aIt == albumIdArtists.end()) {
                    albumIdArtists.insert(key, QSet<QString>() << artist);
                } else {
                    aIt.value().insert(artist);
                }
            }
        }

        QMap<QString, QSet<QString> >::ConstIterator aIt = albumIdArtists.constBegin();
        QMap<QString, QSet<QString> >::ConstIterator aEnd = albumIdArtists.constEnd();
        for(; aIt!=aEnd; ++aIt) {
            if (aIt.value().count()>1) {
                QStringList artists = aIt.value().toList();
                artists.sort();
                Album &al = entries.find(aIt.key()).value();
                al.artist = artists.join(", ");
                al.artistSort = QString();
            }
        }

        albums=entries.values();
        DBUG << count << albums.count();
    }

    DBUG << "After select" << timer.elapsed();
    switch(sort) {
    case AS_AlArYr:
        std::sort(albums.begin(), albums.end(), albumsSortAlArYr);
        break;
    case AS_AlYrAr:
        std::sort(albums.begin(), albums.end(), albumsSortAlYrAr);
        break;
    case AS_ArAlYr:
        std::sort(albums.begin(), albums.end(), albumsSortArAlYr);
        break;
    case AS_ArYrAl:
        std::sort(albums.begin(), albums.end(), albumsSortArYrAl);
        break;
    case AS_YrAlAr:
        std::sort(albums.begin(), albums.end(), albumsSortYrAlAr);
        break;
    case AS_YrArAl:
        std::sort(albums.begin(), albums.end(), albumsSortYrArAl);
        break;
    case AS_Modified:
        std::sort(albums.begin(), albums.end(), albumsSortModified);
        break;
    default:
        break;
    }
    DBUG << "After sort" << timer.elapsed();
    return albums;
}

QList<Song> LibraryDb::getTracks(const QString &artistId, const QString &albumId, const QString &genre, AlbumSort sort, bool useFilter)
{
    DBUG << artistId << albumId << genre << sort;
    QList<Song> songs;
    if (0!=currentVersion && db) {
        SqlQuery query("*", *db);
        if (useFilter) {
            query.setFilter(filter, yearFilter);
        }
        if (!artistId.isEmpty()) {
            query.addWhere("artistId", artistId);
        }
        if (!albumId.isEmpty()) {
            query.addWhere("albumId", albumId);
        }
        if (!genre.isEmpty()) {
            query.addWhere("genre", genre);
        } else if (useFilter && !genreFilter.isEmpty()) {
            query.addWhere("genre", genreFilter);
        }
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            songs.append(getSong(query.realQuery()));
        }
    }

    switch(sort) {
    case AS_AlArYr:
        std::sort(songs.begin(), songs.end(), songsSortAlAr);
        break;
    case AS_ArAlYr:
        std::sort(songs.begin(), songs.end(), songsSortArAl);
        break;
//    case AS_Year:
//        std::sort(songs.begin(), songs.end(), songsSortYrAlAr);
//        break;
//    case AS_Modified:
//        std::sort(songs.begin(), songs.end(), songsSortModified);
//        break;
    default:
        std::sort(songs.begin(), songs.end(), songSort);
        break;
    }
    return songs;
}

QList<Song> LibraryDb::getTracks(int rowFrom, int count)
{
    QList<Song> songList;
    if (db) {
        SqlQuery query("*", *db);
        query.addWhere("rowid", rowFrom, ">");
        query.addWhere("rowid", rowFrom+count, "<=");
        query.addWhere("type", 0);
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            songList.append(getSong(query.realQuery()));
        }
    }
    return songList;
}

int LibraryDb::trackCount()
{
    if (!db) {
        return 0;
    }
    SqlQuery query("(count())", *db);
    query.addWhere("type", 0);
    query.exec();
    DBUG << query.executedQuery();
    int numTracks=query.next() ? query.value(0).toInt() : 0;
    DBUG << numTracks;
    return numTracks;
}

QList<Song> LibraryDb::songs(const QStringList &files, bool allowPlaylists) const
{
    QList<Song> songList;
    if (0!=currentVersion && db) {
        for (const QString &f: files) {
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
    }

    return songList;
}

QList<LibraryDb::Album> LibraryDb::getAlbumsWithArtistOrComposer(const QString &artist)
{
    QList<LibraryDb::Album> albums;
    if (0!=currentVersion && db) {
        SqlQuery query("distinct album, albumId, albumSort", *db);
        query.addWhere("artist", artist);
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            QString album=query.value(0).toString();
            QString albumId=query.value(1).toString();
            albums.append(Album(album.isEmpty() ? albumId : album, albumId, query.value(2).toString(), artist));
        }
    }

    if (albums.isEmpty()) {
        // No artist albums? Try composer...
        SqlQuery query("distinct album, albumId, albumSort", *db);
        query.addWhere("composer", artist);
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            QString album=query.value(0).toString();
            QString albumId=query.value(1).toString();
            albums.append(Album(album.isEmpty() ? albumId : album, albumId, query.value(2).toString(), artist));
        }
    }

    std::sort(albums.begin(), albums.end(), albumsSortArAlYr);

    return albums;
}

LibraryDb::Album LibraryDb::getRandomAlbum(const QString &genre, const QString &artist)
{
    Album al;
    if (0!=currentVersion && db) {
        SqlQuery query("artistId, albumId", *db);
        query.setOrder("random()");
        query.setLimit(1);
        if (!artist.isEmpty()) {
            query.addWhere("artistId", artist);
        }
        if (!genre.isEmpty()) {
            query.addWhere("genre", genre);
        } else if (!genreFilter.isEmpty()) {
            query.addWhere("genre", genreFilter);
        }
        if (!yearFilter.isEmpty()) {
            query.addWhere("year", yearFilter);
        }
        query.exec();
        DBUG << query.executedQuery();
        if (query.next()) {
            al.artist=query.value(0).toString();
            al.id=query.value(1).toString();
        }
    }
    return al;
}

LibraryDb::Album LibraryDb::getRandomAlbum(const QStringList &genres, const QStringList &artists)
{
    if (genres.isEmpty() && artists.isEmpty()) {
        return getRandomAlbum(QString(), QString());
    }

    QList<Album> albums;

    for (const QString &genre: genres) {
        albums.append(getRandomAlbum(genre, QString()));
    }

    for (const QString &artist: artists) {
        albums.append(getRandomAlbum(QString(), artist));
    }

    if (albums.isEmpty()) {
        return Album();
    }

    return albums.at(Utils::random(albums.count()));
}

QSet<QString> LibraryDb::get(const QString &type)
{
    if (detailsCache.contains(type)) {
        return detailsCache[type];
    }
    QSet<QString> set;
    if (!db) {
        return set;
    }

    QStringList columns;
    bool isGenre="genre"==type;
    bool isAlbum="album"==type;

    if (isGenre) {
        for (int i=0; i<Song::constNumGenres; ++i) {
            columns << type+QString::number(i+1);
        }
    } else if (isAlbum) {
        columns << type << "albumId";
    } else {
        columns << type;
    }

    for (const QString &col: columns) {
        SqlQuery query("distinct "+col, *db);
        query.exec();
        DBUG << query.executedQuery();
        while (query.next()) {
            if (isGenre) {
                for (int i=0; i<Song::constNumGenres; ++i) {
                    QString val=query.value(i).toString();
                    if (!val.isEmpty() && constNullGenre!=val) {
                        set.insert(val);
                    }
                }
            } else if (isAlbum) {
                // For albums, we have album and albumId
                // album is only stored if it is different to albumId - in this case albumId would be the musicbrainz id
                QString val=query.value(0).toString();
                if (val.isEmpty()) {
                    val=query.value(1).toString();
                }
                if (!val.isEmpty()) {
                    set.insert(val);
                }
            } else {
                QString val=query.value(0).toString();
                if (!val.isEmpty()) {
                    set.insert(val);
                }
            }
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
    if (!db) {
        return false;
    }
    SqlQuery query("file", *db);
    query.addWhere("artistId", song.albumArtistOrComposer());
    query.addWhere("albumId", song.albumId());
    query.addWhere("title", song.title);
    query.addWhere("track", song.track);
    query.exec();
    return query.next();
}

static const quint16 constMinYear=1500;
static const quint16 constMaxYear=2500; // 2500 (bit hopeful here :-) )

bool LibraryDb::setFilter(const QString &f, const QString &genre)
{
    QString newFilter=f.trimmed().toLower();
    QString year;
    if (!f.isEmpty()) {
        QStringList strings(newFilter.split(QRegExp("\\s+")));
        static QList<QLatin1Char> replaceChars=QList<QLatin1Char>() << QLatin1Char('(') << QLatin1Char(')') << QLatin1Char('"')
                                                                    << QLatin1Char(':') << QLatin1Char('-') << QLatin1Char('#');
        QStringList tokens;
        for (QString str: strings) {
            if (str.startsWith('#')) {
                QStringList parts=str.mid(1).split('-');
                if (1==parts.length()) {
                    int val=parts.at(0).simplified().toUInt();
                    if (val>=constMinYear && val<=constMaxYear) {
                        if (Song::useOriginalYear()) {
                            year = QString().sprintf("( (origYear = %d) OR (origYear = 0 AND year = %d) )", val, val);
                        } else {
                            year = QString().sprintf("year = %d", val);
                        }
                        continue;
                    }
                } else if (2==parts.length()) {
                    int from=parts.at(0).simplified().toUInt();
                    int to=parts.at(1).simplified().toUInt();
                    if (from>=constMinYear && from<=constMaxYear && to>=constMinYear && to<=constMaxYear) {
                        if (Song::useOriginalYear()) {
                            year = QString().sprintf("( (origYear >= %d AND origYear <= %d) OR (origYear = 0 AND year >= %d AND year <= %d))",
                                                     qMin(from, to), qMax(from, to), qMin(from, to), qMax(from, to));
                        } else {
                            year = QString().sprintf("year >= %d AND year <= %d", qMin(from, to), qMax(from, to));
                        }
                        continue;
                    }
                }
            }
            for (const QLatin1Char ch: replaceChars) {
                str.replace(ch, '?');
            }
            if (str.length()>0) {
                tokens.append(str+QLatin1String("*"));
            }
        }
        newFilter=tokens.join(" ");
        DBUG << newFilter;
    }
    bool modified=newFilter!=filter || genre!=genreFilter || year!=yearFilter;
    filter=newFilter;
    genreFilter=genre;
    yearFilter=year;
    return modified;
}

void LibraryDb::updateStarted(time_t ver)
{
    DBUG << (void *)db;
    if (!db) {
        return;
    }
    newVersion=ver;
    timer.start();
    db->transaction();
    if (currentVersion>0) {
        clearSongs(false);
    }
}

void LibraryDb::insertSongs(QList<Song> *songs)
{
    DBUG << (int)(songs ? songs->size() : -1);
    if (!songs) {
        return;
    }

    for (const Song &s: *songs) {
        insertSong(s);
    }
    delete songs;
}

void LibraryDb::updateFinished()
{
    if (!db) {
        return;
    }
    DBUG << timer.elapsed();
    DBUG << "update fts" << timer.elapsed();
    QSqlQuery(*db).exec("insert into songs_fts(fts_artist, fts_artistId, fts_album, fts_albumId, fts_title) "
                        "select artist, artistId, album, albumId, title from songs");
    QSqlQuery(*db).exec("update versions set collection ="+QString::number(newVersion));
    DBUG << "commit" << timer.elapsed();
    db->commit();
    currentVersion=newVersion;
    DBUG << "complete" << timer.elapsed();
    emit libraryUpdated();
}

void LibraryDb::abortUpdate()
{
    if (db) {
        db->rollback();
    }
}

bool LibraryDb::createTable(const QString &q)
{
    if (!db) {
        return false;
    }
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
    s.file=query.value(SF_file).toString();
    s.artist=query.value(SF_artist).toString();
    s.albumartist=query.value(SF_albumArtist).toString();
    s.setComposer(query.value(SF_composer).toString());
    s.album=query.value(SF_album).toString();
    QString val=query.value(SF_albumId).toString();
    if (s.album.isEmpty()) {
        s.album=val;
        val=QString();
    }
    if (!val.isEmpty() && val!=s.album) {
        s.setMbAlbumId(val);
    }
    s.title=query.value(SF_title).toString();
    for (int i=SF_genre1; i<SF_genre1+Song::constNumGenres; ++i) {
        QString genre=query.value(i).toString();
        if (genre!=constNullGenre) {
            s.addGenre(genre);
        }
    }
    s.track=query.value(SF_track).toUInt();
    s.disc=query.value(SF_disc).toUInt();
    s.time=query.value(SF_time).toUInt();
    s.year=query.value(SF_year).toUInt();
    s.origYear=query.value(SF_origYear).toUInt();
    s.type=(Song::Type)query.value(SF_type).toUInt();
    val=query.value(SF_artistSort).toString();
    if (!val.isEmpty() && val!=s.albumArtist()) {
        s.setArtistSort(val);
    }
    val=query.value(SF_albumSort).toString();
    if (!val.isEmpty() && val!=s.album) {
        s.setAlbumSort(val);
    }
    s.lastModified=query.value(SF_lastModified).toUInt();

    return s;
}

void LibraryDb::reset()
{
    bool removeDb=nullptr!=db;
    delete insertSongQuery;
    if (db) {
        db->close();
    }
    delete db;

    insertSongQuery=nullptr;
    db=nullptr;
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
    QSqlQuery(*db).exec("delete from songs_fts");
    detailsCache.clear();
    if (startTransaction) {
        db->commit();
    }
}

#include "moc_librarydb.cpp"
