/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QCache>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include "song.h"
#include "config.h"

class NetworkAccessManager;
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
    void artistImage(const QString &artist, const QImage &img);
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
        JobLastFm
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
        Image(const QImage &i, const QString &f)
            : img(i)
            , fileName(f) {
        }

        QImage img;
        QString fileName;
    };

    static const QSize constMaxSize;

    static Covers * self();
    static bool isCoverFile(const QString &file);
    static bool copyCover(const Song &song, const QString &sourceDir, const QString &destDir, const QString &name=QString(), unsigned short maxSize=0);
    static QStringList standardNames();

    Covers();
    void stop();

    QPixmap * get(const Song &song, int size);
    static Image getImage(const Song &song);
    Image get(const Song &song);
    void requestCover(const Song &song, bool urgent=false);
    void setSaveInMpdDir(bool s);

public Q_SLOTS:
    void download(const Song &song);

Q_SIGNALS:
    void cover(const Song &song, const QImage &img, const QString &file);
    void artistImage(const QString &artist, const QImage &img);
    void coverRetrieved(const Song &song);

private:
    void downloadViaHttp(Job &job, JobType type);
    void downloadViaLastFm(Job &job);

private Q_SLOTS:
    void lastFmCallFinished();
    void jobFinished();

private:
    QString saveImg(const Job &job, const QImage &img, const QByteArray &raw);
    QHash<QNetworkReply *, Job>::Iterator findJob(const Job &job);
    void clearDummyCache(const Song &song, const QImage &img);

private:
    NetworkAccessManager *manager;
    QHash<QNetworkReply *, Job> jobs;
    QCache<QString, QPixmap> cache;
    CoverQueue *queue;
    QThread *queueThread;
};

#endif
