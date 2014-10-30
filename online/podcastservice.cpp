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

#include "podcastservice.h"
#include "podcastsettingsdialog.h"
#include "models/onlineservicesmodel.h"
#include "models/musiclibraryitempodcast.h"
#include "models/musiclibraryitemsong.h"
#include "support/utils.h"
#include "gui/settings.h"
#include "mpd/mpdconnection.h"
#include "config.h"
#include "http/httpserver.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTimer>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <stdio.h>

const QString PodcastService::constName=QLatin1String("Podcasts");
QString PodcastService::iconFile;

static const char * constNewFeedProperty="new-feed";
static const char * constRssUrlProperty="rss-url";
static const char * constDestProperty="dest";
static const QLatin1String constPartialExt(".partial");

bool PodcastService::isPodcastFile(const QString &file)
{
    if (file.startsWith(Utils::constDirSep) && MPDConnection::self()->getDetails().isLocal()) {
        QString downloadPath=Settings::self()->podcastDownloadPath();
        if (downloadPath.isEmpty()) {
            return false;
        }
        return file.startsWith(downloadPath);
    }
    return false;
}

QUrl PodcastService::fixUrl(const QString &url)
{
    QString trimmed(url.trimmed());

    // Thanks gpodder!
    static QMap<QString, QString> prefixMap;
    if (prefixMap.isEmpty()) {
        prefixMap.insert(QLatin1String("fb:"),    QLatin1String("http://feeds.feedburner.com/%1"));
        prefixMap.insert(QLatin1String("yt:"),    QLatin1String("http://www.youtube.com/rss/user/%1/videos.rss"));
        prefixMap.insert(QLatin1String("sc:"),    QLatin1String("http://soundcloud.com/%1"));
        prefixMap.insert(QLatin1String("fm4od:"), QLatin1String("http://onapp1.orf.at/webcam/fm4/fod/%1.xspf"));
        prefixMap.insert(QLatin1String("ytpl:"),  QLatin1String("http://gdata.youtube.com/feeds/api/playlists/%1"));
    }

    QMap<QString, QString>::ConstIterator it(prefixMap.constBegin());
    QMap<QString, QString>::ConstIterator end(prefixMap.constEnd());
    for (; it!=end; ++it) {
        if (trimmed.startsWith(it.key())) {
            trimmed=it.value().arg(trimmed.mid(it.key().length()));
        }
    }

    if (!trimmed.contains(QLatin1String("://"))) {
        trimmed.prepend(QLatin1String("http://"));
    }

    return fixUrl(QUrl(trimmed));
}

QUrl PodcastService::fixUrl(const QUrl &orig)
{
    QUrl u=orig;
    if (u.scheme().isEmpty() || QLatin1String("itpc")==u.scheme() || QLatin1String("pcast")==u.scheme() ||
        QLatin1String("feed")==u.scheme() || QLatin1String("itms")==u.scheme()) {
        u.setScheme(QLatin1String("http"));
    }
    return u;
}

PodcastService::PodcastService(MusicModel *m)
    : OnlineService(m, i18n("Podcasts"))
    , downloadJob(0)
    , rssUpdateTimer(0)
{
    loaded=true;
    QMetaObject::invokeMethod(this, "loadAll", Qt::QueuedConnection);
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(currentMpdSong(const Song &)));
    if (iconFile.isEmpty()) {
        iconFile=QString(CANTATA_SYS_ICONS_DIR+"podcasts.png");
    }

    clearPartialDownloads();
}

Song PodcastService::fixPath(const Song &orig, bool) const
{
    Song song=orig;
    song.setPodcastLocalPath(QString());
    song.setIsFromOnlineService(constName);
    song.artist=constName;
    if (!orig.podcastLocalPath().isEmpty() && QFile::exists(orig.podcastLocalPath())) {
        if (!HttpServer::self()->forceUsage() && MPDConnection::self()->localFilePlaybackSupported()) {
            song.file=QLatin1String("file://")+orig.podcastLocalPath();
        } else if (HttpServer::self()->isAlive()) {
            song.file=orig.podcastLocalPath();
            song.file=HttpServer::self()->encodeUrl(song);
        }
        return song;
    }
    return encode(song);
}

