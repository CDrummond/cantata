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

class NetworkAccessManager;
class MaiaXmlRpcClient;
class QString;
class QThread;
class QNetworkReply;
class QMutex;

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
    void cover(const QString &artist, const QString &album, const QImage &img, const QString &file);
    void download(const Song &song);

private:
    QMutex *mutex;
    QList<Song> songs;
};

class Covers : public QObject
{
    Q_OBJECT

public:
    struct Job
    {
        Job(const QString &ar, const QString &al, const QString &d)
            : artist(ar), album(al), dir(d) { }
        QString artist;
        QString album;
        QString dir;
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

    static Covers * self();
    static bool isCoverFile(const QString &file);
    static void copyCover(const Song &song, const QString &sourceDir, const QString &destDir, const QString &name=QString());
    static QStringList standardNames();

    Covers();
    void stop();

    QPixmap * get(const Song &song, int size, bool isSingleTracks=false);
    Image getImage(const Song &song, bool isSingleTracks=false);
    Image get(const Song &song, bool isSingleTracks=false);
    void requestCover(const Song &song, bool urgent=false);
    void setSaveInMpdDir(bool s);

public Q_SLOTS:
    void download(const Song &song);

Q_SIGNALS:
    void cover(const QString &artist, const QString &album, const QImage &img, const QString &file);
    void coverRetrieved(const QString &artist, const QString &album);

private Q_SLOTS:
    void albumInfo(QVariant &value, QNetworkReply *reply);
    void albumFailure(int, const QString &, QNetworkReply *reply);
    void jobFinished(QNetworkReply *reply);

private:
    QString saveImg(const Job &job, const QImage &img, const QByteArray &raw);
    QHash<QNetworkReply *, Job>::Iterator findJob(const Song &song);
    void clearDummyCache(const QString &artist, const QString &album);

private:
    MaiaXmlRpcClient *rpc;
    NetworkAccessManager *manager;
    QHash<QNetworkReply *, Job> jobs;
    QCache<QString, QPixmap> cache;
    CoverQueue *queue;
    QThread *queueThread;
};

#endif
