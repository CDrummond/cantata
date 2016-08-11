/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "rssparser.h"
#include "support/utils.h"
#include "gui/settings.h"
#include "gui/plurals.h"
#include "widgets/icons.h"
#include "mpd-interface/mpdconnection.h"
#include "config.h"
#include "http/httpserver.h"
#include "qtiocompressor/qtiocompressor.h"
#include "network/networkaccessmanager.h"
#include "models/roles.h"
#include "models/playqueuemodel.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTimer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QCryptographicHash>
#include <QMimeData>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <stdio.h>

PodcastService::Proxy::Proxy(QObject *parent)
    : ProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortLocaleAware(true);
}

bool PodcastService::Proxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (left.row()<0 || right.row()<0) {
        return left.row()<0;
    }

    if (!static_cast<Item *>(left.internalPointer())->isPodcast()) {
        Episode *l=static_cast<Episode *>(left.internalPointer());
        Episode *r=static_cast<Episode *>(right.internalPointer());

        if (l->publishedDate!=r->publishedDate) {
            return l->publishedDate>r->publishedDate;
        }
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

bool PodcastService::Proxy::filterAcceptsPodcast(const Podcast *pod) const
{
    foreach (const Episode *ep, pod->episodes) {
        if (filterAcceptsEpisode(ep)) {
            return true;
        }
    }

    return false;
}

bool PodcastService::Proxy::filterAcceptsEpisode(const Episode *item) const
{
    return matchesFilter(QStringList() << item->name << item->parent->name);
}

bool PodcastService::Proxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!filterEnabled) {
        return true;
    }

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const PodcastService::Item *item = static_cast<const PodcastService::Item *>(index.internalPointer());

    if (filterStrings.isEmpty()) {
        return true;
    }

    if (item->isPodcast()) {
        return filterAcceptsPodcast(static_cast<const Podcast *>(item));
    }
    return filterAcceptsEpisode(static_cast<const Episode *>(item));
}

const QLatin1String PodcastService::constName("podcasts");
static const QLatin1String constExt(".xml.gz");
static const char * constNewFeedProperty="new-feed";
static const char * constRssUrlProperty="rss-url";
static const char * constDestProperty="dest";
static const QLatin1String constPartialExt(".partial");

static QString generateFileName(const QUrl &url, bool creatingNew)
{
    QString hash=QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Md5).toHex();
    QString dir=Utils::dataDir(PodcastService::constName, true);
    QString fileName=dir+hash+constExt;

    if (creatingNew) {
        int i=0;
        while (QFile::exists(fileName) && i<100) {
            fileName=dir+hash+QChar('_')+QString::number(i)+constExt;
            i++;
        }
    }

    return fileName;
}

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

Song PodcastService::Episode::toSong() const
{
    Song song;
    song.title=name;
    song.file=url.toString();
    song.artist=parent->name;
    song.time=duration;
    if (!localFile.isEmpty()) {
        song.setPodcastLocalPath(localFile);
    }
    return song;
}

PodcastService::Podcast::Podcast(const QString &f)
    : unplayedCount(0)
    , fileName(f)
    , imageFile(f)
{
    imageFile=imageFile.replace(constExt, ".jpg");
}

static QLatin1String constTopTag("podcast");
static QLatin1String constImageAttribute("img");
static QLatin1String constRssAttribute("rss");
static QLatin1String constEpisodeTag("episode");
static QLatin1String constNameAttribute("name");
static QLatin1String constDateAttribute("date");
static QLatin1String constUrlAttribute("url");
static QLatin1String constTimeAttribute("time");
static QLatin1String constPlayedAttribute("played");
static QLatin1String constLocalAttribute("local");
static QLatin1String constTrue("true");

