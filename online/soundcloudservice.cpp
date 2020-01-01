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

#include "soundcloudservice.h"
#include "gui/apikeys.h"
#include "network/networkaccessmanager.h"
#include "mpd-interface/mpdconnection.h"
#include "support/utils.h"
#include "support/monoicon.h"
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>

static const QLatin1String constName("soundcloud");
static const QLatin1String constUrl("https://api.soundcloud.com/tracks");

SoundCloudService::SoundCloudService(QObject *p)
    : OnlineSearchService(p)
{
    icn=MonoIcon::icon(FontAwesome::soundcloud, Utils::monoIconColor());
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
    return tr("Search for tracks from soundcloud.com");
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
    QUrlQuery query;
    ApiKeys::self()->addKey(query, ApiKeys::SoundCloud);
    query.addQueryItem("q", currentValue);
    query.addQueryItem("limit", QString::number(200));
    searchUrl.setQuery(query);

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
        QVariant result=QJsonDocument::fromJson(j->readAll()).toVariant();
        if (result.isValid()) {
            QVariantList list = result.toList();
            for (const QVariant &item: list) {
                QVariantMap details=item.toMap();
                if (details["title"].toString().isEmpty()) {
                    continue;
                }
                Song song;
                QUrl url = details["stream_url"].toUrl();
                QUrlQuery query;
                ApiKeys::self()->addKey(query, ApiKeys::SoundCloud);
                url.setQuery(query);
                // MPD does not seem to support https :-(
                if (QLatin1String("https")==url.scheme() && !MPDConnection::self()->urlHandlers().contains(QLatin1String("https"))) {
                    url.setScheme(QLatin1String("http"));
                }
                song.file=url.toString();
                // We don't have a real artist name, but username is the most similar thing we have
                song.artist=details["user"].toMap()["username"].toString();
                song.title=details["title"].toString();
                song.genres[0]=details["genre"].toString();
                song.year=details["release_year"].toInt();
                song.time=details["duration"].toUInt()/1000;
                song.fillEmptyFields();
                songs.append(song);
            }
        }
    } else {
        ApiKeys::self()->isLimitReached(j->actualJob(), ApiKeys::SoundCloud);
    }
    results(songs);
    job=nullptr;
    emit dataChanged(QModelIndex(), QModelIndex());
}

#include "moc_soundcloudservice.cpp"
