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

#ifndef WEBSTREAMS_H
#define WEBSTREAMS_H

#include "streamsmodel.h"
#include <QUrl>
#include <QObject>

class QNetworkReply;
class QNetworkRequest;

class WebStream : public QObject
{
    Q_OBJECT
public:
    static QList<WebStream *> getAll();
    static WebStream * get(const QUrl &url);

    WebStream(const QString &n, const QString &i, const QString &r, const QUrl &u)
        : name(n),icon(i),  region(r), url(u), job(0) { }
    virtual ~WebStream() { }

    virtual QList<StreamsModel::StreamItem *> parse(QIODevice *dev)=0;

    const QString & getName() const { return name; }
    const QString & getIcon() const { return icon; }
    const QString & getRegion() const { return region; }
    const QUrl & getUrl() const { return url; }
    bool isDownloading() const { return 0!=job; }
    void download();
    void cancelDownload();
//    virtual QUrl modifyUrl(const QUrl &u) const { return u; }

Q_SIGNALS:
    void finished();
    void error(const QString &);

private Q_SLOTS:
    void downloadFinished();

private:
    virtual QUrl channelListUrl() const { return url; }
    virtual void addHeaders(QNetworkRequest &) { }

protected:
    QString name;
    QString icon;
    QString region;
    QUrl url;
    QNetworkReply *job;
};

class IceCastWebStream : public WebStream
{
public:
    IceCastWebStream(const QString &n, const QString &i, const QString &r, const QUrl &u)
        : WebStream(n, i, r, u) { }
    QList<StreamsModel::StreamItem *> parse(QIODevice *dev);
};

class SomaFmWebStream : public WebStream
{
public:
    SomaFmWebStream(const QString &n, const QString &i, const QString &r, const QUrl &u)
        : WebStream(n, i, r, u) { }
    QList<StreamsModel::StreamItem *> parse(QIODevice *dev);
};

class RadioWebStream : public WebStream
{
public:
    RadioWebStream(const QString &n, const QString &i, const QString &r, const QUrl &u)
        : WebStream(n, i, r, u) { }
    QList<StreamsModel::StreamItem *> parse(QIODevice *dev);
};

class DigitallyImportedWebStream: public WebStream
{
public:
    DigitallyImportedWebStream(const QString &n, const QString &i, const QString &r, const QUrl &u);
    QList<StreamsModel::StreamItem *> parse(QIODevice *dev);

private:
    QUrl channelListUrl() const;
    void addHeaders(QNetworkRequest &r);
//    QUrl modifyUrl(const QUrl &u) const;

private:
    QString listenHost;
    QString serviceName;
//    QString premiumHash;
//    int premiumStream;
};

#endif