bool PodcastService::Podcast::load()
{
    if (fileName.isEmpty()) {
        return false;
    }

    QFile file(fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::ReadOnly)) {
        return false;
    }

    QXmlStreamReader reader(&compressor);
    unplayedCount=0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.error() && reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                imageUrl=attributes.value(constImageAttribute).toString();
                url=attributes.value(constRssAttribute).toString();
                name=attributes.value(constNameAttribute).toString();
                if (url.isEmpty() || name.isEmpty()) {
                    return false;
                }
            } else if (constEpisodeTag == element) {
                QString epName=attributes.value(constNameAttribute).toString();
                QString epUrl=attributes.value(constUrlAttribute).toString();
                if (!epName.isEmpty() && !epUrl.isEmpty()) {
                    Episode *ep=new Episode(QDateTime::fromString(attributes.value(constDateAttribute).toString(), Qt::ISODate), epName, epUrl, this);
                    QString localFile=attributes.value(constLocalAttribute).toString();
                    QString time=attributes.value(constTimeAttribute).toString();

                    ep->duration=time.isEmpty() ? 0 : time.toUInt();
                    ep->played=constTrue==attributes.value(constPlayedAttribute).toString();
                    if (QFile::exists(localFile)) {
                        ep->localFile=localFile;
                    }

                    episodes.append(ep);
                    if (!ep->played) {
                        unplayedCount++;
                    }
                }
            }
        }
    }

    return true;
}

bool PodcastService::Podcast::save() const
{
    if (fileName.isEmpty()) {
        return false;
    }

    QFile file(fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::WriteOnly)) {
        return false;
    }

    QXmlStreamWriter writer(&compressor);
    writer.writeStartElement(constTopTag);
    writer.writeAttribute(constImageAttribute, imageUrl.toString()); // ??
    writer.writeAttribute(constRssAttribute, url.toString()); // ??
    writer.writeAttribute(constNameAttribute, name);
    foreach (Episode *ep, episodes) {
        writer.writeStartElement(constEpisodeTag);
        writer.writeAttribute(constNameAttribute, ep->name);
        writer.writeAttribute(constUrlAttribute, ep->url.toString()); // ??
        if (ep->duration) {
            writer.writeAttribute(constTimeAttribute, QString::number(ep->duration));
        }
        if (ep->played) {
            writer.writeAttribute(constPlayedAttribute, constTrue);
        }
        if (ep->publishedDate.isValid()) {
            writer.writeAttribute(constDateAttribute, ep->publishedDate.toString(Qt::ISODate));
        }
        if (!ep->localFile.isEmpty()) {
            writer.writeAttribute(constLocalAttribute, ep->localFile);
        }
        writer.writeEndElement();
    }
    writer.writeEndElement();
    compressor.close();
    return true;
}

void PodcastService::Podcast::add(Episode *ep)
{
    ep->parent=this;
    episodes.append(ep);
}

void PodcastService::Podcast::add(QList<Episode *> &eps)
{
    foreach(Episode *ep, eps) {
        add(ep);
    }
    setUnplayedCount();
}

PodcastService::Episode * PodcastService::Podcast::getEpisode(const QUrl &epUrl) const
{
    foreach (Episode *episode, episodes) {
        if (episode->url==epUrl) {
            return episode;
        }
    }
    return 0;
}

void PodcastService::Podcast::setUnplayedCount()
{
    unplayedCount=episodes.count();
    foreach (Episode *episode, episodes) {
        if (episode->played) {
            unplayedCount--;
        }
    }
}

void PodcastService::Podcast::removeFiles()
{
    if (!fileName.isEmpty() && QFile::exists(fileName)) {
        QFile::remove(fileName);
    }
    if (!imageFile.isEmpty() && QFile::exists(imageFile)) {
        QFile::remove(imageFile);
    }
}

const Song & PodcastService::Podcast::coverSong()
{
    if (song.isEmpty()) {
        song.artist=constName;
        song.album=name;
        song.title=name;
        song.type=Song::OnlineSvrTrack;
        song.setIsFromOnlineService(constName);
        song.setExtraField(Song::OnlineImageUrl, imageUrl.toString());
        song.setExtraField(Song::OnlineImageCacheName, imageFile);
        song.file=url.toString();
    }
    return song;
}

