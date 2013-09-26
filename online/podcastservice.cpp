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
#include "settings.h"
#include "dialog.h"
#include "buddylabel.h"
#include "mpdconnection.h"
#include "config.h"
#include <QCoreApplication>
#include <QDir>
#include <QUrl>
#include <QSet>
#include <QTimer>
#include <QComboBox>
#include <QFormLayout>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <stdio.h>

class PodcastSettingsDialog : public Dialog
{
public:
    PodcastSettingsDialog(QWidget *p)
        : Dialog(p)
    {
        QWidget *mw=new QWidget(this);
        QFormLayout * lay=new QFormLayout(mw);
        BuddyLabel *label=new BuddyLabel(i18n("Check for new updates:"), mw);
        combo = new QComboBox(this);
        label->setBuddy(combo);
        lay->setWidget(0, QFormLayout::LabelRole, label);
        lay->setWidget(0, QFormLayout::FieldRole, combo);
        setButtons(Ok|Cancel);
        setMainWidget(mw);
        setCaption(i18n("Podcast Update"));

        combo->addItem(i18n("Manually"), 0);
        combo->addItem(i18n("Every 15 minutes"), 15);
        combo->addItem(i18n("Every 30 minutes"), 30);
        combo->addItem(i18n("Every hour"), 60);
        combo->addItem(i18n("Every 2 hours"), 2*60);
        combo->addItem(i18n("Every 6 hours"), 6*60);
        combo->addItem(i18n("Every 12 hours"), 12*60);
        combo->addItem(i18n("Every day"), 24*60);
        combo->addItem(i18n("Every week"), 7*24*60);

        int val=Settings::self()->rssUpdate();
        int possible=0;
        for (int i=0; i<combo->count(); ++i) {
            int cval=combo->itemData(i).toInt();
            if (cval==val) {
                combo->setCurrentIndex(i);
                possible=-1;
                break;
            }
            if (cval<val) {
                possible=i;
            }
        }

        if (possible>=0) {
            combo->setCurrentIndex(possible);
        }
    }

    int update() const { return combo->itemData(combo->currentIndex()).toInt(); }

private:
    QComboBox *combo;
};

const QString PodcastService::constName=QLatin1String("Podcasts");
QString PodcastService::iconFile;

static const char * constNewFeedProperty="new-feed";

// Move files from previous ~/.config/cantata to ~/.local/share/cantata
static void moveToNewLocation()
{
    #if !defined Q_OS_WIN && !defined Q_OS_MAC // Not required for windows - as already stored in data location!
    if (Settings::self()->version()<CANTATA_MAKE_VERSION(1, 51, 0)) {
        Utils::moveDir(Utils::configDir(MusicLibraryItemPodcast::constDir), Utils::dataDir(MusicLibraryItemPodcast::constDir, true));
    }
    #endif
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
    , updateTimer(0)
{
    moveToNewLocation();

    loaded=true;
    setUseArtistImages(false);
    setUseAlbumImages(false);
    loadAll();
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(currentMpdSong(const Song &)));
    if (iconFile.isEmpty()) {
        #ifdef Q_OS_WIN
        iconFile=QCoreApplication::applicationDirPath()+"/icons/podcasts.png";
        #else
        iconFile=QString(INSTALL_PREFIX"/share/")+QCoreApplication::applicationName()+"/icons/podcasts.png";
        #endif
    }
}

Song PodcastService::fixPath(const Song &orig, bool) const
{
    Song song=orig;
    song.setIsFromOnlineService(constName);
    return encode(song);
}

void PodcastService::clear()
{
    cancelAll();
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
        startTimer();
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

    if (updateUrls.contains(j->url())){
        updateUrls.remove(j->url());
        if (updateUrls.isEmpty()) {
            lastRssUpdate=QDateTime::currentDateTime();
            Settings::self()->saveLastRssUpdate(lastRssUpdate);
            startTimer();
        }
    }

    bool isNew=j->property(constNewFeedProperty).toBool();

    MusicLibraryItemPodcast *podcast=new MusicLibraryItemPodcast(QString(), this);
    if (podcast->loadRss(j->actualJob())) {

        if (isNew) {
            podcast->save();
            beginInsertRows(index(), childCount(), childCount());
            m_childItems.append(podcast);
            endInsertRows();
            emitNeedToSort();
        } else {
            MusicLibraryItemPodcast *orig = getPodcast(j->url());
            if (!orig) {
                delete podcast;
                return;
            }
            QSet<QString> origSongs;
            QSet<QString> newSongs;
            QSet<QString> playedSongs;
            foreach (MusicLibraryItem *i, orig->childItems()) {
                MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                origSongs.insert(song->file());
                if (song->song().hasbeenPlayed()) {
                    playedSongs.insert(song->file());
                }
            }
            foreach (MusicLibraryItem *i, podcast->childItems()) {
                MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                newSongs.insert(song->file());
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

                // Restore played status...
                foreach (MusicLibraryItem *i, orig->childItems()) {
                    MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                    if (playedSongs.contains(song->file())) {
                        orig->setPlayed(song);
                        playedSongs.remove(song->file());
                        if (playedSongs.isEmpty()) {
                            break;
                        }
                    }
                }

                orig->save();
                emitNeedToSort();
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
    PodcastSettingsDialog dlg(p);
    if (QDialog::Accepted==dlg.exec()) {
        int current=Settings::self()->rssUpdate();
        if (current!=dlg.update()) {
            Settings::self()->saveRssUpdate(dlg.update());
            startTimer();
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

void PodcastService::unSubscribe(MusicLibraryItem *item)
{
    int row=m_childItems.indexOf(item);
    if (row>=0) {
        beginRemoveRows(index(), row, row);
        static_cast<MusicLibraryItemPodcast *>(item)->removeFiles();
        delete m_childItems.takeAt(row);
        resetRows();
        endRemoveRows();
        if (m_childItems.isEmpty()) {
            stopTimer();
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
    NetworkJob *job=NetworkAccessManager::self()->get(QUrl(url));
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    job->setProperty(constNewFeedProperty, isNew);
    jobs.append(job);
}

void PodcastService::startTimer()
{
    if (0==Settings::self()->rssUpdate() || m_childItems.isEmpty()) {
        stopTimer();
        return;
    }
    if (!updateTimer) {
        updateTimer=new QTimer(this);
        updateTimer->setSingleShot(true);
        connect(updateTimer, SIGNAL(timeout()), this, SLOT(updateRss()));
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
            updateTimer->start(secsUntilNextUpdate*1000ll);
        }
    }
}

void PodcastService::stopTimer()
{
    if (updateTimer) {
        updateTimer->stop();
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
    if (s.isFromOnlineService() && s.album==constName) {
        foreach (MusicLibraryItem *p, m_childItems) {
            MusicLibraryItemPodcast *podcast=static_cast<MusicLibraryItemPodcast *>(p);
            foreach (MusicLibraryItem *i, podcast->childItems()) {
                MusicLibraryItemSong *song=static_cast<MusicLibraryItemSong *>(i);
                if (song->file()==s.file) {
                    if (!song->song().hasbeenPlayed()) {
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
