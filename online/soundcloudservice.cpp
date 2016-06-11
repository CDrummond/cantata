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

#include "soundcloudservice.h"
#include "support/localize.h"
#include "network/networkaccessmanager.h"
#include "mpd-interface/mpdconnection.h"
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#include <QJsonDocument>
#else
#include "qjson/parser.h"
#endif

static const QLatin1String constName("soundcloud");
static const QLatin1String constApiKey("0cb23dce473528973ce74815bd36a334");
static const QLatin1String constUrl("https://api.soundcloud.com/tracks");

SoundCloudService::SoundCloudService(QObject *p)
    : OnlineSearchService(p)
{
    icn.addFile(":"+constName);
}

QString SoundCloudService::name() const
{
    return constName;
}

QString SoundCloudService::title() const
{
    return QLatin1String("SoundCloud");
}

QString SoundCloudService::descr() const
{
    return i18n("Search for tracks from soundcloud.com");
}

void SoundCloudService::search(const QString &key, const QString &value)
{
    Q_UNUSED(key);

    if (value==currentValue) {
        return;
    }

    clear();
    cancel();

    currentValue=value;

    if (currentValue.isEmpty()) {
        return;
    }

    QUrl searchUrl(constUrl);
    #if QT_VERSION < 0x050000
    QUrl &query=searchUrl;
    #else
    QUrlQuery query;
    #endif
    query.addQueryItem("client_id", constApiKey);
    query.addQueryItem("q", currentValue);
    #if QT_VERSION >= 0x050000
    searchUrl.setQuery(query);
    #endif

    QNetworkRequest req(searchUrl);
    req.setRawHeader("Accept", "application/json");
    job=NetworkAccessManager::self()->get(req);
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    emit searching();
    emit dataChanged(QModelIndex(), QModelIndex());
}

void SoundCloudService::jobFinished()
{
    NetworkJob *j=dynamic_cast<NetworkJob *>(sender());
    if (!j || j!=job) {
        return;
    }

    j->deleteLater();
    QList<Song> songs;

    if (j->ok()) {
        #if QT_VERSION >= 0x050000
        QVariant result=QJsonDocument::fromJson(j->readAll()).toVariant();
        #else
        QVariant result = QJson::Parser().parse(j->readAll());
        #endif
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
                if (QLatin1String("https")==url.scheme() && !MPDConnection::self()->urlHandlers().contains(QLatin1String("https"))) {
                    url.setScheme(QLatin1String("http"));
                }
                song.file=url.toString();
                // We don't have a real artist name, but username is the most similar thing we have
                song.artist=details["user"].toMap()["username"].toString();
                song.title=details["title"].toString();
                song.genre=details["genre"].toString();
                song.year=details["release_year"].toInt();
                song.time=details["duration"].toUInt()/1000;
                song.fillEmptyFields();
                songs.append(song);
            }
        }
    }
    results(songs);
    job=0;
    emit dataChanged(QModelIndex(), QModelIndex());
}