PodcastService::PodcastService(QObject *p)
    : ActionModel(p)
    , downloadJob(0)
    , rssUpdateTimer(0)
{
    QMetaObject::invokeMethod(this, "loadAll", Qt::QueuedConnection);
    icn.addFile(":"+constName);
    useCovers(name(), true);
    clearPartialDownloads();
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(currentMpdSong(const Song &)));
}

QString PodcastService::name() const
{
    return constName;
}

QString PodcastService::title() const
{
    return QLatin1String("Podcasts");
}

QString PodcastService::descr() const
{
    return i18n("Subscribe to RSS feeds");
}

int PodcastService::rowCount(const QModelIndex &index) const
{
    if (index.column()>0) {
        return 0;
    }

    if (!index.isValid()) {
        return podcasts.size();
    }

    Item *item=static_cast<Item *>(index.internalPointer());
    if (item->isPodcast()) {
        return static_cast<Podcast *>(index.internalPointer())->episodes.count();
    }
    return 0;
}

bool PodcastService::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid() || static_cast<Item *>(parent.internalPointer())->isPodcast();
}

QModelIndex PodcastService::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isPodcast()) {
        return QModelIndex();
    } else {
        Episode *episode=static_cast<Episode *>(item);

        if (episode->parent) {
            return createIndex(podcasts.indexOf(episode->parent), 0, episode->parent);
        }
    }

    return QModelIndex();
}

QModelIndex PodcastService::index(int row, int col, const QModelIndex &parent) const
{
    if (!hasIndex(row, col, parent)) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        Item *p=static_cast<Item *>(parent.internalPointer());

        if (p->isPodcast()) {
            Podcast *podcast=static_cast<Podcast *>(p);
            return row<podcast->episodes.count() ? createIndex(row, col, podcast->episodes.at(row)) : QModelIndex();
        }
    }

    return row<podcasts.count() ? createIndex(row, col, podcasts.at(row)) : QModelIndex();
}

QVariant PodcastService::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        switch (role) {
        case Cantata::Role_TitleText:
            return title();
        case Cantata::Role_SubText:
            return Plurals::podcasts(podcasts.count());
        case Qt::DecorationRole:
            return icon();
        }
        return QVariant();
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isPodcast()) {
        Podcast *podcast=static_cast<Podcast *>(item);

        switch(role) {
        case Cantata::Role_ListImage:
            return true;
        case Cantata::Role_CoverSong: {
            QVariant v;
            v.setValue<Song>(podcast->coverSong());
            return v;
        }
        case Qt::DecorationRole:
            return Icons::self()->podcastIcon;
        case Cantata::Role_LoadCoverInUIThread:
            return true;
        case Cantata::Role_MainText:
        case Qt::DisplayRole:
            return i18nc("podcast name (num unplayed episodes)", "%1 (%2)", podcast->name, podcast->unplayedCount);
        case Cantata::Role_SubText:
            return Plurals::episodes(podcast->episodes.count());
        case Qt::ToolTipRole:
            if (Settings::self()->infoTooltips()) {
                return podcast->name+QLatin1String("<br/>")+Plurals::episodes(podcast->episodes.count());
            }
            break;
        case Qt::FontRole:
            if (podcast->unplayedCount>0) {
                QFont f;
                f.setBold(true);
                return f;
            }
        default:
            break;
        }
    } else {
        Episode *episode=static_cast<Episode *>(item);

        switch (role) {
        case Qt::DecorationRole:
            if (!episode->localFile.isEmpty()) {
                return Icons::self()->savedRssListIcon;
            } else if (episode->downloadProg>=0) {
                return Icons::self()->downloadIcon;
            } else if (Episode::QueuedForDownload==episode->downloadProg) {
                return Icons::self()->clockIcon;
            } else {
                return Icons::self()->rssListIcon;
            }
        case Cantata::Role_MainText:
        case Qt::DisplayRole:
            return episode->name;
        case Cantata::Role_SubText:
            if (episode->downloadProg>=0) {
                return Utils::formatTime(episode->duration, true)+QLatin1Char(' ')+
                       i18n("(Downloading: %1%)", episode->downloadProg);
            }
            return episode->publishedDate.toString(Qt::LocalDate)+
                        (0==episode->duration
                            ? QString()
                            : (QLatin1String(" (")+Utils::formatTime(episode->duration, true)+QLatin1Char(')')));
        case Qt::ToolTipRole:
            if (Settings::self()->infoTooltips()) {
                return QLatin1String("<b>")+episode->parent->name+QLatin1String("</b><br/>")+
                        episode->name+QLatin1String("<br/>")+Utils::formatTime(episode->duration, true)+
                        QLatin1String("<br/>")+episode->publishedDate.toString(Qt::LocalDate);
            }
            break;
        case Qt::FontRole:
            if (!episode->played) {
                QFont f;
                f.setBold(true);
                return f;
            }
        default:
            break;
        }
    }

    return ActionModel::data(index, role);
}

