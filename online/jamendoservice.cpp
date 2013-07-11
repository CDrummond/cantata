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

#include "jamendoservice.h"
#include "jamendosettingsdialog.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "song.h"
#include "settings.h"
#include <QXmlStreamReader>

static QString id3Genre(int id)
{
    static QMap<int, QString> idMap;

    if (idMap.isEmpty()) {
        idMap.insert(0, "Blues");
        idMap.insert(1, "Classic Rock");
        idMap.insert(2, "Country");
        idMap.insert(3, "Dance");
        idMap.insert(4, "Disco");
        idMap.insert(5, "Funk");
        idMap.insert(6, "Grunge");
        idMap.insert(7, "Hip-Hop");
        idMap.insert(8, "Jazz");
        idMap.insert(9, "Metal");
        idMap.insert(10, "New Age");
        idMap.insert(11, "Oldies");
        idMap.insert(12, "Other");
        idMap.insert(13, "Pop");
        idMap.insert(14, "R&B");
        idMap.insert(15, "Rap");
        idMap.insert(16, "Reggae");
        idMap.insert(17, "Rock");
        idMap.insert(18, "Techno");
        idMap.insert(19, "Industrial");
        idMap.insert(20, "Alternative");
        idMap.insert(21, "Ska");
        idMap.insert(22, "Death Metal");
        idMap.insert(23, "Pranks");
        idMap.insert(24, "Soundtrack");
        idMap.insert(25, "Euro-Techno");
        idMap.insert(26, "Ambient");
        idMap.insert(27, "Trip-Hop");
        idMap.insert(28, "Vocal");
        idMap.insert(29, "Jazz+Funk");
        idMap.insert(30, "Fusion");
        idMap.insert(31, "Trance");
        idMap.insert(32, "Classical");
        idMap.insert(33, "Instrumental");
        idMap.insert(34, "Acid");
        idMap.insert(35, "House");
        idMap.insert(36, "Game");
        idMap.insert(37, "Sound Clip");
        idMap.insert(38, "Gospel");
        idMap.insert(39, "Noise");
        idMap.insert(40, "AlternRock");
        idMap.insert(41, "Bass");
        idMap.insert(42, "Soul");
        idMap.insert(43, "Punk");
        idMap.insert(44, "Space");
        idMap.insert(45, "Meditative");
        idMap.insert(46, "Instrumental Pop");
        idMap.insert(47, "Instrumental Rock");
        idMap.insert(48, "Ethnic");
        idMap.insert(49, "Gothic");
        idMap.insert(50, "Darkwave");
        idMap.insert(51, "Techno-Industrial");
        idMap.insert(52, "Electronic");
        idMap.insert(53, "Pop-Folk");
        idMap.insert(54, "Eurodance");
        idMap.insert(55, "Dream");
        idMap.insert(56, "Southern Rock");
        idMap.insert(57, "Comedy");
        idMap.insert(58, "Cult");
        idMap.insert(59, "Gangsta");
        idMap.insert(60, "Top 40");
        idMap.insert(61, "Christian Rap");
        idMap.insert(62, "Pop/Funk");
        idMap.insert(63, "Jungle");
        idMap.insert(64, "Native American");
        idMap.insert(65, "Cabaret");
        idMap.insert(66, "New Wave");
        idMap.insert(67, "Psychadelic");
        idMap.insert(68, "Rave");
        idMap.insert(69, "Showtunes");
        idMap.insert(70, "Trailer");
        idMap.insert(71, "Lo-Fi");
        idMap.insert(72, "Tribal");
        idMap.insert(73, "Acid Punk");
        idMap.insert(74, "Acid Jazz");
        idMap.insert(75, "Polka");
        idMap.insert(76, "Retro");
        idMap.insert(77, "Musical");
        idMap.insert(78, "Rock & Roll");
        idMap.insert(79, "Hard Rock");
        idMap.insert(80, "Folk");
        idMap.insert(81, "Folk-Rock");
        idMap.insert(82, "National Folk");
        idMap.insert(83, "Swing");
        idMap.insert(84, "Fast Fusion");
        idMap.insert(85, "Bebob");
        idMap.insert(86, "Latin");
        idMap.insert(87, "Revival");
        idMap.insert(88, "Celtic");
        idMap.insert(89, "Bluegrass");
        idMap.insert(90, "Avantgarde");
        idMap.insert(91, "Gothic Rock");
        idMap.insert(92, "Progressive Rock");
        idMap.insert(93, "Psychedelic Rock");
        idMap.insert(94, "Symphonic Rock");
        idMap.insert(95, "Slow Rock");
        idMap.insert(96, "Big Band");
        idMap.insert(97, "Chorus");
        idMap.insert(98, "Easy Listening");
        idMap.insert(99, "Acoustic");
        idMap.insert(100, "Humour");
        idMap.insert(101, "Speech");
        idMap.insert(102, "Chanson");
        idMap.insert(103, "Opera");
        idMap.insert(104, "Chamber Music");
        idMap.insert(105, "Sonata");
        idMap.insert(106, "Symphony");
        idMap.insert(107, "Booty Bass");
        idMap.insert(108, "Primus");
        idMap.insert(109, "Porn Groove");
        idMap.insert(110, "Satire");
        idMap.insert(111, "Slow Jam");
        idMap.insert(112, "Club");
        idMap.insert(113, "Tango");
        idMap.insert(114, "Samba");
        idMap.insert(115, "Folklore");
        idMap.insert(116, "Ballad");
        idMap.insert(117, "Power Ballad");
        idMap.insert(118, "Rhythmic Soul");
        idMap.insert(119, "Freestyle");
        idMap.insert(120, "Duet");
        idMap.insert(121, "Punk Rock");
        idMap.insert(122, "Drum Solo");
        idMap.insert(123, "A capella");
        idMap.insert(124, "Euro-House");
        idMap.insert(125, "Dance Hall");
    }

    return idMap[id];
}

