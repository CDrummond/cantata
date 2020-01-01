/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef NETWORK_ACCESS_MANAGER_H
#define NETWORK_ACCESS_MANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>

class QTimerEvent;
class NetworkAccessManager;

class NetworkJob : public QObject
{
    Q_OBJECT

public:
    NetworkJob(QNetworkReply *j);
    ~NetworkJob() override;

    QNetworkReply * actualJob() const { return job; }

    void cancelAndDelete();
    bool open(QIODevice::OpenMode mode) { return job && job->open(mode); }
    void close() { if (job) job->close(); }

    QUrl url() const { return job ? job->url() : origU; }
    QUrl origUrl() const { return origU; }
    void setOrigUrl(const QUrl &u) { origU=u; }
    QNetworkReply::NetworkError error() const { return job ? job->error() : QNetworkReply::UnknownNetworkError; }
    QString errorString() const { return job ? job->errorString() : QString(); }
    QByteArray readAll() { return job ? job->readAll() : QByteArray(); }
    bool ok() const { return job && QNetworkReply::NoError==job->error(); }
    QVariant attribute(QNetworkRequest::Attribute code) const { return job ? job->attribute(code) : QVariant(); }
    qint64 bytesAvailable() const { return job ? job->bytesAvailable() : -1; }
    QByteArray read(qint64 maxlen) { return job ? job->read(maxlen) : QByteArray(); }

Q_SIGNALS:
    void finished();
    void error(QNetworkReply::NetworkError);
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadPercent(int pc);
    void readyRead();

private Q_SLOTS:
    void jobFinished();
    void jobDestroyed(QObject *o);
    void downloadProg(qint64 bytesReceived, qint64 bytesTotal);
    void handleReadyRead();

private:
    NetworkJob(NetworkAccessManager *p, const QUrl &u);
    void connectJob();
    void cancelJob();
    void abortJob();

private:
    int numRedirects;
    int lastDownloadPc;
    QNetworkReply *job;
    QUrl origU;

    friend class NetworkAccessManager;
};

class NetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    static void enableDebug();
    static void disableNetworkAccess();
    static NetworkAccessManager * self();

    NetworkAccessManager(QObject *parent=nullptr);
    ~NetworkAccessManager() override { }

    NetworkJob * get(const QNetworkRequest &req, int timeout=0);
    NetworkJob * get(const QUrl &url, int timeout=0) { return get(QNetworkRequest(url), timeout); }
    QNetworkReply * postFormData(QNetworkRequest req, const QByteArray &data);
    QNetworkReply * postFormData(const QUrl &url, const QByteArray &data) { return postFormData(QNetworkRequest(url), data); }

protected:
    void timerEvent(QTimerEvent *e) override;

private Q_SLOTS:
    void replyFinished();

private:
    QMap<NetworkJob *, int> timers;
    friend class NetworkJob;
};

#endif // NETWORK_ACCESS_MANAGER_H
