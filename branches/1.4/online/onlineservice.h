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

#ifndef ONLINE_SERVICE_H
#define ONLINE_SERVICE_H

#include "models/musiclibraryitemroot.h"
#include "mpd/song.h"
#include "support/localize.h"
#include <QObject>
#include <QUrl>
#include <QModelIndex>

class Thread;
class NetworkAccessManager;
class MusicModel;
class NetworkJob;
class QXmlStreamReader;

class OnlineServiceMusicRoot : public MusicLibraryItemRoot
{
public:
    OnlineServiceMusicRoot(const QString &name=QString()) : MusicLibraryItemRoot(name, false) { }
    virtual ~OnlineServiceMusicRoot() { }
    virtual bool isOnlineService() const { return true; }
};

class OnlineMusicLoader : public QObject, public MusicLibraryProgressMonitor
{
    Q_OBJECT

public:
    OnlineMusicLoader(const QUrl &src);

    void start();
    void stop();
    bool wasStopped() const { return stopRequested; }
    OnlineServiceMusicRoot * takeLibrary();
    virtual bool parse(QXmlStreamReader &xml)=0;
    void setCacheFileName(const QString &c) { cache=c; }

protected:
    void setBusy(bool b);

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
    OnlineServiceMusicRoot *library;
    NetworkAccessManager *network;
    NetworkJob *downloadJob;
    bool stopRequested;
    int lastProg;
};

// MOC requires the QObject class to be first. But due to models storing void pointers, and
// needing to cast these - the model prefers MusicLibraryItemRoot to be first!
#ifdef Q_MOC_RUN
class OnlineService : public QObject, public OnlineServiceMusicRoot
#else
class OnlineService : public OnlineServiceMusicRoot, public QObject
#endif
{
    Q_OBJECT

public:
    static Song encode(const Song &song);
    static bool decode(Song &song);

    OnlineService(MusicModel *m, const QString &name);
    virtual ~OnlineService() { }
    void destroy();
    void stopLoader();
    Icon icon() const { return icn; }
    virtual void createLoader()=0;
    virtual void loadConfig()=0;
    virtual void saveConfig()=0;
    virtual void configure(QWidget *) { }
    virtual bool canDownload() const { return false; }
    virtual bool canConfigure() const { return true; }
    virtual bool canSubscribe() const { return false; }
    virtual bool canLoad() const { return true; }
    virtual bool isSearchBased() const { return false; }
    virtual bool isPodcasts() const { return false; }
    virtual QString currentSearchString() const { return QString(); }
    virtual void setSearch(const QString &) { }
    virtual void cancelSearch() { }
    virtual bool isSearching()  const { return false; }
    virtual bool isFlat() const { return false; }
    double loadProgress() { return lProgress; }
    bool isLoaded() const { return loaded; }
    void reload(bool fromCache=true);
    void toggle();
    virtual void clear();
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
    QModelIndex index() const;

protected:
    QModelIndex createIndex(MusicLibraryItem *child) const;
    void emitUpdated();
    void emitError(const QString &msg, bool isPodcastError=false);
    void emitDataChanged(const QModelIndex &idx);
//    void emitNeedToSort();
    void setBusy(bool b);
    void beginInsertRows(const QModelIndex &idx, int from, int to);
    void endInsertRows();
    void beginRemoveRows(const QModelIndex &idx, int from, int to);
    void endRemoveRows();

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
    Icon icn;
    bool configured;
    OnlineServiceMusicRoot *update;
    QString statusMsg;
    double lProgress;
    bool loaded;
    OnlineMusicLoader *loader;
};

#endif
