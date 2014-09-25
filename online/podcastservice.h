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

#ifndef PODCAST_SERVICE_H
#define PODCAST_SERVICE_H

#include "onlineservice.h"
#include "network/networkaccessmanager.h"
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

    bool isDownloading() const { return 0!=downloadJob; }
    void cancelAllDownloads();
    void downloadPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes);
    void deleteDownloadedPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes);
    void setPodcastsAsListened(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes, bool listened);

    void cancelAllJobs() { cancelAll(); cancelAllDownloads(); }
    MusicLibraryItemPodcast * getPodcast(const QUrl &url) const;

private:
    void cancelAll();
    MusicLibraryItemPodcastEpisode * getEpisode(const MusicLibraryItemPodcast *podcast, const QUrl &episode);
    void startRssUpdateTimer();
    void stopRssUpdateTimer();
    bool downloadingEpisode(const QUrl &url) const;
    void downloadEpisode(const MusicLibraryItemPodcast *podcast, const QUrl &episode);
    void cancelDownloads(const QList<MusicLibraryItemPodcastEpisode *> episodes);
    void cancelDownload(const QUrl &url);
    void cancelDownload();
    void doNextDownload();
    void updateEpisode(const QUrl &rssUrl, const QUrl &url, int pc);

private Q_SLOTS:
    void loadAll();
    void rssJobFinished();
    void updateRss();
    void currentMpdSong(const Song &s);
    void downloadJobFinished();
    void downloadReadyRead();
    void downloadPercent(int pc);

private:
    struct DownloadEntry {
        DownloadEntry(const QUrl &u=QUrl(), const QUrl &r=QUrl(), const QString &d=QString()) : url(u), rssUrl(r), dest(d) { }
        bool operator==(const DownloadEntry &o) const { return o.url==url; }
        QUrl url;
        QUrl rssUrl;
        QString dest;
        MusicLibraryItemPodcastEpisode *ep;
    };

    QList<NetworkJob *> rssJobs;
    NetworkJob * downloadJob;
    QList<DownloadEntry> toDownload;
    QTimer *rssUpdateTimer;
    QDateTime lastRssUpdate;
    QTimer *deleteTimer;
    QDateTime lastDelete;
    QSet<QUrl> updateUrls;
    static QString iconFile;
};

#endif

