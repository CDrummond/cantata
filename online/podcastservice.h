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

#ifndef PODCAST_SERVICE_H
#define PODCAST_SERVICE_H

#include "onlineservice.h"
#include "networkaccessmanager.h"
#include <QLatin1String>
#include <QList>
#include <QDateTime>
#include <QSet>
#include <QUrl>

class QTimer;
class MusicLibraryItemPodcast;
class MusicLibraryItemPodcastEpisode;

class PodcastService : public OnlineService
{
    Q_OBJECT

public:
    static const QString constName;

    PodcastService(MusicModel *m);
    ~PodcastService() { cancelAll(); }

    Song fixPath(const Song &orig, bool) const;
    void loadConfig() { }
    void saveConfig() { }
    void createLoader() { }
    bool canConfigure() const { return true; }
    bool canSubscribe() const { return true; }
    bool canLoad() const { return false; }
    bool isPodcasts() const { return true; }
    void clear();
    const QString & id() const { return constName; }
    void configure(QWidget *p);
    bool subscribedToUrl(const QUrl &url) { return 0!=getPodcast(url); }
    void unSubscribe(MusicLibraryItem *item);
    void refreshSubscription(MusicLibraryItem *item);
    bool processingUrl(const QUrl &url) const;
    void addUrl(const QUrl &url, bool isNew=true);
    static const QString & iconPath() { return iconFile; }
    static QUrl fixUrl(const QString &url);
    static QUrl fixUrl(const QUrl &orig);
    static bool isUrlOk(const QUrl &u) { return QLatin1String("http")==u.scheme() || QLatin1String("https")==u.scheme(); }

    bool isDownloading() const { return !downloadJobs.isEmpty(); }
    void cancelAllDownloads();
    void downloadPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes);
    void deleteDownloadedPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes);

    void cancelAllJobs() { cancelAll(); cancelAllDownloads(); }

private:
    void loadAll();
    void cancelAll();
    MusicLibraryItemPodcast * getPodcast(const QUrl &url) const;
    MusicLibraryItemPodcastEpisode * getEpisode(const MusicLibraryItemPodcast *podcast, const QUrl &episode);
    void startRssUpdateTimer();
    void stopRssUpdateTimer();
    bool downloadingEpisode(const QUrl &url) const;
    void downloadEpisode(const MusicLibraryItemPodcast *podcast, const QUrl &episode);
    void cancelDownload(const QUrl &url);
    void cancelDownload(NetworkJob *job);

private Q_SLOTS:
    void rssJobFinished();
    void updateRss();
    void currentMpdSong(const Song &s);
    void downloadJobFinished();
    void downloadReadyRead();
    void downloadPercent(int pc);

private:
    QList<NetworkJob *> rssJobs;
    QList<NetworkJob *> downloadJobs;
    QTimer *rssUpdateTimer;
    QDateTime lastRssUpdate;
    QTimer *deleteTimer;
    QDateTime lastDelete;
    QSet<QUrl> updateUrls;
    static QString iconFile;
};

#endif

