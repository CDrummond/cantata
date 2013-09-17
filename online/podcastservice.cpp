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

#include "podcastservice.h"
#include "networkaccessmanager.h"
#include "onlineservicesmodel.h"
#include "musiclibraryitempodcast.h"
#include "musiclibraryitemsong.h"
#include "utils.h"
#include <QDir>
#include <QUrl>
#include <QSet>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

const QString PodcastService::constName=QLatin1String("Podcasts");

static const char * constNewFeedProperty="new-feed";

PodcastService::PodcastService(MusicModel *m)
    : OnlineService(m, i18n("Podcasts"))
{
    loaded=true;
    setUseArtistImages(false);
    setUseAlbumImages(false);
    loadAll();
}

Song PodcastService::fixPath(const Song &orig, bool) const
{
    Song song=orig;
    song.album=constName;
    song.albumartist=i18n("Streams");
    song.disc=0xFF;
    return encode(song);
}

void PodcastService::clear()
{
    cancelAll();
    ::OnlineService::clear();
}

void PodcastService::loadAll()
{
    QString dir=Utils::configDir(MusicLibraryItemPodcast::constDir);

    if (!dir.isEmpty()) {
        QDir d(dir);
        QStringList entries=d.entryList(QStringList() << "*"+MusicLibraryItemPodcast::constExt, QDir::Files|QDir::Readable|QDir::NoDot|QDir::NoDotDot);
        foreach (const QString &e, entries) {
            if (!update) {
                update=new MusicLibraryItemRoot();
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
    }
}

void PodcastService::cancelAll()
{
    foreach (NetworkJob *j, jobs) {
        disconnect(j, SIGNAL(finished()), this, SLOT(jobFinished()));
        j->abort();
        j->deleteLater();
    }
    jobs.clear();
    setBusy(false);
}

void PodcastService::jobFinished()
{
    NetworkJob *j=dynamic_cast<NetworkJob *>(sender());
    if (!j || !jobs.contains(j)) {
        return;
    }

    j->deleteLater();
    jobs.removeAll(j);

    if (!j->ok()) {
        emitError(i18n("Failed to download %1", j->url().toString()));
        return;
    }

    bool isNew=j->property(constNewFeedProperty).toBool();

    MusicLibraryItemPodcast *podcast=new MusicLibraryItemPodcast(QString(), this);
    if (podcast->loadRss(j->actualJob())) {

        if (isNew) {
            podcast->save();
            beginInsertRows(index(), childCount(), childCount());
            m_childItems.append(podcast);
            endInsertRows();
        } else {
            MusicLibraryItemPodcast *orig = getPodcast(j->url());
            if (!orig) {
                delete podcast;
                return;
            }
            QSet<QString> origSongs;
            QSet<QString> newSongs;
            foreach (MusicLibraryItem *i, orig->childItems()) {
                MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                origSongs.insert(song->file()+song->data());
            }
            foreach (MusicLibraryItem *i, podcast->childItems()) {
                MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                newSongs.insert(song->file()+song->data());
            }

            QSet<QString> added=newSongs-origSongs;
            QSet<QString> removed=origSongs-newSongs;
            if (added.count() || removed.count()) {
                QModelIndex origIndex=createIndex(orig);
                if (orig->childCount()) {
                    beginRemoveRows(origIndex, 0, orig->childCount()-1);
                    orig->clear();
                    endRemoveRows();
                }
                if (added.count()) {
                    beginInsertRows(origIndex, 0, podcast->childCount()-1);
                    orig->addAll(podcast);
                    endInsertRows();
                }
                orig->updateTrackNumbers();
                emitUpdated();
            }

            delete podcast;
        }

    } else if (isNew) {
        delete podcast;
        emitError(i18n("Failed to parse %1", j->url().toString()));
    }

    if (jobs.isEmpty()) {
        setBusy(false);
    }
}

void PodcastService::configure(QWidget *p)
{

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

void PodcastService::unSubscribe(MusicLibraryItem *item)
{
    int row=m_childItems.indexOf(item);
    if (row>=0) {
        beginRemoveRows(index(), row, row);
        static_cast<MusicLibraryItemPodcast *>(item)->removeFiles();
        delete m_childItems.takeAt(row);
        resetRows();
        endRemoveRows();
    }
}

void PodcastService::refreshSubscription(MusicLibraryItem *item)
{
    QUrl url=static_cast<MusicLibraryItemPodcast *>(item)->rssUrl();
    if (processingUrl(url)) {
        return;
    }
    addUrl(url, false);
}

bool PodcastService::processingUrl(const QUrl &url)
{
    foreach (NetworkJob *j, jobs) {
        if (j->url()==url) {
            return true;
        }
    }

    return false;
}

void PodcastService::addUrl(const QUrl &url, bool isNew)
{
    setBusy(true);
    NetworkJob *job=NetworkAccessManager::self()->getNew(QUrl(url));
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    job->setProperty(constNewFeedProperty, isNew);
    jobs.append(job);
}