Qt::ItemFlags PodcastService::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    }
    return Qt::NoItemFlags;
}

QMimeData * PodcastService::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData=0;
    QStringList paths=filenames(indexes);

    if (!paths.isEmpty()) {
        mimeData=new QMimeData();
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}

QStringList PodcastService::filenames(const QModelIndexList &indexes, bool allowPlaylists) const
{
    Q_UNUSED(allowPlaylists)
    QList<Song> songList=songs(indexes);
    QStringList files;
    foreach (const Song &song, songList) {
        files.append(song.file);
    }
    return files;
}

QList<Song> PodcastService::songs(const QModelIndexList &indexes, bool allowPlaylists) const
{
    Q_UNUSED(allowPlaylists)
    QList<Song> songs;
    QSet<Item *> selectedPodcasts;

    foreach (const QModelIndex &idx, indexes) {
        Item *item=static_cast<Item *>(idx.internalPointer());
        if (item->isPodcast()) {
            Podcast *podcast=static_cast<Podcast *>(item);
            foreach (const Episode *episode, podcast->episodes) {
                selectedPodcasts.insert(item);
                Song s=episode->toSong();
                songs.append(fixPath(s));
            }
        }
    }
    foreach (const QModelIndex &idx, indexes) {
        Item *item=static_cast<Item *>(idx.internalPointer());
        if (!item->isPodcast()) {
            Episode *episode=static_cast<Episode *>(item);
            if (!selectedPodcasts.contains(episode->parent)) {
                Song s=episode->toSong();
                songs.append(fixPath(s));
            }
        }
    }
    return songs;
}

Song & PodcastService::fixPath(Song &song) const
{
    song.setPodcastLocalPath(QString());
    song.setIsFromOnlineService(constName);
    song.artist=title();
    if (!song.podcastLocalPath().isEmpty() && QFile::exists(song.podcastLocalPath())) {
        if (!HttpServer::self()->forceUsage() && MPDConnection::self()->localFilePlaybackSupported()) {
            song.file=QLatin1String("file://")+song.podcastLocalPath();
        } else if (HttpServer::self()->isAlive()) {
            song.file=song.podcastLocalPath();
            song.file=HttpServer::self()->encodeUrl(song);
        }
        return song;
    }
    return encode(song);
}

void PodcastService::clear()
{
    cancelAllJobs();
    beginResetModel();
    qDeleteAll(podcasts);
    podcasts.clear();
    endResetModel();
    emit dataChanged(QModelIndex(), QModelIndex());
}