void PodcastService::clear()
{
    cancelAllJobs();
    ::OnlineService::clear();
}

void PodcastService::loadAll()
{
    QString dir=Utils::dataDir(MusicLibraryItemPodcast::constDir);

    if (!dir.isEmpty()) {
        QDir d(dir);
        QStringList entries=d.entryList(QStringList() << "*"+MusicLibraryItemPodcast::constExt, QDir::Files|QDir::Readable|QDir::NoDot|QDir::NoDotDot);
        foreach (const QString &e, entries) {
            if (!update) {
                update=new OnlineServiceMusicRoot();
            }

            MusicLibraryItemPodcast *podcast=new MusicLibraryItemPodcast(dir+e, update);
            if (podcast->load()) {
                update->append(podcast);
            } else {
                delete podcast;
            }
        }

        if (update) {
            if (update->childItems().isEmpty()) {
                delete update;
            } else {
                applyUpdate();
            }
        }
        startRssUpdateTimer();
    }
}

void PodcastService::cancelAll()
{
    foreach (NetworkJob *j, rssJobs) {
        disconnect(j, SIGNAL(finished()), this, SLOT(rssJobFinished()));
        j->cancelAndDelete();
    }
    rssJobs.clear();
    cancelAllDownloads();
    setBusy(!rssJobs.isEmpty() || 0!=downloadJob);
}

void PodcastService::rssJobFinished()
{
    NetworkJob *j=dynamic_cast<NetworkJob *>(sender());
    if (!j || !rssJobs.contains(j)) {
        return;
    }

    j->deleteLater();
    rssJobs.removeAll(j);
    bool isNew=j->property(constNewFeedProperty).toBool();

    if (j->ok()) {
        if (updateUrls.contains(j->origUrl())){
            updateUrls.remove(j->origUrl());
            if (updateUrls.isEmpty()) {
                lastRssUpdate=QDateTime::currentDateTime();
                Settings::self()->saveLastRssUpdate(lastRssUpdate);
                startRssUpdateTimer();
            }
        }

        MusicLibraryItemPodcast *podcast=new MusicLibraryItemPodcast(QString(), this);
        MusicLibraryItemPodcast::RssStatus loadStatus=podcast->loadRss(j->actualJob());
        if (MusicLibraryItemPodcast::Loaded==loadStatus) {
            int autoDownload=Settings::self()->podcastAutoDownloadLimit();

            if (isNew) {
                podcast->save();
                beginInsertRows(index(), childCount(), childCount());
                m_childItems.append(podcast);
                if (autoDownload) {
                    int ep=0;
                    foreach (MusicLibraryItem *i, podcast->childItems()) {
                        MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                        downloadEpisode(podcast, QUrl(song->file()));
                        if (autoDownload<1000 && ++ep>=autoDownload) {
                            break;
                        }
                    }
                }
                endInsertRows();
//                emitNeedToSort();
            } else {
                MusicLibraryItemPodcast *orig = getPodcast(j->origUrl());
                if (!orig) {
                    delete podcast;
                    return;
                }
                QSet<QString> origSongs;
                QSet<QString> newSongs;
                foreach (MusicLibraryItem *i, orig->childItems()) {
                    MusicLibraryItemPodcastEpisode *episode=static_cast<MusicLibraryItemPodcastEpisode *>(i);
                    origSongs.insert(episode->file());
                }
                foreach (MusicLibraryItem *i, podcast->childItems()) {
                    MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                    newSongs.insert(song->file());
                }

                QSet<QString> added=newSongs-origSongs;
                QSet<QString> removed=origSongs-newSongs;
                if (added.count() || removed.count()) {
                    QModelIndex origIndex=createIndex(orig);
                    if (removed.count()) {
                        foreach (const QString &s, removed) {
                            MusicLibraryItemPodcastEpisode *episode=orig->getEpisode(s);
                            if (episode->localPath().isEmpty() || !QFile::exists(episode->localPath())) {
                                int idx=orig->indexOf(episode);
                                if (-1!=idx) {
                                    beginRemoveRows(origIndex, idx, idx);
                                    orig->remove(idx);
                                    endRemoveRows();
                                }
                            }
                        }
                    }
                    if (added.count()) {
                        QList<MusicLibraryItemPodcastEpisode *> newSongs;
                        foreach (const QString &s, added) {
                            MusicLibraryItemPodcastEpisode *episode=podcast->getEpisode(s);
                            if (episode) {
                                newSongs.append(episode);
                                if (autoDownload) {
                                    downloadEpisode(orig, QUrl(episode->file()));
                                }
                            }
                        }

                        beginInsertRows(origIndex, orig->childCount(), (orig->childCount()+newSongs.count())-1);
                        orig->addAll(newSongs);
                        endInsertRows();
                    }

                    orig->setUnplayedCount();
                    orig->save();
//                    emitNeedToSort();
                }

                delete podcast;
            }

        } else if (isNew) {
            delete podcast;
            if (MusicLibraryItemPodcast::VideoPodcast==loadStatus) {
                emitError(i18n("Cantata only supports audio podcasts! %1 contains only video podcasts.", j->origUrl().toString()), isNew);
            } else {
                emitError(i18n("Failed to parse %1", j->origUrl().toString()), isNew);
            }
        }
    } else {
        emitError(i18n("Failed to download %1", j->origUrl().toString()), isNew);
    }
    setBusy(!rssJobs.isEmpty() || 0!=downloadJob);
}