JamendoMusicLoader::JamendoMusicLoader(const QUrl &src)
    : OnlineMusicLoader(src)
{
}

bool JamendoMusicLoader::parse(QXmlStreamReader &xml)
{
    while (!xml.atEnd()) {
        if (wasStopped()) {
            delete library;
            library=0;
            return false;
        }
        xml.readNext();
        if (QXmlStreamReader::StartElement==xml.tokenType() && QLatin1String("artist")==xml.name()) {
            parseArtist(xml);
        }
    }
    return true;
}

void JamendoMusicLoader::parseArtist(QXmlStreamReader &xml)
{
    MusicLibraryItemArtist *artist=0;

    while (!xml.atEnd()) {
        if (wasStopped()) {
            return;
        }
        xml.readNext();

        if (QXmlStreamReader::StartElement==xml.tokenType()) {
            QStringRef name = xml.name();

            if (QLatin1String("name")==name) {
                Song s;
                s.artist=xml.readElementText().trimmed();
                artist=library->artist(s, true);
            } else if (artist && QLatin1String("album")==name) {
                parseAlbum(artist, xml);
            } else if (artist && QLatin1String("image")==name) {
                artist->setImageUrl(xml.readElementText().trimmed());
            }
        } else if (xml.isEndElement() && QLatin1String("artist")==xml.name()) {
            break;
        }
    }
}

void JamendoMusicLoader::parseAlbum(MusicLibraryItemArtist *artist, QXmlStreamReader &xml)
{
    MusicLibraryItemAlbum *album=0;

    while (!xml.atEnd()) {
        if (wasStopped()) {
            return;
        }
        xml.readNext();

        if (QXmlStreamReader::StartElement==xml.tokenType()) {
            QStringRef name = xml.name();

            if (QLatin1String("name")==name) {
                Song s;
                s.artist=artist->data();
                s.album=xml.readElementText().trimmed();
                album=artist->album(s, true);
            } else if (album && QLatin1String("track")==name) {
                parseSong(artist, album, xml);
            }  else if (album && QLatin1String("image")==name) {
                album->setImageUrl(xml.readElementText().trimmed());
            }
        } else if (xml.isEndElement() && QLatin1String("album")==xml.name()) {
            break;
        }
    }
}

void JamendoMusicLoader::parseSong(MusicLibraryItemArtist *artist, MusicLibraryItemAlbum *album, QXmlStreamReader &xml)
{
    Song s;
    s.artist=album->parentItem()->data();
    s.album=album->data();

    while (!xml.atEnd()) {
        if (wasStopped()) {
            return;
        }
        xml.readNext();

        if (QXmlStreamReader::StartElement==xml.tokenType()) {
            QStringRef name = xml.name();

            if (QLatin1String("name")==name) {
                s.title=xml.readElementText().trimmed();
            } else if (QLatin1String("duration")==name) {
                s.time=xml.readElementText().toFloat();
            } else if (QLatin1String("id3genre")==name) {
                s.genre=id3Genre(xml.readElementText().toInt());
            } else if (QLatin1String("id")==name) {
                s.file=xml.readElementText().trimmed();
            }
        } else if (xml.isEndElement() && QLatin1String("track")==xml.name()) {
            break;
        }
    }

    if (!s.title.isEmpty()) {
        s.fillEmptyFields();
        s.track=album->childItems().count()+1;
        MusicLibraryItemSong *song=new MusicLibraryItemSong(s, album);
        album->append(song);
        album->addGenre(s.genre);
        artist->addGenre(s.genre);
        library->addGenre(s.genre);
    }
}

const QLatin1String JamendoService::constName("Jamendo");
static const QLatin1String constStreamUrl("http://api.jamendo.com/get2/stream/track/redirect/?id=%1&streamencoding=");
void JamendoService::createLoader()
{
    loader=new JamendoMusicLoader(QUrl("http://img.jamendo.com/data/dbdump_artistalbumtrack.xml.gz"));
}

Song JamendoService::fixPath(const Song &orig) const
{
    Song s(orig);
    s.file=constStreamUrl;
    s.file.replace("id=%1", "id="+orig.file);
    s.file+=FMT_MP3==format ? QLatin1String("mp31") : QLatin1String("ogg2");
    s.genre=FMT_MP3==format ? QLatin1String("mp3") : QLatin1String("ogg");
    return s;
}

static const QLatin1String constMp3Format("mp3");
static const QLatin1String constOggFormat("ogg");

static QString formatStr(JamendoService::Format f)
{
    return JamendoService::FMT_MP3==f ? "mp3" : "ogg";
}

static JamendoService::Format toFormat(const QString &f)
{
    return f=="ogg" ? JamendoService::FMT_Ogg : JamendoService::FMT_MP3;
}

void JamendoService::loadConfig()
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), constName);
    #else
    QSettings cfg;
    cfg.beginGroup(constName);
    #endif
    format=toFormat(GET_STRING("format", formatStr(format)));
}

void JamendoService::saveConfig()
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), constName);
    #else
    QSettings cfg;
    cfg.beginGroup(constName);
    #endif
    SET_VALUE("format", formatStr(format));
    cfg.sync();
}

void JamendoService::configure(QWidget *p)
{
    JamendoSettingsDialog dlg(p);
    if (dlg.run(FMT_MP3==format)) {
        Format f=0==dlg.format() ? FMT_MP3 : FMT_Ogg;
        if (f!=format) {
            format=f;
            saveConfig();
        }
    }
}
