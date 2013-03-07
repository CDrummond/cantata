/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QCache>
#include <QImage>
#include <QPixmap>
#include "song.h"
#include "config.h"

class QString;
class QThread;
class QNetworkReply;
class QMutex;
#ifdef ENABLE_KDE_SUPPORT
class KUrl;
#endif

class CoverQueue : public QObject
{
    Q_OBJECT

public:
    CoverQueue();

    ~CoverQueue() {
    }

    void getCover(const Song &s, bool urgent);

private Q_SLOTS:
    void getNextCover();

Q_SIGNALS:
    void getNext();
    void cover(const Song &song, const QImage &img, const QString &file);
    void artistImage(const Song &song, const QImage &img);
    void download(const Song &song);

private:
    QMutex *mutex;
    QList<Song> songs;
};

class Covers : public QObject
{
    Q_OBJECT

public:
    enum JobType {
        JobHttpJpg,
        JobHttpPng,
        JobLastFm,

        JobOnline
    };

    struct Job
    {
        Job(const Song &s, const QString &d, bool a=false)
            : song(s), dir(d), isArtist(a), type(JobLastFm) { }
        Song song;
        QString dir;
        bool isArtist;
        JobType type;
    };

    struct Image
    {
        Image(const QImage &i=QImage(), const QString &f=QString())
            : img(i)
            , fileName(f) {
        }

        QImage img;
        QString fileName;
    };

    static const QSize constMaxSize;
    static const QLatin1String constLastFmApiKey;
    static const QLatin1String constCoverDir;
    static const QLatin1String constFileName;
    static const QLatin1String constCddaCoverDir;

    static Covers * self();
    static bool isCoverFile(const QString &file);
    static bool copyImage(const QString &sourceDir, const QString &destDir, const QString &coverFile, const QString &destName, unsigned short maxSize=0);
    static bool copyCover(const Song &song, const QString &sourceDir, const QString &destDir, const QString &name=QString(), unsigned short maxSize=0);
    static QStringList standardNames();
    static QString encodeName(QString name);

    Covers();
    void stop();

    QPixmap * get(const Song &song, int size);
    Image getImage(const Song &song);
    Image get(const Song &song);
    void requestCover(const Song &song, bool urgent=false);
    void setSaveInMpdDir(bool s);
    void emitCoverUpdated(const Song &song, const QImage &img, const QString &file);

    #ifdef CDDB_FOUND
    void cleanCdda();
    #endif

public Q_SLOTS:
    void download(const Song &song);

Q_SIGNALS:
    void cover(const Song &song, const QImage &img, const QString &file);
    void coverUpdated(const Song &song, const QImage &img, const QString &file);
    void artistImage(const Song &song, const QImage &img);
    void coverRetrieved(const Song &song);

private:
    void donwloadOnlineImage(Job &job);
    void downloadViaHttp(Job &job, JobType type);
    void downloadViaLastFm(Job &job);

private Q_SLOTS:
    void lastFmCallFinished();
    void jobFinished();

private:
    QString saveImg(const Job &job, const QImage &img, const QByteArray &raw);
    QHash<QNetworkReply *, Job>::Iterator findJob(const Job &job);
    void clearCache(const Song &song, const QImage &img, bool dummyEntriesOnly);
    void gotAlbumCover(const Song &song, const Image &img, bool emitResult=true);
    void gotArtistImage(const Song &song, const Image &img, bool emitResult=true);
    QString getFilename(const Song &s, bool isArtist);

private:
    QHash<QNetworkReply *, Job> jobs;
    QSet<int> cacheSizes;
    QCache<quint32, QPixmap> cache;
    QMap<QString, QString> filenames;
    CoverQueue *queue;
    QThread *queueThread;
};

#endif
