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

class Song;
class QImage;
class QString;
class NetworkAccessManager;
class QNetworkReply;
class MaiaXmlRpcClient;

class Covers : public QObject
{
    Q_OBJECT

public:
    struct Job
    {
        Job(const QString &ar, const QString &al, const QString &d, bool l)
            : artist(ar), album(al), dir(d), isLocal(l) { }
        QString artist;
        QString album;
        QString dir;
        bool isLocal;
    };

    static Covers * self();
    static bool isCoverFile(const QString &file);
    static void copyCover(const Song &song, const QString &sourceDir, const QString &destDir, const QString &name=QString());
    static QStringList standardNames();

    Covers();

    void get(const Song &song, bool isSingleTracks=false, bool isLocal=false);
    void setMpdDir(const QString &dir) { mpdDir=dir; }
    QString mpd() const { return mpdDir; }

Q_SIGNALS:
    void cover(const QString &artist, const QString &album, const QImage &img, const QString &file);

private Q_SLOTS:
    void albumInfo(QVariant &value, QNetworkReply *reply);
    void albumFailure(int, const QString &, QNetworkReply *reply);
    void jobFinished(QNetworkReply *reply);

private:
    QString saveImg(const Job &job, const QImage &img, const QByteArray &raw);
    QHash<QNetworkReply *, Job>::Iterator findJob(const Song &song);

private:
    QString mpdDir;
    MaiaXmlRpcClient *rpc;
    NetworkAccessManager *manager;
    QHash<QNetworkReply *, Job> jobs;
};

#endif