void PodcastService::loadAll()
{
    beginResetModel();
    QString dir=Utils::dataDir(constName);

    if (!dir.isEmpty()) {
        QDir d(dir);
        QStringList entries=d.entryList(QStringList() << "*"+constExt, QDir::Files|QDir::Readable|QDir::NoDot|QDir::NoDotDot);
        foreach (const QString &e, entries) {
            Podcast *podcast=new Podcast(dir+e);
            if (podcast->load()) {
                podcasts.append(podcast);
            } else {
                delete podcast;
            }
        }
        startRssUpdateTimer();
    }
    endResetModel();
    emit dataChanged(QModelIndex(), QModelIndex());
}

void PodcastService::cancelAll()
{
    foreach (NetworkJob *j, rssJobs) {
        disconnect(j, SIGNAL(finished()), this, SLOT(rssJobFinished()));
        j->cancelAndDelete();
    }
    rssJobs.clear();
    cancelAllDownloads();
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

        RssParser::Channel ch=RssParser::parse(j->actualJob());

        if (!ch.isValid()) {
            if (isNew) {
                emit newError(i18n("Failed to parse %1", j->origUrl().toString()));
            } else {
                emit error(i18n("Failed to parse %1", j->origUrl().toString()));
            }
            return;
        }

        if (ch.video) {
            if (isNew) {
                emit newError(i18n("Cantata only supports audio podcasts! %1 contains only video podcasts.", j->origUrl().toString()));
            } else {
                emit error(i18n("Cantata only supports audio podcasts! %1 contains only video podcasts.", j->origUrl().toString()));
            }
            return;
        }

        int autoDownload=Settings::self()->podcastAutoDownloadLimit();

        if (isNew) {
            Podcast *podcast=new Podcast();
            podcast->url=j->origUrl();
            podcast->fileName=podcast->imageFile=generateFileName(podcast->url, true);
            podcast->imageFile=podcast->imageFile.replace(constExt, ".jpg");
            podcast->imageUrl=ch.image.toString();
            podcast->name=ch.name;
            podcast->unplayedCount=ch.episodes.count();
            foreach (const RssParser::Episode &ep, ch.episodes) {
                Episode *episode=new Episode(ep.publicationDate, ep.name, ep.url, podcast);
                episode->duration=ep.duration;
                podcast->add(episode);
            }
            podcast->save();
            beginInsertRows(QModelIndex(), podcasts.count(), podcasts.count());
            podcasts.append(podcast);
            emit dataChanged(QModelIndex(), QModelIndex());
            if (autoDownload) {
                int ep=0;
                foreach (Episode *episode, podcast->episodes) {
                    downloadEpisode(podcast, QUrl(episode->url));
                    if (autoDownload<1000 && ++ep>=autoDownload) {
                        break;
                    }
                }
            }
            endInsertRows();
        } else {
            Podcast *podcast = getPodcast(j->origUrl());
            if (!podcast) {
                return;
            }
            QSet<QUrl> newEpisodes;
            QSet<QUrl> oldEpisodes;
            foreach (Episode *episode, podcast->episodes) {
                newEpisodes.insert(episode->url);
            }
            foreach (const RssParser::Episode &ep, ch.episodes) {
                oldEpisodes.insert(ep.url);
            }

            QSet<QUrl> added=oldEpisodes-newEpisodes;
            QSet<QUrl> removed=newEpisodes-oldEpisodes;
            if (added.count() || removed.count()) {
                QModelIndex podcastIndex=createIndex(podcasts.indexOf(podcast), 0, (void *)podcast);
                if (removed.count()) {
                    foreach (const QUrl &s, removed) {
                        Episode *episode=podcast->getEpisode(s);
                        if (episode->localFile.isEmpty() || !QFile::exists(episode->localFile)) {
                            int idx=podcast->episodes.indexOf(episode);
                            if (-1!=idx) {
                                beginRemoveRows(podcastIndex, idx, idx);
                                podcast->episodes.removeAt(idx);
                                delete episode;
                                endRemoveRows();
                            }
                        }
                    }
                }
                if (added.count()) {
                    beginInsertRows(podcastIndex, podcast->episodes.count(), (podcast->episodes.count()+added.count())-1);

                    foreach (const RssParser::Episode &ep, ch.episodes) {
                        QString epUrl=ep.url.toString();
                        if (added.contains(epUrl)) {
                            Episode *episode=new Episode(ep.publicationDate, ep.name, ep.url, podcast);
                            episode->duration=ep.duration;
                            podcast->add(episode);
                        }
                    }
                    endInsertRows();
                }

                podcast->setUnplayedCount();
                podcast->save();
                emit dataChanged(podcastIndex, podcastIndex);
            }
        }
    } else {
        if (isNew) {
            emit newError(i18n("Failed to download %1", j->origUrl().toString()));
        } else {
            emit error(i18n("Failed to download %1", j->origUrl().toString()));
        }
    }
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

