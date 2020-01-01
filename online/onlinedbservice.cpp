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

#include "onlinedbservice.h"
#include "gui/covers.h"
#include "models/roles.h"
#include "network/networkaccessmanager.h"
#include "qtiocompressor/qtiocompressor.h"
#include "db/onlinedb.h"
#include <QXmlStreamReader>

OnlineXmlParser::OnlineXmlParser()
{
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
    connect(this, SIGNAL(startParsing(NetworkJob*)), this, SLOT(doParsing(NetworkJob*)));
}

OnlineXmlParser::~OnlineXmlParser()
{
}

void OnlineXmlParser::start(NetworkJob *job)
{
    emit startParsing(job);
}

void OnlineXmlParser::doParsing(NetworkJob *job)
{
    QtIOCompressor comp(job->actualJob());
    comp.setStreamFormat(QtIOCompressor::GzipFormat);
    if (comp.open(QIODevice::ReadOnly)) {
        QXmlStreamReader reader;
        reader.setDevice(&comp);
        emit startUpdate();
        int artistCount=parse(reader);
        if (artistCount>0) {
            emit endUpdate();
            emit stats(artistCount);
        } else {
            emit error(tr("Failed to parse"));
            emit abortUpdate();
        }
    } else {
        emit error(tr("Failed to parse"));
    }
    emit complete();
}

OnlineDbService::OnlineDbService(LibraryDb *d, QObject *p)
    : SqlLibraryModel(d, p, T_Genre)
    , lastPc(-1)
    , job(nullptr)
{
    connect(Covers::self(), SIGNAL(cover(Song,QImage,QString)), this, SLOT(cover(Song,QImage,QString)));
}

QVariant OnlineDbService::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        switch (role) {
        case Cantata::Role_TitleText:
            return title();
        case Cantata::Role_SubText:
            if (!status.isEmpty()) {
                return status;
            }
            if (!stats.isEmpty()) {
                return stats;
            }
            return descr();
        case Qt::DecorationRole:
            return icon();
        }
    }
    return SqlLibraryModel::data(index, role);
}

bool OnlineDbService::previouslyDownloaded() const
{
    // Create DB, if it does not already exist
    static_cast<OnlineDb *>(db)->create();
    return 0!=db->getCurrentVersion();
}

void OnlineDbService::open()
{
    if (0==rowCount(QModelIndex())) {
        // Create DB, if it does not already exist
        static_cast<OnlineDb *>(db)->create();
        libraryUpdated();
        updateStats();
    }
}

void OnlineDbService::download(bool redownload)
{
    if (job) {
        return;
    }
    if (redownload || !previouslyDownloaded()) {
        job=NetworkAccessManager::self()->get(QUrl(listingUrl()));
        connect(job, SIGNAL(downloadPercent(int)), this, SLOT(downloadPercent(int)));
        connect(job, SIGNAL(finished()), this, SLOT(downloadFinished()));
        lastPc=-1;
        downloadPercent(0);
    }
}

void OnlineDbService::abort()
{
    if (job) {
        job->cancelAndDelete();
        job=nullptr;
    }
    db->abortUpdate();
}

void OnlineDbService::cover(const Song &song, const QImage &img, const QString &file)
{
    if (file.isEmpty() || img.isNull() || !song.isFromOnlineService() || song.onlineService()!=name()) {
        return;
    }

    const Item *genre=root ? root->getChild(song.genres[0]) : nullptr;
    if (genre) {
        const Item *artist=static_cast<const CollectionItem *>(genre)->getChild(song.albumArtistOrComposer());
        if (artist) {
            const Item *album=static_cast<const CollectionItem *>(artist)->getChild(song.albumId());
            if (album) {
                QModelIndex idx=index(album->getRow(), 0, index(artist->getRow(), 0, index(genre->getRow(), 0, QModelIndex())));
                emit dataChanged(idx, idx);
            }
        }
    }
}

void OnlineDbService::updateStatus(const QString &msg)
{
    status=msg;
    emit dataChanged(QModelIndex(), QModelIndex());
}

void OnlineDbService::downloadPercent(int pc)
{
    if (lastPc!=pc) {
        lastPc=pc;
        updateStatus(tr("Downloading...%1%").arg(pc));
    }
}

void OnlineDbService::downloadFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (!reply) {
        return;
    }

    if (reply!=job) {
        reply->deleteLater();
        return;
    }

    if (reply->ok()) {
        // Ensure DB is created
        static_cast<OnlineDb *>(db)->create();
        updateStatus(tr("Parsing music list...."));
        OnlineXmlParser *parser=createParser();
        db->clear();
        connect(parser, SIGNAL(startUpdate()), static_cast<OnlineDb *>(db), SLOT(startUpdate()));
        connect(parser, SIGNAL(endUpdate()), static_cast<OnlineDb *>(db), SLOT(endUpdate()));
        connect(parser, SIGNAL(abortUpdate()), static_cast<OnlineDb *>(db), SLOT(abortUpdate()));
        connect(parser, SIGNAL(stats(int)), static_cast<OnlineDb *>(db), SLOT(insertStats(int)));
        connect(parser, SIGNAL(coverUrl(QString,QString,QString)), static_cast<OnlineDb *>(db), SLOT(storeCoverUrl(QString,QString,QString)));
        connect(parser, SIGNAL(songs(QList<Song>*)), static_cast<OnlineDb *>(db), SLOT(insertSongs(QList<Song>*)));
        connect(parser, SIGNAL(complete()), job, SLOT(deleteLater()));
        connect(parser, SIGNAL(complete()), this, SLOT(updateStats()));
        connect(parser, SIGNAL(error(QString)), this, SIGNAL(error(QString)));
        connect(parser, SIGNAL(complete()), parser, SLOT(deleteLater()));
        parser->start(reply);
    } else {
        reply->deleteLater();
        updateStatus(QString());
        emit error(tr("Failed to download"));
    }
    job=nullptr;
}

void OnlineDbService::updateStats()
{
    int numArtists=static_cast<OnlineDb *>(db)->getStats();
    if (numArtists>0) {
        stats=tr("%n Artist(s)", "", numArtists);
    } else {
        stats=QString();
    }
    updateStatus(QString());
}

#include "moc_onlinedbservice.cpp"
