/*
 * Cantata
 *
 * Copyright (c) 2011-2019 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpd-interface/song.h"
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>

static const QString constUrlGuard=QLatin1String("#{SONG_DETAILS}");
static const QString constDeliminator=QLatin1String("<@>");

static inline QString fixString(QString str)
{
    return str.replace(constDeliminator, " ").replace("\n", "");
}

static inline void add(QJsonObject &json, const QString &key, const QString &val)
{
    if (!val.isEmpty()) {
        json.insert(key, val);
    }
}

static inline void add(QJsonObject &json, const QString &key, int val)
{
    if (0!=val) {
        json.insert(key, val);
    }
}

Song & OnlineService::encode(Song &song)
{
    QJsonObject json;
    add(json, "artist", song.artist);
    add(json, "albumartist", song.albumartist);
    add(json, "album", song.album);
    add(json, "title", song.title);
    add(json, "genre", song.genres[0]);
    add(json, "time", song.time);
    add(json, "year", song.year);
    add(json, "track", song.track);
    add(json, "disc", song.disc);
    add(json, "service", song.onlineService());
    song.file=QUrl(song.file).toEncoded()+"#"+QJsonDocument(json).toJson(QJsonDocument::Compact);
    return song;
}

bool OnlineService::decode(Song &song)
{
    if (!song.file.startsWith(QLatin1String("http://")) && !song.file.startsWith(QLatin1String("https://"))) {
        return false;
    }

    int pos=song.file.indexOf(constUrlGuard);

    if (pos>0) { // Old (pre 2.3.1) style
        QStringList parts=song.file.mid(pos+constUrlGuard.length()).split(constDeliminator);
        if (parts.length()>=10) {
            song.artist=parts.at(0);
            song.albumartist=parts.at(1);
            song.album=parts.at(2);
            song.title=parts.at(3);
            song.genres[0]=parts.at(4);
            song.time=parts.at(5).toUInt();
            song.year=parts.at(6).toUInt();
            song.track=parts.at(7).toUInt();
            song.disc=parts.at(8).toUInt();
            song.type=Song::OnlineSvrTrack;
            song.setIsFromOnlineService(parts.at(9));
            song.file=song.file.left(pos);
            return true;
        }
    } else {
        pos = song.file.indexOf("#{");
        if (pos>0) {
            QJsonDocument doc = QJsonDocument::fromJson(song.file.mid(pos+1).toUtf8());
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                song.artist=obj["artist"].toString();
                song.albumartist=obj["albumartist"].toString();
                song.album=obj["album"].toString();
                song.title=obj["title"].toString();
                song.genres[0]=obj["genre"].toString();
                if (obj.contains("time")) song.track=obj["time"].toInt();
                if (obj.contains("year")) song.track=obj["year"].toInt();
                if (obj.contains("track")) song.track=obj["track"].toInt();
                if (obj.contains("disc")) song.track=obj["disc"].toInt();
                song.setIsFromOnlineService(obj["service"].toString());
                song.type=Song::OnlineSvrTrack;
                song.file=song.file.left(pos);
                return true;
            }
        }
    }
    return false;
}

static QSet<QString> servicesWithCovers;
static QSet<QString> servicesWithCoversIfCached;

QString OnlineService::iconPath(const QString &srv)
{
    QString file=QString(CANTATA_SYS_ICONS_DIR+srv+".png");
    if (!QFile::exists(file)) {
        file=QString(CANTATA_SYS_ICONS_DIR+srv+".svg");
    }
    return file;
}

bool OnlineService::isPodcasts(const QString &srv)
{
    return QLatin1String("podcasts")==srv;
}

bool OnlineService::showLogoAsCover(const Song &s)
{
    return s.isFromOnlineService() && !servicesWithCovers.contains(s.onlineService())
           && (!servicesWithCoversIfCached.contains(s.onlineService()) || s.extraField(Song::OnlineImageCacheName).isEmpty());
}

void OnlineService::useCovers(const QString &name, bool onlyIfCache)
{
    if (onlyIfCache) {
        servicesWithCoversIfCached.insert(name);
    } else {
        servicesWithCovers.insert(name);
    }
}
