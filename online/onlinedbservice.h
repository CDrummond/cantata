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

#ifndef ONLINE_DB_SERVICE_H
#define ONLINE_DB_SERVICE_H

#include "onlineservice.h"
#include "models/sqllibrarymodel.h"

class NetworkJob;
class Thread;
class QXmlStreamReader;
struct Song;

class OnlineXmlParser : public QObject
{
    Q_OBJECT
public:
    OnlineXmlParser();
    ~OnlineXmlParser() override;
    void start(NetworkJob *job);
    virtual int parse(QXmlStreamReader &xml) = 0;
Q_SIGNALS:
    void songs(QList<Song> *s);
    void coverUrl(const QString &artist, const QString &album, const QString &cover);
    void startUpdate();
    void endUpdate();
    void abortUpdate();
    void stats(int numArtists);
    void complete();
    void error(const QString &msg);
    void startParsing(NetworkJob *job);

private Q_SLOTS:
    void doParsing(NetworkJob *job);

private:
    Thread *thread;
};

class OnlineDbService : public SqlLibraryModel, public OnlineService
{
    Q_OBJECT
public:
    OnlineDbService(LibraryDb *d, QObject *p);
    ~OnlineDbService() override { }

    void createDb();
    QVariant data(const QModelIndex &index, int role) const override;
    bool previouslyDownloaded() const;
    bool isDownloading() { return nullptr!=job; }
    void open();
    void download(bool redownload);
    virtual OnlineXmlParser * createParser() = 0;
    virtual QUrl listingUrl() const = 0;
    virtual void configure(QWidget *p) =0;
    virtual int averageSize() const = 0;

public Q_SLOTS:
    void abort();

Q_SIGNALS:
    void error(const QString &msg);

private Q_SLOTS:
    void cover(const Song &song, const QImage &img, const QString &file);
    void updateStatus(const QString &msg);
    void downloadPercent(int pc);
    void downloadFinished();
    void updateStats();

protected:
    int lastPc;
    QString status;
    QString stats;
    NetworkJob *job;
};

#endif
