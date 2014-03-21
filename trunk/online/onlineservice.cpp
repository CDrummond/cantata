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

#include "onlineservice.h"
#include "onlineservicesmodel.h"
#include "musiclibrarymodel.h"
#include "utils.h"
#include "networkaccessmanager.h"
#include "mpdparseutils.h"
#include "covers.h"
#include "qtiocompressor/qtiocompressor.h"
#include "thread.h"
#include "settings.h"
#include <QFile>
#include <QXmlStreamReader>

static QString cacheFileName(OnlineService *srv, bool create=false)
{
    return Utils::cacheDir(srv->id().toLower(), create)+QLatin1String("library")+MusicLibraryModel::constLibraryCompressedExt;
}

OnlineMusicLoader::OnlineMusicLoader(const QUrl &src)
    : source(src)
    , library(0)
    , network(0)
    , downloadJob(0)
    , stopRequested(false)
    , lastProg(-1)
{
    connect(this, SIGNAL(load()), this, SLOT(doLoad()));
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

void OnlineMusicLoader::start()
{
    emit load();
}

void OnlineMusicLoader::doLoad()
{
    if (library) {
        delete library;
    }
    library = new OnlineServiceMusicRoot;
    if (!readFromCache()) {
        emit status(i18n("Dowloading"), 0);
        if (!network) {
            network=new NetworkAccessManager(this);
        }
        downloadJob=network->get(source);
        connect(downloadJob, SIGNAL(finished()), SLOT(downloadFinished()));
        connect(downloadJob, SIGNAL(downloadProgress(qint64,qint64)), SLOT(downloadProgress(qint64,qint64)));
    }
}

void OnlineMusicLoader::stop()
{
    stopRequested=true;
    thread->stop();
}

OnlineServiceMusicRoot * OnlineMusicLoader::takeLibrary()
{
    OnlineServiceMusicRoot *lib=library;
    library=0;
    return lib;
}

bool OnlineMusicLoader::readFromCache()
{
    if (!cache.isEmpty() && QFile::exists(cache)) {
        emit status(i18n("Reading cache"), 0);
        if (library->fromXML(cache, QDateTime(), 0, QString(), this)) {
            if (!stopRequested) {
                fixLibrary();
                emit status(i18n("Updating display"), -100);
                emit loaded();
            }
            return true;
        }
    }
    return false;
}

void OnlineMusicLoader::fixLibrary()
{
    emit status(i18n("Grouping tracks"), -100);
    if (MPDParseUtils::groupSingle()) {
        library->groupSingleTracks();
    }
    if (MPDParseUtils::groupMultiple()) {
        library->groupMultipleArtists();
    }
}

void OnlineMusicLoader::downloadFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (stopRequested) {
        return;
    }

    if (reply->ok()) {
        emit status(i18n("Parsing response"), -100);
        QtIOCompressor comp(reply->actualJob());
        comp.setStreamFormat(QtIOCompressor::GzipFormat);
        if (comp.open(QIODevice::ReadOnly)) {
            QXmlStreamReader reader;
            reader.setDevice(&comp);
            if (parse(reader)) {
                fixLibrary();
                emit status(i18n("Saving cache"), 0);
                library->toXML(cache, QDateTime(), this);
                emit loaded();
            } else {
                emit error(i18n("Failed to parse"));
            }
        } else {
            emit error(i18n("Failed to parse"));
        }
    } else {
        emit error(i18n("Failed to download"));
    }
}

void OnlineMusicLoader::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    progressReport(i18n("Dowloading"), 0==bytesTotal ? 0 : ((bytesReceived*100)/bytesTotal));
}

void OnlineMusicLoader::readProgress(double pc)
{
    progressReport(i18n("Reading cache"), (int)pc);
}

void OnlineMusicLoader::writeProgress(double pc)
{
    progressReport(i18n("Saving cache"), (int)pc);
}

void OnlineMusicLoader::progressReport(const QString &str, int prog)
{
    if (stopRequested) {
        return;
    }
    if (prog!=lastProg) {
        lastProg=prog;
        emit status(str, lastProg);
    }
}

