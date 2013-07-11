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

#ifndef ONLINE_SERVICE_H
#define ONLINE_SERVICE_H

#include "musiclibraryitemroot.h"
#include "song.h"
#include "icons.h"
#include "localize.h"
#include <QObject>
#include <QUrl>

class Thread;
class NetworkAccessManager;
class OnlineServicesModel;
class QNetworkReply;
class QXmlStreamReader;

class OnlineMusicLoader : public QObject, public MusicLibraryProgressMonitor
{
    Q_OBJECT

public:
    OnlineMusicLoader(const QUrl &src);

    void start();
    void stop();
    bool wasStopped() const { return stopRequested; }
    MusicLibraryItemRoot * takeLibrary();
    virtual bool parse(QXmlStreamReader &xml)=0;
    void setCacheFileName(const QString &c) { cache=c; }

Q_SIGNALS:
    void load();
    void status(const QString &msg, int prog);
    void error(const QString &msg);
    void loaded();

private Q_SLOTS:
    void doLoad();
    void downloadFinished();
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    bool readFromCache();
    void fixLibrary();
    void readProgress(double pc);
    void writeProgress(double pc);
    void progressReport(const QString &str, int pc);

protected:
    Thread *thread;
    QUrl source;
    QString cache;
    MusicLibraryItemRoot *library;
    NetworkAccessManager *network;
    QNetworkReply *downloadJob;
    bool stopRequested;
    int lastProg;
};

// MOC requires the QObject class to be first. But due to models storing void pointers, and
// needing to cast these - the model prefers MusicLibraryItemRoot to be first!
#ifdef Q_MOC_RUN
class OnlineService : public QObject, public MusicLibraryItemRoot
#else
class OnlineService : public MusicLibraryItemRoot, public QObject
#endif
{
    Q_OBJECT

public:
    OnlineService(OnlineServicesModel *m, const QString &name)
        : MusicLibraryItemRoot(name, false)
        , model(m)
        , configured(false)
        , update(0)
        , lProgress(0.0)
        , loaded(false)
        , loader(0)    {
        setUseArtistImages(true);
        setUseAlbumImages(true);
    }
    virtual ~OnlineService() { }
    void destroy(bool delCache=true);
    void stopLoader();
    virtual const Icon & serviceIcon() const =0;
    virtual Song fixPath(const Song &orig) const =0;
    virtual void createLoader()=0;
    virtual void loadConfig()=0;
    virtual void saveConfig()=0;
    virtual void configure(QWidget *) { }
    virtual bool canDownload() const { return false; }
    const QString name() const { return data(); }
    double loadProgress() { return lProgress; }
    bool isLoaded() const { return loaded; }
    void reload(bool fromCache=true);
    void toggle();
    void clear();
    bool isLoading() const { return 0!=loader; }
    bool isIdle() const { return isLoaded() && !isLoading(); }
    void removeCache();
    void applyUpdate();
    bool haveUpdate() const { return 0!=update; }
    int newRows() const { return update ? update->childCount() : 0; }
    const QString & statusMessage() const { return statusMsg; }
    const MusicLibraryItem * findSong(const Song &s) const;
    bool songExists(const Song &s) const;
    bool isConfigured() { return configured; }

private Q_SLOTS:
    void loaderError(const QString &msg);
    void loaderstatus(const QString &msg, int prog);
    void loaderFinished();

private:
    void setStatusMessage(const QString &msg);

Q_SIGNALS:
    void actionStatus(int status, bool copiedCover=false);
    void error(const QString &);

protected:
    OnlineServicesModel *model;
    bool configured;
    MusicLibraryItemRoot *update;
    QString statusMsg;
    double lProgress;
    bool loaded;
    OnlineMusicLoader *loader;
};

#endif