void PodcastService::configure(QWidget *p)
{
    PodcastSettingsDialog dlg(p);
    if (QDialog::Accepted==dlg.exec()) {
        int changes=dlg.changes();
        if (changes&PodcastSettingsDialog::RssUpdate) {
            startRssUpdateTimer();
        }
    }
}

MusicLibraryItemPodcast * PodcastService::getPodcast(const QUrl &url) const
{
    foreach (MusicLibraryItem *i, m_childItems) {
        if (static_cast<MusicLibraryItemPodcast *>(i)->rssUrl()==url) {
            return static_cast<MusicLibraryItemPodcast *>(i);
        }
    }
    return 0;
}

MusicLibraryItemPodcastEpisode * PodcastService::getEpisode(const MusicLibraryItemPodcast *podcast, const QUrl &episode)
{
    if (podcast) {
        foreach (MusicLibraryItem *i, podcast->childItems()) {
            MusicLibraryItemPodcastEpisode *song=static_cast<MusicLibraryItemPodcastEpisode *>(i);
            if (QUrl(song->file())==episode) {
                return song;
            }
        }
    }

    return 0;
}

void PodcastService::unSubscribe(MusicLibraryItem *item)
{
    int row=m_childItems.indexOf(item);
    if (row>=0) {
        QList<MusicLibraryItemPodcastEpisode *> episodes;
        foreach (MusicLibraryItem *e, static_cast<MusicLibraryItemPodcast *>(item)->childItems()) {
            episodes.append(static_cast<MusicLibraryItemPodcastEpisode *>(e));
        }
        cancelDownloads(episodes);
        beginRemoveRows(index(), row, row);
        static_cast<MusicLibraryItemPodcast *>(item)->removeFiles();
        delete m_childItems.takeAt(row);
        resetRows();
        endRemoveRows();
        if (m_childItems.isEmpty()) {
            stopRssUpdateTimer();
        }
    }
}

void PodcastService::refreshSubscription(MusicLibraryItem *item)
{
    if (item) {
        QUrl url=static_cast<MusicLibraryItemPodcast *>(item)->rssUrl();
        if (processingUrl(url)) {
            return;
        }
        addUrl(url, false);
    } else {
        updateRss();
    }
}

bool PodcastService::processingUrl(const QUrl &url) const
{
    foreach (NetworkJob *j, rssJobs) {
        if (j->origUrl()==url) {
            return true;
        }
    }
    return false;
}

