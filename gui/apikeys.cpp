/*
 * Cantata
 *
 * Copyright (c) 2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "apikeys.h"
#include "support/configuration.h"
#include "support/globalstatic.h"
#include <QObject>

GLOBAL_STATIC(ApiKeys, instance)

ApiKeys::ApiKeys()
{    
    defaultKeys[LastFm]="5a854b839b10f8d46e630e8287c2299b";
    defaultKeys[FanArt]="ee86404cb429fa27ac32a1a3c117b006";
    defaultKeys[Dirble]="1035d2834bdc7195b8929ad7f70a8410f02c633e";
    defaultKeys[Shoutcast]="fa1669MuiRPorUBw";
    defaultKeys[SoundCloud]="0cb23dce473528973ce74815bd36a334";

    queryItems[LastFm]="api_key";
    queryItems[FanArt]="api_key";
    queryItems[Dirble]="token";
    queryItems[Shoutcast]="k";
    queryItems[SoundCloud]="client_id";
}

void ApiKeys::load()
{
    Configuration cfg("ApiKeys");
    QList<Details> keys = getDetails();

    for (const auto &k: keys) {
        set(k.srv, cfg.get(k.name, QString()));
    }
}

void ApiKeys::save()
{
    Configuration cfg("ApiKeys");
    QList<Details> keys = getDetails();

    for (const auto &k: keys) {
        if (k.key.isEmpty()) {
            cfg.remove(k.name);
        } else {
            cfg.set(k.name, k.key);
        }
    }
}

QList<ApiKeys::Details> ApiKeys::getDetails()
{
    QList<Details> list;
    list.append(Details(LastFm, "LastFM", userKeys[LastFm], "https://www.last.fm/api"));
    list.append(Details(FanArt, "FanArt", userKeys[FanArt], "https://fanart.tv/get-an-api-key/"));
    list.append(Details(Dirble, "Dirble", userKeys[Dirble], "https://dirble.com/developer/"));
    list.append(Details(Shoutcast, "Shoutcast", userKeys[Shoutcast], "https://shoutcast.com/Developer"));
    list.append(Details(SoundCloud, "Soundcloud", userKeys[SoundCloud], "https://developers.soundcloud.com/"));
    return list;
}

const QString & ApiKeys::get(Service srv)
{
    if (srv>0 && srv<NumServices) {
        return userKeys[srv].isEmpty() ? defaultKeys[srv] : userKeys[srv];
    } else {
        return defaultKeys[0];
    }
}

void ApiKeys::set(Service srv, const QString &key)
{
    if (srv>0 && srv<NumServices) {
        userKeys[srv]=key.trimmed();
    }
}

void ApiKeys::addKey(QUrlQuery &query, Service srv)
{
    if (srv>0 && srv<NumServices) {
        query.addQueryItem(queryItems[srv], get(srv));
    }
}

QString ApiKeys::addKey(const QString &url, Service srv)
{
    if (srv>0 && srv<NumServices) {
        if (-1==url.indexOf("?")) {
            return url+"?"+queryItems[srv]+"="+get(srv);
        } else {
            return url+"&"+queryItems[srv]+"="+get(srv);
        }
    }
    return url;
}
