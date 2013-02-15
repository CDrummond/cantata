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

#include "onlineservice.h"
#include "onlineservicesmodel.h"
#include "musiclibrarymodel.h"
#include "utils.h"
#include "networkaccessmanager.h"
#include "mpdparseutils.h"
#include "covers.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QFile>
#include <QXmlStreamReader>

static QString cacheFileName(OnlineService *srv, bool create=false)
{
    return Utils::cacheDir(srv->name().toLower(), create)+QLatin1String("library")+MusicLibraryModel::constLibraryCompressedExt;
}

OnlineMusicLoader::OnlineMusicLoader(const QUrl &src)
    : source(src)
    , library(0)
    , network(0)
    , downloadJob(0)
    , stopRequested(false)
    , lastProg(-1)
{
    moveToThread(this);
}

void OnlineMusicLoader::run()
{
    if (library) {
        delete library;
    }
    library = new MusicLibraryItemRoot;
    if (!readFromCache()) {
        emit status(i18n("Dowloading"), 0);
        if (!network) {
            network=new NetworkAccessManager(this);
        }
        downloadJob=network->get(source);
        connect(downloadJob, SIGNAL(finished()), SLOT(downloadFinished()));
        connect(downloadJob, SIGNAL(downloadProgress(qint64,qint64)), SLOT(downloadProgress(qint64,qint64)));
        exec();
    }
}

void OnlineMusicLoader::stop()
{
    if (downloadJob) {
        quit();
    }
    stopRequested=true;
    Utils::stopThread(this);
}

MusicLibraryItemRoot * OnlineMusicLoader::takeLibrary()
{
    MusicLibraryItemRoot *lib=library;
    library=0;
    return lib;
}

bool OnlineMusicLoader::readFromCache()
{
    if (!cache.isEmpty() && QFile::exists(cache)) {
        emit status(i18n("Reading cache"), 0);
        if (library->fromXML(cache, QDateTime(), QString(), this)) {
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
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (stopRequested) {
        return;
    }

    if(QNetworkReply::NoError==reply->error()) {
        emit status(i18n("Parsing response"), -100);
        QtIOCompressor comp(reply);
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
    quit();
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

void OnlineService::destroy(bool delCache)
{
    if (delCache) {
        removeCache();
    }
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
    connect(loader, SIGNAL(error(QString)), this, SLOT(loaderError(QString)));
    connect(loader, SIGNAL(status(QString,int)), this, SLOT(loaderstatus(QString,int)));
    connect(loader, SIGNAL(loaded()), this, SLOT(loaderFinished()));
    loader->setCacheFileName(cache);
    loader->start();
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
        model->beginRemoveRows(model->createIndex(model->services.indexOf(this), 0, this), 0, count-1);
        clearItems();
        model->endRemoveRows();
    }
    lProgress=0.0;
    setStatusMessage(i18n("Not Loaded"));
    model->updateGenres();
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
    if (!update) {
        return;
    }
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
            model->beginRemoveRows(model->createIndex(model->services.indexOf(this), 0, this), 0, oldCount-1);
            clearItems();
            model->endRemoveRows();
        }
        int newCount=newRows();
        if (newCount>0) {
            model->beginInsertRows(model->createIndex(model->services.indexOf(this), 0, this), 0, newCount-1);
            foreach (MusicLibraryItem *item, update->childItems()) {
                item->setParent(this);
            }
            refreshIndexes();
            model->endInsertRows();
        }
    }
    delete update;
    update=0;
}

void OnlineService::loaderError(const QString &msg)
{
    lProgress=0;
    setStatusMessage(msg);
}

void OnlineService::loaderstatus(const QString &msg, int prog)
{
    lProgress=-1==prog ? 0.0 : (abs(prog)/100.0);
    if (prog<0) {
        setStatusMessage(msg);
    } else {
        setStatusMessage(i18nc("Message percent", "%1 %2%").arg(msg).arg(prog));
    }
}

void OnlineService::loaderFinished()
{
    lProgress=100;
    loaded=true;
    setStatusMessage(i18n("Loaded"));
    update=loader->takeLibrary();
    stopLoader();
    applyUpdate();
    model->updateGenres();
}

void OnlineService::setStatusMessage(const QString &msg)
{
    statusMsg=msg;
    QModelIndex modelIndex=model->createIndex(model->services.indexOf(this), 0, this);
    emit model->dataChanged(modelIndex, modelIndex);
}