void PodcastService::addUrl(const QUrl &url, bool isNew)
{
    setBusy(true);
    NetworkJob *job=NetworkAccessManager::self()->get(url);
    connect(job, SIGNAL(finished()), this, SLOT(rssJobFinished()));
    job->setProperty(constNewFeedProperty, isNew);
    rssJobs.append(job);
}

bool PodcastService::downloadingEpisode(const QUrl &url) const
{
    if (downloadJob && downloadJob->origUrl()==url) {
        return true;
    }
    return toDownload.contains(url);
}

void PodcastService::cancelAllDownloads()
{
    foreach (const DownloadEntry &e, toDownload) {
        updateEpisode(e.rssUrl, e.url, MusicLibraryItemPodcastEpisode::NotDownloading);
    }

    toDownload.clear();
    cancelDownload();
    setBusy(!rssJobs.isEmpty() || 0!=downloadJob);
}

void PodcastService::downloadPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes)
{
    foreach (MusicLibraryItemPodcastEpisode *ep, episodes) {
        downloadEpisode(pod, QUrl(ep->file()));
    }
}

void PodcastService::deleteDownloadedPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes)
{
    cancelDownloads(episodes);
    bool modified=false;
    foreach (MusicLibraryItemPodcastEpisode *ep, episodes) {
        QString fileName=ep->localPath();
        if (!fileName.isEmpty()) {
            if (QFile::exists(fileName)) {
                QFile::remove(fileName);
            }
            QString dirName=fileName.isEmpty() ? QString() : Utils::getDir(fileName);
            if (!dirName.isEmpty()) {
                QDir dir(dirName);
                if (dir.exists()) {
                    dir.rmdir(dirName);
                }
            }
            ep->setLocalPath(QString());
            ep->setDownloadProgress(-1);
            emitDataChanged(createIndex(ep));
            modified=true;
        }
    }
    if (modified) {
        emitDataChanged(createIndex(pod));
        pod->save();
    }
}

void PodcastService::setPodcastsAsListened(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes, bool listened)
{
    bool modified=false;
    foreach (MusicLibraryItemPodcastEpisode *ep, episodes) {
        MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(ep);
        if (listened!=song->song().hasBeenPlayed()) {
            pod->setPlayed(song, listened);
            emitDataChanged(createIndex(song));
            modified=true;
        }
    }
    if (modified) {
        emitDataChanged(createIndex(pod));
        pod->save();
    }
}

static QString encodeName(const QString &name)
{
    QString n=name;
    n=n.replace("/", "_");
    n=n.replace("\\", "_");
    n=n.replace(":", "_");
    return n;
}

void PodcastService::downloadEpisode(const MusicLibraryItemPodcast *podcast, const QUrl &episode)
{
    QString dest=Settings::self()->podcastDownloadPath();
    if (dest.isEmpty()) {
        return;
    }
    if (downloadingEpisode(episode)) {
        return;
    }
    dest=Utils::fixPath(dest)+Utils::fixPath(encodeName(podcast->data()))+Utils::getFile(episode.toString());
    toDownload.append(DownloadEntry(episode, podcast->rssUrl(), dest));
    updateEpisode(podcast->rssUrl(), episode, MusicLibraryItemPodcastEpisode::QueuedForDownload);
    doNextDownload();
}

void PodcastService::cancelDownloads(const QList<MusicLibraryItemPodcastEpisode *> episodes)
{
    bool cancelDl=false;
    foreach (MusicLibraryItemPodcastEpisode *e, episodes) {
        QUrl u(e->file());
        toDownload.removeAll(u);
        e->setDownloadProgress(MusicLibraryItemPodcastEpisode::NotDownloading);
        emitDataChanged(createIndex(e));
        if (!cancelDl && downloadJob && downloadJob->url()==u) {
            cancelDl=true;
        }
    }
    if (cancelDl) {
        cancelDownload();
    }
}

void PodcastService::cancelDownload(const QUrl &url)
{
    if (downloadJob && downloadJob->origUrl()==url) {
        cancelDownload();
        doNextDownload();
    }
}

