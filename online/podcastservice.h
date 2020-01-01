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

#ifndef PODCAST_SERVICE_H
#define PODCAST_SERVICE_H

#include "onlineservice.h"
#include "models/actionmodel.h"
#include "models/proxymodel.h"
#include "mpd-interface/song.h"
#include <QLatin1String>
#include <QList>
#include <QDateTime>
#include <QSet>
#include <QUrl>

class QTimer;
class NetworkJob;

class PodcastService : public ActionModel, public OnlineService
{
    Q_OBJECT

public:
    struct Item
    {
        Item(const QString &n=QString(), const QUrl &u=QUrl()) : name(n), url(u) { }
        virtual ~Item() { }
        virtual bool isPodcast() const { return false; }
        QString name;
        QString descr;
        QUrl url;
    };

    struct Podcast;
    struct Episode : public Item
    {
        enum DownloadState {
            NotDownloading = -1,
            QueuedForDownload = -2
        };

        Episode(const QDateTime &d=QDateTime(), const QString &n=QString(), const QUrl &u=QUrl(), Podcast *p=nullptr)
            : Item(n, u), played(false), duration(0), publishedDate(d), parent(p), downloadProg(NotDownloading) { }
        ~Episode() override { }
        Song toSong() const;
        bool played;
        int duration;
        QDateTime publishedDate;
        Podcast *parent;
        QString localFile;
        int downloadProg;
    };

    struct Podcast : public Item
    {
        Podcast(const QString &f=QString());
        ~Podcast() override { qDeleteAll(episodes); }
        bool isPodcast() const override { return true; }
        bool load();
        bool save() const;
        void add(Episode *ep);
        void add(QList<Episode *> &eps);
        Episode * getEpisode(const QUrl &epUrl) const;
        void setUnplayedCount();
        void removeFiles();
        const Song & coverSong();

        QList<Episode *> episodes;
        int unplayedCount;
        QString fileName;
        QString imageFile;
        QUrl imageUrl;
        Song song;
    };

    class Proxy : public ProxyModel
    {
    public:
        Proxy(QObject *parent);

        void showUnplayedOnly(bool on);

    private:
        bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
        bool filterAcceptsPodcast(const Podcast *pod) const;
        bool filterAcceptsEpisode(const Episode *item) const;
        bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

    private:
        bool unplayedOnly;
    };

    static const QLatin1String constName;

    static PodcastService * self();

    PodcastService();
    ~PodcastService() override { cancelAll(); }

    Song & fixPath(Song &song) const;
    QString name() const override;
    QString title() const override;
    QString descr() const override;
    bool episodeCover(const Song &s, QImage &img, QString &imgFilename) const;
    QString episodeDescr(const Song &s) const;
    int rowCount(const QModelIndex &index = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override { Q_UNUSED(parent) return 1; }
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QModelIndex index(int row, int col, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &, int) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QMimeData * mimeData(const QModelIndexList &indexes) const override;
    QStringList filenames(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QList<Song> songs(const QModelIndexList &indexes, bool allowPlaylists=false) const;

    int podcastCount() const { return podcasts.count(); }
    void clear();
    void configure(QWidget *p);
    bool subscribedToUrl(const QUrl &url) { return nullptr!=getPodcast(url); }
    void unSubscribe(Podcast *podcast);
    void refresh(const QModelIndexList &list);
    void refreshAll();
    void refreshSubscription(Podcast *item);
    bool processingUrl(const QUrl &url) const;
    void addUrl(const QUrl &url, bool isNew=true);
    static bool isPodcastFile(const QString &file);
    static const QString & iconPath() { return iconFile; }
    static QUrl fixUrl(const QString &url);
    static QUrl fixUrl(const QUrl &orig);
    static bool isUrlOk(const QUrl &u) { return QLatin1String("http")==u.scheme() || QLatin1String("https")==u.scheme(); }

    bool isDownloading() const { return nullptr!=downloadJob; }
    void cancelAllDownloads();
    void downloadPodcasts(Podcast *pod, const QList<Episode *> &episodes);
    void deleteDownloadedPodcasts(Podcast *pod, const QList<Episode *> &episodes);
    void setPodcastsAsListened(Podcast *pod, const QList<Episode *> &episodes, bool listened);
    void cancelAllJobs() { cancelAll(); cancelAllDownloads(); }
    Podcast * getPodcast(const QUrl &url) const;
    void cancelAll();
    void startRssUpdateTimer();
    void stopRssUpdateTimer();
    bool exportSubscriptions(const QString &name);

Q_SIGNALS:
    void error(const QString &msg);
    void newError(const QString &msg);

private:
    bool downloadingEpisode(const QUrl &url) const;
    void downloadEpisode(const Podcast *podcast, const QUrl &episode);
    void cancelDownloads(const QList<Episode *> episodes);
    void cancelDownload(const QUrl &url);
    void cancelDownload();
    void doNextDownload();
    void updateEpisode(const QUrl &rssUrl, const QUrl &url, int pc);
    void clearPartialDownloads();

private Q_SLOTS:
    void loadAll();
    void rssJobFinished();
    void updateRss();
    void currentMpdSong(const Song &s);
    void downloadJobFinished();
    void downloadReadyRead();
    void downloadPercent(int pc);

private:
    struct DownloadEntry
    {
        DownloadEntry(const QUrl &u=QUrl(), const QUrl &r=QUrl(), const QString &d=QString()) : url(u), rssUrl(r), dest(d) { }
        bool operator==(const DownloadEntry &o) const { return o.url==url; }
        QUrl url;
        QUrl rssUrl;
        QString dest;
        Episode *ep;
    };

    QList<Podcast *> podcasts;
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

