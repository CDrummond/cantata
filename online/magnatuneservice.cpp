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

#include "magnatuneservice.h"
#include "magnatunesettingsdialog.h"
#include "models/musiclibraryitemartist.h"
#include "models/musiclibraryitemalbum.h"
#include "models/musiclibraryitemsong.h"
#include "mpd/song.h"
#include "support/configuration.h"
#include "support/localize.h"
#include <QXmlStreamReader>

MagnatuneMusicLoader::MagnatuneMusicLoader(const QUrl &src)
    : OnlineMusicLoader(src)
{
}

bool MagnatuneMusicLoader::parse(QXmlStreamReader &xml)
{
    while (!xml.atEnd()) {
        if (wasStopped()) {
            delete library;
            library=0;
            return false;
        }
        xml.readNext();
        if (QXmlStreamReader::StartElement==xml.tokenType() && QLatin1String("Track")==xml.name()) {
            parseSong(xml);
        }
    }
    return true;
}

void MagnatuneMusicLoader::parseSong(QXmlStreamReader &xml)
{
    Song s;
//    QString artistImg;
    QString albumImg;

    while (!xml.atEnd()) {
        if (wasStopped()) {
            return;
        }
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
                foreach (const QString &g, genres) {
                    s.addGenre(g);
                }
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

    if (!s.title.isEmpty()) {
        s.fillEmptyFields();
        MusicLibraryItemArtist *artist = library->artist(s);
        MusicLibraryItemAlbum *album = artist->album(s);
        MusicLibraryItemSong *song=new MusicLibraryItemSong(s, album);
        const QSet<QString> &songGenres=song->allGenres();
        album->append(song);
        album->addGenres(songGenres);
        artist->addGenres(songGenres);
        library->addGenres(songGenres);
//        if (!artistImg.isEmpty() && artist->imageUrl().isEmpty()) {
//            artist->setImageUrl(artistImg);
//        }
        if (!albumImg.isEmpty() && album->imageUrl().isEmpty()) {
            album->setImageUrl(albumImg);
        }
    }
}

const QLatin1String MagnatuneService::constName("Magnatune");
static const char * constStreamingHostname = "streaming.magnatune.com";
static const char * constDownloadHostname = "download.magnatune.com";

void MagnatuneService::createLoader()
{
    loader=new MagnatuneMusicLoader(QUrl("http://magnatune.com/info/song_info_xml.gz"));
}

Song MagnatuneService::fixPath(const Song &orig, bool) const
{
    Song s=orig;
    s.type=Song::OnlineSvrTrack;
    if (MB_None!=membership) {
        QUrl url;
        #if QT_VERSION < 0x050000
        url.setEncodedUrl(orig.file.toLocal8Bit());
        #else
        url=QUrl(orig.file);
        #endif
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
    //        s.genre=downloadTypeStr(download);
    //    }
    }
    return encode(s);
}

QString MagnatuneService::membershipStr(MemberShip f, bool trans)
{
    switch (f) {
    default:
    case MB_None :      return trans ? i18n("None") : QLatin1String("none");
    case MB_Streaming : return trans ? i18n("Streaming") : QLatin1String("streaming");
//    case MB_Download :  return trans ? i18n("Download") : QLatin1String("download"); // TODO: Magnatune downloads!
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
    case DL_Mp3 :    return trans ? i18n("MP3 128k") : QLatin1String("mp3");
    case DL_Mp3Vbr : return trans ? i18n("MP3 VBR") : QLatin1String("vbr");
    case DL_Ogg :    return trans ? i18n("Ogg Vorbis") : QLatin1String("ogg");
    case DL_Flac :   return trans ? i18n("FLAC"): QLatin1String("flac");
    case DL_Wav :    return trans ? i18n("WAV") : QLatin1String("wav");
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

void MagnatuneService::loadConfig()
{
    Configuration cfg(constName);

    membership=toMembership(cfg.get("membership", membershipStr(membership)));
    download=toDownloadType(cfg.get("download", downloadTypeStr(download)));
    username=cfg.get("username", username);
    password=cfg.get("username", password);
}

void MagnatuneService::saveConfig()
{
    Configuration cfg(constName);

    cfg.set("membership",  membershipStr(membership));
    cfg.set("download",  downloadTypeStr(download));
    cfg.set("username", username);
    cfg.set("password", password);
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
        saveConfig();
    }
}
