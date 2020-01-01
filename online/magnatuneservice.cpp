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

#include "magnatuneservice.h"
#include "magnatunesettingsdialog.h"
#include "db/onlinedb.h"
#include "models/roles.h"
#include "support/icon.h"
#include "support/configuration.h"
#include "support/monoicon.h"
#include <QXmlStreamReader>
#include <QUrl>

int MagnatuneXmlParser::parse(QXmlStreamReader &xml)
{
    QList<Song> *songList=new QList<Song>();
    while (!xml.atEnd()) {
        xml.readNext();
        if (QXmlStreamReader::StartElement==xml.tokenType() && QLatin1String("Track")==xml.name()) {
            songList->append(parseSong(xml));
            if (songList->count()>500) {
                emit songs(songList);
                songList=new QList<Song>();
            }
        }
    }
    if (songList->isEmpty()) {
        delete songList;
    } else {
        emit songs(songList);
    }
    return artists.count();
}

Song MagnatuneXmlParser::parseSong(QXmlStreamReader &xml)
{
    Song s;
//    QString artistImg;
    QString albumImg;

    while (!xml.atEnd()) {
        xml.readNext();

        if (QXmlStreamReader::StartElement==xml.tokenType()) {
            QStringRef name = xml.name();
            QString value = xml.readElementText(QXmlStreamReader::SkipChildElements);

            if (QLatin1String("artist")==name) {
                s.artist=value;
            } else if (QLatin1String("albumname")==name) {
                s.album=value;
            } else if (QLatin1String("trackname")==name) {
                s.title=value;
            } else if (QLatin1String("tracknum")==name) {
                s.track=value.toInt();
            } else if (QLatin1String("year")==name) {
                s.year=value.toInt();
            } else  if (QLatin1String("magnatunegenres")==name) {
                QStringList genres=value.split(',', QString::SkipEmptyParts);
//                for (const QString &g: genres) {
//                    s.addGenre(g);
//                }
                s.genres[0]=genres.first();
            } else if (QLatin1String("seconds")==name) {
                 s.time=value.toInt();
//            } else if (QLatin1String("cover_small")==name) {
//            } else if (QLatin1String("albumsku")==name) {
            } else if (QLatin1String("url")==name) {
                s.file=value;
            } /*else if (QLatin1String("artistphoto")==name) {
                artistImg=value;
            }*/ else if (QLatin1String("cover_small")==name) {
                albumImg=value;
            }
        } else if (QXmlStreamReader::EndElement==xml.tokenType()) {
            break;
        }
    }

    if (!albumImg.isEmpty()) {
        QString key=s.albumArtistOrComposer()+"-"+s.albumId();
        if (!albumUrls.contains(key)) {
            albumUrls.insert(key);
            emit coverUrl(s.albumArtistOrComposer(), s.album, albumImg);
        }
    }
    artists.insert(s.albumArtistOrComposer());
    return s;
}

static const QLatin1String constListingUrl("http://magnatune.com/info/song_info_xml.gz");
static const QLatin1String constName("magnatune");
static const char * constStreamingHostname = "streaming.magnatune.com";
static const char * constDownloadHostname = "download.magnatune.com";

QString MagnatuneService::membershipStr(MemberShip f, bool trans)
{
    switch (f) {
    default:
    case MB_None :      return trans ? tr("None") : QLatin1String("none");
    case MB_Streaming : return trans ? tr("Streaming") : QLatin1String("streaming");
//    case MB_Download :  return trans ? tr("Download") : QLatin1String("download"); // TODO: Magnatune downloads!
    }
}

static MagnatuneService::MemberShip toMembership(const QString &f)
{
    for (int i=0; i<=MagnatuneService::MB_Count; ++i) {
        if (f==MagnatuneService::membershipStr((MagnatuneService::MemberShip)i)) {
            return (MagnatuneService::MemberShip)i;
        }
    }
    return MagnatuneService::MB_None;
}