static const QString constUrlGuard=QLatin1String("#{SONG_DETAILS}");
static const QString constDeliminator=QLatin1String("<@>");

Song OnlineService::encode(const Song &song)
{
    Song encoded=song;
    encoded.file=song.file+constUrlGuard+
                encoded.artist.replace(constDeliminator, " ")+constDeliminator+
                encoded.albumartist.replace(constDeliminator, " ")+constDeliminator+
                encoded.album.replace(constDeliminator, " ")+constDeliminator+
                encoded.title.replace(constDeliminator, " ")+constDeliminator+
                encoded.genre.replace(constDeliminator, " ")+constDeliminator+
                QString::number(song.time)+constDeliminator+
                QString::number(song.year)+constDeliminator+
                QString::number(song.track)+constDeliminator+
                QString::number(song.disc)+constDeliminator+
                QString::number(song.type);
    return encoded;
}

bool OnlineService::decode(Song &song)
{
    if (!song.file.startsWith(QLatin1String("http://"))) {
        return false;
    }

    int pos=song.file.indexOf(constUrlGuard);

    if (pos>0) {
        QStringList parts=song.file.mid(pos+constUrlGuard.length()).split(constDeliminator);
        if (parts.length()>=10) {
            song.artist=parts.at(0);
            song.albumartist=parts.at(1);
            song.album=parts.at(2);
            song.title=parts.at(3);
            song.genre=parts.at(4);
            song.time=parts.at(5).toUInt();
            song.year=parts.at(6).toUInt();
            song.track=parts.at(7).toUInt();
            song.disc=parts.at(8).toUInt();
            song.type=(Song::Type)parts.at(9).toUInt();
            song.file=song.file.left(pos);
            return true;
        }
    }
    return false;
}

OnlineService::OnlineService(MusicModel *m, const QString &name)
    : OnlineServiceMusicRoot(name)
    , configured(false)
    , update(0)
    , lProgress(0.0)
    , loaded(false)
    , loader(0)
{
    setUseArtistImages(false); // Settings::self()->libraryArtistImage());
    setUseAlbumImages(true);
    setLargeImages(false);
    m_model=m;
    icn.addFile(":"+name.toLower());
}

void OnlineService::destroy()
{
    stopLoader();
    deleteLater();
}

void OnlineService::stopLoader()
{
    // Load music in a separate thread...
    if (loader) {
        disconnect(loader, SIGNAL(error(QString)), this, SLOT(loaderError(QString)));
        disconnect(loader, SIGNAL(status(QString,int)), this, SLOT(loaderstatus(QString,int)));
        disconnect(loader, SIGNAL(loaded()), this, SLOT(loaderFinished()));
        loader->stop();
        loader->deleteLater();
        loader=0;
    }
}

void OnlineService::reload(const bool fromCache)
{
    if (loader) {
        return;
    }

    clear();
    QString cache=cacheFileName(this, true);
    if (!fromCache && QFile::exists(cache)) {
        QFile::remove(cache);
    }
    createLoader();
    if (!loader) {
        return;
    }
    connect(loader, SIGNAL(error(QString)), this, SLOT(loaderError(QString)));
    connect(loader, SIGNAL(status(QString,int)), this, SLOT(loaderstatus(QString,int)));
    connect(loader, SIGNAL(loaded()), this, SLOT(loaderFinished()));
    loader->setCacheFileName(cache);
    loader->start();
    setBusy(true);
}

void OnlineService::toggle()
{
    if (loaded) {
        clear();
    } else {
        reload(true);
    }
}

void OnlineService::clear()
{
    loaded=false;
    int count=childCount();
    if (count>0) {
        m_model->beginRemoveRows(index(), 0, count-1);
        clearItems();
        m_model->endRemoveRows();
    }
    lProgress=0.0;
    setStatusMessage(QString());
    static_cast<OnlineServicesModel *>(m_model)->updateGenres();
}