void PodcastService::cancelDownload()
{
    if (downloadJob) {
        downloadJob->cancelAndDelete();
        disconnect(downloadJob, SIGNAL(finished()), this, SLOT(downloadJobFinished()));
        disconnect(downloadJob, SIGNAL(readyRead()), this, SLOT(downloadReadyRead()));
        disconnect(downloadJob, SIGNAL(downloadPercent(int)), this, SLOT(downloadPercent(int)));

        QString dest=downloadJob->property(constDestProperty).toString();
        QString partial=dest.isEmpty() ? QString() : QString(dest+constPartialExt);
        if (!partial.isEmpty() && QFile::exists(partial)) {
            QFile::remove(partial);
        }
        updateEpisode(downloadJob->property(constRssUrlProperty).toUrl(), downloadJob->origUrl(), MusicLibraryItemPodcastEpisode::NotDownloading);
        downloadJob=0;
    }
    setBusy(!rssJobs.isEmpty() || 0!=downloadJob);
}

void PodcastService::doNextDownload()
{
    if (downloadJob) {
        return;
    }

    if (toDownload.isEmpty()) {
        setBusy(!rssJobs.isEmpty());
        return;
    }

    DownloadEntry entry=toDownload.takeFirst();
    downloadJob=NetworkAccessManager::self()->get(entry.url);
    connect(downloadJob, SIGNAL(finished()), this, SLOT(downloadJobFinished()));
    connect(downloadJob, SIGNAL(readyRead()), this, SLOT(downloadReadyRead()));
    connect(downloadJob, SIGNAL(downloadPercent(int)), this, SLOT(downloadPercent(int)));
    downloadJob->setProperty(constRssUrlProperty, entry.rssUrl);
    downloadJob->setProperty(constDestProperty, entry.dest);
    updateEpisode(entry.rssUrl, entry.url, 0);

    QString partial=entry.dest+constPartialExt;
    if (QFile::exists(partial)) {
        QFile::remove(partial);
    }
}

void PodcastService::updateEpisode(const QUrl &rssUrl, const QUrl &url, int pc)
{
    MusicLibraryItemPodcast *pod=getPodcast(rssUrl);
    if (pod) {
        MusicLibraryItemPodcastEpisode *song=getEpisode(pod, url);
        if (song && song->downloadProgress()!=pc) {
            song->setDownloadProgress(pc);
            emitDataChanged(createIndex(song));
        }
    }
}

void PodcastService::clearPartialDownloads()
{
    QString dest=Settings::self()->podcastDownloadPath();
    if (dest.isEmpty()) {
        return;
    }

    dest=Utils::fixPath(dest);
    QStringList sub=QDir(dest).entryList(QDir::Dirs|QDir::NoDotAndDotDot);
    foreach (const QString &d, sub) {
        QStringList partials=QDir(dest+d).entryList(QStringList() << QLatin1Char('*')+constPartialExt, QDir::Files);
        foreach (const QString &p, partials) {
            QFile::remove(dest+d+Utils::constDirSep+p);
        }
    }
}

void PodcastService::downloadJobFinished()
{
    NetworkJob *job=dynamic_cast<NetworkJob *>(sender());
    if (!job || job!=downloadJob) {
        return;
    }
    job->deleteLater();

    QString dest=job->property(constDestProperty).toString();
    QString partial=dest.isEmpty() ? QString() : QString(dest+constPartialExt);

    if (job->ok()) {
        QString dest=job->property(constDestProperty).toString();
        if (dest.isEmpty()) {
            return;
        }

        QString partial=dest+constPartialExt;
        if (QFile::exists(partial)) {
            if (QFile::exists(dest)) {
                QFile::remove(dest);
            }
            if (QFile::rename(partial, dest)) {
                MusicLibraryItemPodcast *pod=getPodcast(job->property(constRssUrlProperty).toUrl());
                if (pod) {
                    MusicLibraryItemPodcastEpisode *song=getEpisode(pod, job->origUrl());
                    if (song) {
                        song->setLocalPath(dest);
                        pod->save();
                        emitDataChanged(createIndex(song));
                    }
                }
            }
        }
    } else if (!partial.isEmpty() && QFile::exists(partial)) {
        QFile::remove(partial);
    }
    updateEpisode(job->property(constRssUrlProperty).toUrl(), job->origUrl(), MusicLibraryItemPodcastEpisode::NotDownloading);
    downloadJob=0;
    doNextDownload();
}