PodcastService::Podcast * PodcastService::getPodcast(const QUrl &url) const
{
    foreach (Podcast *podcast, podcasts) {
        if (podcast->url==url) {
            return podcast;
        }
    }
    return 0;
}

void PodcastService::unSubscribe(Podcast *podcast)
{
    int row=podcasts.indexOf(podcast);
    if (row>=0) {
        QList<Episode *> episodes;
        foreach (Episode *e, podcast->episodes) {
            episodes.append(e);
        }
        cancelDownloads(episodes);
        beginRemoveRows(QModelIndex(), row, row);
        podcast->removeFiles();
        delete podcasts.takeAt(row);
        endRemoveRows();
        emit dataChanged(QModelIndex(), QModelIndex());
        if (podcasts.isEmpty()) {
            stopRssUpdateTimer();
        }
    }
}

void PodcastService::refresh(const QModelIndexList &list)
{
    foreach (const QModelIndex &idx, list) {
        Item *itm=static_cast<Item *>(idx.internalPointer());
        if (itm->isPodcast()) {
            refreshSubscription(static_cast<Podcast *>(itm));
        }
    }
}

void PodcastService::refreshAll()
{
    foreach (Podcast *pod, podcasts) {
        refreshSubscription(pod);
    }
}

void PodcastService::refreshSubscription(Podcast *item)
{
    if (item) {
        QUrl url=item->url;
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
        updateEpisode(e.rssUrl, e.url, Episode::NotDownloading);
    }

    toDownload.clear();
    cancelDownload();
}

void PodcastService::downloadPodcasts(Podcast *pod, const QList<Episode *> &episodes)
{
    foreach (Episode *ep, episodes) {
        downloadEpisode(pod, QUrl(ep->url));
    }
}

void PodcastService::deleteDownloadedPodcasts(Podcast *pod, const QList<Episode *> &episodes)
{
    cancelDownloads(episodes);
    bool modified=false;
    foreach (Episode *ep, episodes) {
        QString fileName=ep->localFile;
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
            ep->localFile=QString();
            ep->downloadProg=Episode::NotDownloading;
            QModelIndex idx=createIndex(pod->episodes.indexOf(ep), 0, (void *)ep);
            emit dataChanged(idx, idx);
            modified=true;
        }
    }
    if (modified) {
        QModelIndex idx=createIndex(podcasts.indexOf(pod), 0, (void *)pod);
        emit dataChanged(idx, idx);
        pod->save();
    }
}