void OnlineService::removeCache()
{
    QString cn=cacheFileName(this);
    if (QFile::exists(cn)) {
        QFile::remove(cn);
    }
}

void OnlineService::applyUpdate()
{
    if (update) {
        /*if (m_childItems.count() && update && update->childCount()) {
            QSet<Song> currentSongs=allSongs();
            QSet<Song> updateSongs=update->allSongs();
            QSet<Song> removed=currentSongs-updateSongs;
            QSet<Song> added=updateSongs-currentSongs;

            foreach (const Song &s, removed) {
                removeSongFromList(s);
            }
            foreach (const Song &s, added) {
                addSongToList(s);
            }
            delete update;
        } else*/ {
            int oldCount=childCount();
            if (oldCount>0) {
                m_model->beginRemoveRows(index(), 0, oldCount-1);
                clearItems();
                m_model->endRemoveRows();
            }
            int newCount=newRows();
            if (newCount>0) {
                m_model->beginInsertRows(index(), 0, newCount-1);
                foreach (MusicLibraryItem *item, update->childItems()) {
                    item->setParent(this);
                }
                refreshIndexes();
                m_model->endInsertRows();
            }
        }
        delete update;
        update=0;
        static_cast<OnlineServicesModel *>(m_model)->updateGenres();
        emitUpdated();
    }
    setBusy(false);
}

void OnlineService::loaderError(const QString &msg)
{
    loaded=true;
    lProgress=0;
    emit error(data()+QLatin1String(" - ")+msg);
    setStatusMessage(msg);
    stopLoader();
    setBusy(false);
}

void OnlineService::loaderstatus(const QString &msg, int prog)
{
    lProgress=-1==prog ? 0.0 : (abs(prog)/100.0);
    if (prog<0) {
        setStatusMessage(msg);
    } else {
        setStatusMessage(i18nc("Message percent", "%1 %2%", msg, prog));
    }
}

void OnlineService::loaderFinished()
{
    lProgress=100;
    loaded=true;
    setStatusMessage(QString());
    update=loader->takeLibrary();
    stopLoader();
    applyUpdate();
}

void OnlineService::setStatusMessage(const QString &msg)
{
    statusMsg=msg;
    QModelIndex modelIndex=index();
    emit m_model->dataChanged(modelIndex, modelIndex);
}

QModelIndex OnlineService::index() const
{
    return m_model->createIndex(m_model->row((void *)this), 0, (void *)this);
}

QModelIndex OnlineService::createIndex(MusicLibraryItem *child) const
{
    return m_model->createIndex(child->row(), 0, (void *)child);
}

void OnlineService::emitUpdated()
{
    emit static_cast<OnlineServicesModel *>(m_model)->updated(index());
}

void OnlineService::emitError(const QString &msg, bool isPodcastError)
{
    OnlineServicesModel *om=static_cast<OnlineServicesModel *>(m_model);

    if (isPodcastError && om->receivers(SIGNAL(podcastError(QString)))>0) {
        emit om->podcastError(msg);
    } else {
        emit om->error(msg);
    }
}

void OnlineService::emitDataChanged(const QModelIndex &idx)
{
    emit static_cast<OnlineServicesModel *>(m_model)->dataChanged(idx, idx);
}

//void OnlineService::emitNeedToSort()
//{
//    emit static_cast<OnlineServicesModel *>(m_model)->needToSort();
//}

void OnlineService::setBusy(bool b)
{
    static_cast<OnlineServicesModel *>(m_model)->setBusy(id(), b);
}

void OnlineService::beginInsertRows(const QModelIndex &idx, int from, int to)
{
    m_model->beginInsertRows(idx, from, to);
}

void OnlineService::endInsertRows()
{
    m_model->endInsertRows();
}

void OnlineService::beginRemoveRows(const QModelIndex &idx, int from, int to)
{
    m_model->beginRemoveRows(idx, from , to);
}

void OnlineService::endRemoveRows()
{
    m_model->endRemoveRows();
}