void PodcastService::downloadReadyRead()
{
    NetworkJob *job=dynamic_cast<NetworkJob *>(sender());
    if (!job || job!=downloadJob) {
        return;
    }
    QString dest=job->property(constDestProperty).toString();
    QString partial=dest.isEmpty() ? QString() : QString(dest+constPartialExt);
    if (!partial.isEmpty()) {
        QString dir=Utils::getDir(partial);
        if (!QDir(dir).exists()) {
            QDir(dir).mkpath(dir);
        }
        if (!QDir(dir).exists()) {
            return;
        }
        QFile f(partial);
        while (true) {
            const qint64 bytes = job->bytesAvailable();
            if (bytes <= 0) {
                break;
            }
            if (!f.isOpen()) {
                if (!f.open(QIODevice::Append)) {
                    return;
                }
            }
            f.write(job->read(bytes));
        }
    }
}

void PodcastService::downloadPercent(int pc)
{
    NetworkJob *job=dynamic_cast<NetworkJob *>(sender());
    if (!job || job!=downloadJob) {
        return;
    }
    updateEpisode(job->property(constRssUrlProperty).toUrl(), job->origUrl(), pc);
}

void PodcastService::startRssUpdateTimer()
{
    if (0==Settings::self()->rssUpdate() || m_childItems.isEmpty()) {
        stopRssUpdateTimer();
        return;
    }
    if (!rssUpdateTimer) {
        rssUpdateTimer=new QTimer(this);
        rssUpdateTimer->setSingleShot(true);
        connect(rssUpdateTimer, SIGNAL(timeout()), this, SLOT(updateRss()));
    }
    if (!lastRssUpdate.isValid()) {
        lastRssUpdate=Settings::self()->lastRssUpdate();
    }
    if (!lastRssUpdate.isValid()) {
        updateRss();
    } else {
        QDateTime nextUpdate = lastRssUpdate.addSecs(Settings::self()->rssUpdate()*60);
        int secsUntilNextUpdate = QDateTime::currentDateTime().secsTo(nextUpdate);
        if (secsUntilNextUpdate<0) {
            // Oops, missed update time!!!
            updateRss();
        } else {
            rssUpdateTimer->start(secsUntilNextUpdate*1000ll);
        }
    }
}

void PodcastService::stopRssUpdateTimer()
{
    if (rssUpdateTimer) {
        rssUpdateTimer->stop();
    }
}

void PodcastService::updateRss()
{
    foreach (MusicLibraryItem *i, m_childItems) {
        QUrl url=static_cast<MusicLibraryItemPodcast *>(i)->rssUrl();
        updateUrls.insert(url);
        if (!processingUrl(url)) {
            addUrl(url, false);
        }
    }
}

void PodcastService::currentMpdSong(const Song &s)
{
    if ((s.isFromOnlineService() && s.album==constName) || isPodcastFile(s.file)) {
        QString path=s.decodedPath();
        if (path.isEmpty()) {
            path=s.file;
        }
        foreach (MusicLibraryItem *p, m_childItems) {
            MusicLibraryItemPodcast *podcast=static_cast<MusicLibraryItemPodcast *>(p);
            foreach (MusicLibraryItem *i, podcast->childItems()) {
                MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                if (song->file()==path || song->song().podcastLocalPath()==path) {
                    if (!song->song().hasBeenPlayed()) {
                        podcast->setPlayed(song);
                        emitDataChanged(createIndex(song));
                        emitDataChanged(createIndex(podcast));
                        podcast->save();
                    }
                    return;
                }
            }
        }
    }
}