void PodcastService::setPodcastsAsListened(Podcast *pod, const QList<Episode *> &episodes, bool listened)
{
    bool modified=false;
    foreach (Episode *ep, episodes) {
        if (listened!=ep->played) {
            ep->played=listened;
            QModelIndex idx=createIndex(pod->episodes.indexOf(ep), 0, (void *)ep);
            emit dataChanged(idx, idx);
            modified=true;
            if (listened) {
                pod->unplayedCount--;
            } else {
                pod->unplayedCount++;
            }
        }
    }
    if (modified) {
        QModelIndex idx=createIndex(podcasts.indexOf(pod), 0, (void *)pod);
        emit dataChanged(idx, idx);
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

void PodcastService::downloadEpisode(const Podcast *podcast, const QUrl &episode)
{
    QString dest=Settings::self()->podcastDownloadPath();
    if (dest.isEmpty()) {
        return;
    }
    if (downloadingEpisode(episode)) {
        return;
    }
    dest=Utils::fixPath(dest)+Utils::fixPath(encodeName(podcast->name))+Utils::getFile(episode.toString());
    toDownload.append(DownloadEntry(episode, podcast->url, dest));
    updateEpisode(podcast->url, episode, Episode::QueuedForDownload);
    doNextDownload();
}

void PodcastService::cancelDownloads(const QList<Episode *> episodes)
{
    bool cancelDl=false;
    foreach (Episode *e, episodes) {
        toDownload.removeAll(e->url);
        e->downloadProg=Episode::NotDownloading;
        QModelIndex idx=createIndex(e->parent->episodes.indexOf(e), 0, (void *)e);
        emit dataChanged(idx, idx);
        if (!cancelDl && downloadJob && downloadJob->url()==e->url) {
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
        updateEpisode(downloadJob->property(constRssUrlProperty).toUrl(), downloadJob->origUrl(), Episode::NotDownloading);
        downloadJob=0;
    }
}

void PodcastService::doNextDownload()
{
    if (downloadJob) {
        return;
    }

    if (toDownload.isEmpty()) {
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
    Podcast *pod=getPodcast(rssUrl);
    if (pod) {
        Episode *episode=pod->getEpisode(url);
        if (episode && episode->downloadProg!=pc) {
            episode->downloadProg=pc;
            QModelIndex idx=createIndex(pod->episodes.indexOf(episode), 0, (void *)episode);
            emit dataChanged(idx, idx);
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
                Podcast *pod=getPodcast(job->property(constRssUrlProperty).toUrl());
                if (pod) {
                    Episode *episode=pod->getEpisode(job->origUrl());
                    if (episode) {
                        episode->localFile=dest;
                        pod->save();
                        QModelIndex idx=createIndex(pod->episodes.indexOf(episode), 0, (void *)episode);
                        emit dataChanged(idx, idx);
                    }
                }
            }
        }
    } else if (!partial.isEmpty() && QFile::exists(partial)) {
        QFile::remove(partial);
    }
    updateEpisode(job->property(constRssUrlProperty).toUrl(), job->origUrl(), Episode::NotDownloading);
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
    if (0==Settings::self()->rssUpdate() || podcasts.isEmpty()) {
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
    foreach (Podcast *podcast, podcasts) {
        const QUrl &url=podcast->url;
        updateUrls.insert(url);
        if (!processingUrl(url)) {
            addUrl(url, false);
        }
    }
}

void PodcastService::currentMpdSong(const Song &s)
{
    if ((s.isFromOnlineService() && s.onlineService()==constName) || isPodcastFile(s.file)) {
        QString path=s.decodedPath();
        if (path.isEmpty()) {
            path=s.file;
        }
        foreach (Podcast *podcast, podcasts) {
            foreach (Episode *episode, podcast->episodes) {
                if (episode->url==path || episode->localFile==path) {
                    if (!episode->played) {
                        episode->played=true;
                        QModelIndex idx=createIndex(podcast->episodes.indexOf(episode), 0, (void *)episode);
                        emit dataChanged(idx, idx);
                        podcast->unplayedCount--;
                        podcast->save();
                        idx=createIndex(podcasts.indexOf(podcast), 0, (void *)podcast);
                        emit dataChanged(idx, idx);
                    }
                    return;
                }
            }
        }
    }
}
