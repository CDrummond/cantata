/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef COVERS_H
#define COVERS_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QMap>
#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QCache>
#include "mpd/song.h"
#include "config.h"

class QString;
class Thread;
class NetworkJob;
class QMutex;
class QTimer;
#ifdef ENABLE_KDE_SUPPORT
class KUrl;
#endif
class NetworkAccessManager;

class CoverDownloader : public QObject
{
    Q_OBJECT

public:
    enum JobType {
        JobHttpJpg,
        JobHttpPng,
        JobLastFm
    };

    struct Job
    {
        Job(const Song &s, const QString &d, bool a=false)
            : song(s), filePath(s.filePath()), dir(d), isArtist(a), type(JobLastFm), level(0) { }
        Song song;
        QString filePath;
        QString dir;
        bool isArtist;
        JobType type;
        int level;
    };

    CoverDownloader();
    ~CoverDownloader() { }

    void stop();

public Q_SLOTS:
    void download(const Song &s);

Q_SIGNALS:
    void cover(const Song &song, const QImage &img, const QString &file);
    void artistImage(const Song &song, const QImage &img, const QString &file);

private:
    bool downloadViaHttp(Job &job, JobType type);
    void downloadViaLastFm(Job &job);

private Q_SLOTS:
    void lastFmCallFinished();
    void jobFinished();
    void onlineJobFinished();

private:
    void failed(const Job &job);
    QString saveImg(const Job &job, const QImage &img, const QByteArray &raw);
    QHash<NetworkJob *, Job>::Iterator findJob(const Job &job);
    NetworkAccessManager * network();

private:
    QHash<NetworkJob *, Job> jobs;

private:
    Thread *thread;
    NetworkAccessManager *manager;
};

struct LocatedCover
{
    LocatedCover(const Song &s=Song(), const QImage &i=QImage(), const QString &f=QString())
        : song(s), img(i), fileName(f) { }
    Song song;
    QImage img;
    QString fileName;
};

class CoverLocator : public QObject
{
    Q_OBJECT
public:
    CoverLocator();
    ~CoverLocator() { }

    void stop();

Q_SIGNALS:
    void located(const QList<LocatedCover> &covers);

public Q_SLOTS:
    void locate(const Song &s);
    void locate();

private:
    void startTimer(int interval);

private:
    Thread *thread;
    QTimer *timer;
    QList<Song> queue;
};

struct LoadedCover
{
    LoadedCover(const Song &sng=Song(), const QImage &i=QImage())
        : song(sng), img(i) { }
    Song song;
    QImage img;
};

class CoverLoader : public QObject
{
    Q_OBJECT
public:
    CoverLoader();
    ~CoverLoader() { }

    void stop();

Q_SIGNALS:
    void loaded(const QList<LoadedCover> &covers);

public Q_SLOTS:
    void load(const Song &song);
    void load();

private:
    void startTimer(int interval);

private:
    Thread *thread;
    QTimer *timer;
    QList<LoadedCover> queue;
};

class Covers : public QObject
{
    Q_OBJECT

public:
    struct Image
    {
        Image(const QImage &i=QImage(), const QString &f=QString())
            : img(i)
            , fileName(f) {
        }
        bool validFileName() const;
        QImage img;
        QString fileName;
    };

    static void enableDebug();
    static bool debugEnabled();

    static const QSize constMaxSize;
    static const QLatin1String constLastFmApiKey;
    static const QLatin1String constCoverDir;
    static const QLatin1String constScaledCoverDir;
    static const QLatin1String constFileName;
    static const QLatin1String constCddaCoverDir;
    static const QLatin1String constArtistImage;

    static Covers * self();
//    static bool isCoverFile(const QString &file);
    static bool copyImage(const QString &sourceDir, const QString &destDir, const QString &coverFile, const QString &destName, unsigned short maxSize=0);
    static bool copyCover(const Song &song, const QString &sourceDir, const QString &destDir, const QString &name=QString(), unsigned short maxSize=0);
    static const QStringList &standardNames();
    static QString encodeName(QString name);
    static QString albumFileName(const Song &song);
    static QString artistFileName(const Song &song);
    static QString fixArtist(const QString &artist);
    static bool isJpg(const QByteArray &data);
    static bool isPng(const QByteArray &data);
    static const char * imageFormat(const QByteArray &data);

    Covers();
    #ifdef ENABLE_UBUNTU
    void setFetchCovers(bool f);
    #else
    void readConfig();
    #endif
    void stop();

    void clearNameCache();
    void clearScaleCache();
    QPixmap * getScaledCover(const Song &song, int size);
    QPixmap * saveScaledCover(const QImage &img, const Song &song, int size);
    // Get cover image of specified size. If this is not found 0 will be returned, and the cover
    // will be downloaded.
    QPixmap * get(const Song &song, int size, bool urgent=false);
    // Get QImage and filename associated with Song request. If this is not found, then the cover
    // will NOT be downloaded. 'emitResult' controls whether 'cover()/artistImage()' is emitted if
    // a cover is found.
    Image getImage(const Song &song) { return findImage(song, false); }
    // Get QImage and filename associated with Song request. If this is not found, then the cover
    // will be downloaded. If more than 5 covers have been requested in an event-loop iteration, then
    // the cover requests are placed on a queue.
    Image requestImage(const Song &song, bool urgent=false);
    void updateCover(const Song &song, const QImage &img, const QString &file);

    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    void cleanCdda();
    #endif

    static Image locateImage(const Song &song);

Q_SIGNALS:
    void download(const Song &s);
    void locate(const Song &s);
    void load(const Song &song);
    void loaded(const Song &song, int s);
    void cover(const Song &song, const QImage &img, const QString &file);
    void coverUpdated(const Song &song, const QImage &img, const QString &file);
    void artistImage(const Song &song, const QImage &img, const QString &file);
    void coverRetrieved(const Song &song);

private Q_SLOTS:
    void located(const QList<LocatedCover> &covers);
    void loaded(const QList<LoadedCover> &covers);
    void coverDownloaded(const Song &song, const QImage &img, const QString &file);
    void artistImageDownloaded(const Song &song, const QImage &img, const QString &file);

private:
    QPixmap * defaultPix(const Song &song, int size, int origSize);
    void tryToLocate(const Song &song);
    void tryToDownload(const Song &song);
    void tryToLoad(const Song &song);
    Image findImage(const Song &song, bool emitResult);
    bool updateCache(const Song &song, const QImage &img, bool dummyEntriesOnly);
    void gotAlbumCover(const Song &song, const QImage &img, const QString &fileName, bool emitResult=true);
    void gotArtistImage(const Song &song, const QImage &img, const QString &fileName, bool emitResult=true);
    QString getFilename(const Song &s, bool isArtist);

private:
    QSet<QString> currentImageRequests;
    QList<Song> queue;
    QSet<int> cacheSizes;
    QCache<QString, QPixmap> cache;
    QMap<QString, QString> filenames;
    CoverDownloader *downloader;
    CoverLocator *locator;
    CoverLoader *loader;
    QMutex mutex;
};

#endif
