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

#include "soundcloudservice.h"
#include "networkaccessmanager.h"
#include "qjson/parser.h"
#include "onlineservicesmodel.h"
#include "musiclibraryitemsong.h"
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

const QLatin1String SoundCloudService::constName("SoundCloud");
static const QString constApiKey=QLatin1String("0cb23dce473528973ce74815bd36a334");
static const QString constHost=QLatin1String("api.soundcloud.com");
static const QString constUrl=QLatin1String("http://")+constHost+QLatin1Char('/');

SoundCloudService::SoundCloudService(MusicModel *m)
    : OnlineService(m, constName)
    , job(0)
{
    setUseArtistImages(false);
    setUseAlbumImages(false);
}

Song SoundCloudService::fixPath(const Song &orig, bool) const
{
    return encode(orig);
}

void SoundCloudService::clear()
{
    cancelAll();
    ::OnlineService::clear();
}

void SoundCloudService::setSearch(const QString &searchTerm)
{
    if (searchTerm==currentSearch) {
        return;
    }

    clear();

    currentSearch=searchTerm;

    if (currentSearch.isEmpty()) {
        return;
    }

    QUrl searchUrl(constUrl);
    #if QT_VERSION < 0x050000
    QUrl &query=searchUrl;
    #else
    QUrlQuery query;
    #endif
    searchUrl.setPath("tracks");
    query.addQueryItem("client_id", constApiKey);
    query.addQueryItem("q", searchTerm);
    #if QT_VERSION >= 0x050000
    searchUrl.setQuery(query);
    #endif

    QNetworkRequest req(searchUrl);
    req.setRawHeader("Accept", "application/json");
    job=NetworkAccessManager::self()->get(req);
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    emitUpdated();
    setBusy(true);
}

void SoundCloudService::cancelAll()
{
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        job->abort();
        job->deleteLater();
        job=0;
    }
    setBusy(false);
}

void SoundCloudService::jobFinished()
{
    QNetworkReply *j=dynamic_cast<QNetworkReply *>(sender());
    if (!j || j!=job) {
        return;
    }

    j->deleteLater();

    QJson::Parser parser;
    QVariant result = parser.parse(j);
    if (result.isValid()) {
        QVariantList list = result.toList();
        foreach(const QVariant &item, list) {
            QVariantMap details=item.toMap();
            if (details["title"].toString().isEmpty()) {
                continue;
            }
            Song song;
            QUrl url = details["stream_url"].toUrl();
            #if QT_VERSION < 0x050000
            QUrl &query=url;
            #else
            QUrlQuery query;
            #endif
            query.addQueryItem("client_id", constApiKey);
            #if QT_VERSION >= 0x050000
            url.setQuery(query);
            #endif
            // MPD does not seem to support https :-(
            if (QLatin1String("https")==url.scheme()) {
                url.setScheme(QLatin1String("http"));
            }
            song.file=url.toString();
            // We don't have a real artist name, but username is the most similar thing we have
            song.artist=details["user"].toMap()["username"].toString();
            song.title=details["title"].toString();
            song.genre=details["genre"].toString();
            song.year=details["release_year"].toInt();
            song.time=details["duration"].toUInt()/1000;
            if (!update) {
                update=new MusicLibraryItemRoot();
            }
            song.fillEmptyFields();
            update->append(new MusicLibraryItemSong(song, update));
        }
    }

    loaded=true;
    job=0;
    applyUpdate();
}