QString MagnatuneService::downloadTypeStr(DownloadType f, bool trans)
{
    switch (f) {
    default:
    case DL_Mp3 :    return trans ? tr("MP3 128k") : QLatin1String("mp3");
    case DL_Mp3Vbr : return trans ? tr("MP3 VBR") : QLatin1String("vbr");
    case DL_Ogg :    return trans ? tr("Ogg Vorbis") : QLatin1String("ogg");
    case DL_Flac :   return trans ? tr("FLAC"): QLatin1String("flac");
    case DL_Wav :    return trans ? tr("WAV") : QLatin1String("wav");
    }
}

static MagnatuneService::DownloadType toDownloadType(const QString &f)
{
    for (int i=0; i<=MagnatuneService::DL_Count; ++i) {
        if (f==MagnatuneService::downloadTypeStr((MagnatuneService::DownloadType)i)) {
            return (MagnatuneService::DownloadType)i;
        }
    }
    return MagnatuneService::DL_Mp3;
}

MagnatuneService::MagnatuneService(QObject *p)
    : OnlineDbService(new OnlineDb(constName, p), p)
{
    icn=MonoIcon::icon(":magnatune.svg", Utils::monoIconColor());
    useCovers(name());
    Configuration cfg(constName);
    membership=toMembership(cfg.get("membership", membershipStr(MB_None)));
    download=toDownloadType(cfg.get("download", downloadTypeStr(DL_Mp3)));
    username=cfg.get("username", username);
    password=cfg.get("username", password);
}

QVariant MagnatuneService::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        switch (role) {
        case Cantata::Role_CoverSong: {
            QVariant v;
            Item *item = static_cast<Item *>(index.internalPointer());
            switch (item->getType()) {
            case T_Album:
                if (item->getSong().isEmpty()) {
                    Song song;
                    song.artist=item->getParent()->getId();
                    song.album=item->getId();
                    song.setIsFromOnlineService(constName);
                    song.file=constName; // Just so that isEmpty() is false!
                    QString url=static_cast<OnlineDb *>(db)->getCoverUrl(/*T_Album==topLevel() ? static_cast<AlbumItem *>(item)->getArtistId() : */item->getParent()->getId(), item->getId());
                    song.setExtraField(Song::OnlineImageUrl, url);
                    item->setSong(song);
                }
                v.setValue<Song>(item->getSong());
                break;
            case T_Artist:
                break;
            default:
                break;
            }
            return v;
        }
        }
    }
    return OnlineDbService::data(index, role);
}

QString MagnatuneService::name() const
{
    return constName;
}

QString MagnatuneService::title() const
{
    return QLatin1String("Magnatune");
}

QString MagnatuneService::descr() const
{
    return tr("Online music from magnatune.com");
}

OnlineXmlParser * MagnatuneService::createParser()
{
    return new MagnatuneXmlParser();
}

QUrl MagnatuneService::listingUrl() const
{
    return QUrl(constListingUrl);
}

Song & MagnatuneService::fixPath(Song &s) const
{
    s.type=Song::OnlineSvrTrack;
    if (MB_None!=membership) {
        QUrl url(s.file);
        url.setScheme("http");
        url.setHost(MB_Streaming==membership ? constStreamingHostname : constDownloadHostname);
        url.setUserName(username);
        url.setPassword(password);

        // And remove the commercial
        QString path = url.path();
        path.insert(path.lastIndexOf('.'), "_nospeech");
        url.setPath(path);
        s.file=url.toString();
    // TODO: Magnatune downloads!
    //    if (MB_Download==membership) {
    //        ???
    //    }
    }
    s.setIsFromOnlineService(name());
    return encode(s);
}

void MagnatuneService::configure(QWidget *p)
{
    MagnatuneSettingsDialog dlg(p);
    if (dlg.run(membership, download, username, password) &&
        (username!=dlg.username() || password!=dlg.password() || membership!=dlg.membership()  || download!=dlg.download())) {
        username=dlg.username();
        password=dlg.password();
        membership=(MemberShip)dlg.membership();
        download=(DownloadType)dlg.download();
        Configuration cfg(constName);

        cfg.set("membership",  membershipStr(membership));
        cfg.set("download",  downloadTypeStr(download));
        cfg.set("username", username);
        cfg.set("password", password);    }
}

#include "moc_magnatuneservice.cpp"
