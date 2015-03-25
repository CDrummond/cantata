/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include "config.h"
#include "song.h"
#include "models/musiclibraryitemalbum.h"
#include "support/localize.h"
#if !defined CANTATA_NO_UI_FUNCTIONS && defined ENABLE_ONLINE_SERVICES
#include "online/onlineservice.h"
#endif
#include <QStringList>
#include <QSet>
#include <QChar>
#include <QLatin1Char>
#include <QtAlgorithms>
#include <QUrl>

//static const quint8 constOnlineDiscId=0xEE;

const QString Song::constCddaProtocol=QLatin1String("cantata-cdda:/");
const QString Song::constMopidyLocal=QLatin1String("local:track:");

static QString unknownStr;
static QString variousArtistsStr;
const QString & Song::unknown()
{
    return unknownStr;
}

const QString & Song::variousArtists()
{
    return variousArtistsStr;
}

void Song::initTranslations()
{
    unknownStr=i18n("Unknown");
    variousArtistsStr=i18n("Various Artists");
}

// When displaying albums, we use the 1st track's year as the year of the album.
// The map below stores the mapping from artist+album to year.
// This way the grouped view can find this quickly...
static QHash<QString, quint16> albumYears;

void Song::storeAlbumYear(const Song &s)
{
    albumYears.insert(s.albumKey(), s.year);
}

int Song::albumYear(const Song &s)
{
    QHash<QString, quint16>::ConstIterator it=albumYears.find(s.albumKey());
    return it==albumYears.end() ? s.year : it.value();
}

static int songType(const Song &s)
{
    static QStringList extensions=QStringList() << QLatin1String(".flac")
                                                << QLatin1String(".wav")
                                                << QLatin1String(".dff")
                                                << QLatin1String(".dsf")
                                                << QLatin1String(".aac")
                                                << QLatin1String(".m4a")
                                                << QLatin1String(".m4b")
                                                << QLatin1String(".m4p")
                                                << QLatin1String(".mp4")
                                                << QLatin1String(".ogg")
                                                << QLatin1String(".opus")
                                                << QLatin1String(".mp3")
                                                << QLatin1String(".wma");

    for (int i=0; i<extensions.count(); ++i) {
        if (s.file.endsWith(extensions.at(i), Qt::CaseInsensitive)) {
            return i;
        }
    }

    if (s.isCdda()) {
        return extensions.count()+2;
    }
    if (s.isStream()) {
        return extensions.count()+3;
    }
    return extensions.count()+1;
}

static bool songTypeSort(const Song &s1, const Song &s2)
{
    int t1=songType(s1);
    int t2=songType(s2);
    return t1<t2 || (t1==t2 && s1.id<s2.id);
}

void Song::sortViaType(QList<Song> &songs)
{
    qSort(songs.begin(), songs.end(), songTypeSort);
}

QString Song::decodePath(const QString &file)
{
    return file.startsWith(constMopidyLocal) ? QUrl::fromPercentEncoding(file.mid(constMopidyLocal.length()).toLatin1()) : file;
}

QString Song::encodePath(const QString &file)
{
    return constMopidyLocal+QString(QUrl::toPercentEncoding(file, "/"));
}

static QSet<QString> compGenres;

const QSet<QString> &Song::composerGenres()
{
    return compGenres;
}

void Song::setComposerGenres(const QSet<QString> &g)
{
    compGenres=g;
}

Song::Song()
    : extraFields(0)
    , disc(0)
    , priority(0)
    , time(0)
    , track(0)
    , year(0)
    , type(Standard)
    , guessed(false)
    , id(-1)
    , size(0)
    , rating(Rating_Null)
    , key(Null_Key)
{
}

Song & Song::operator=(const Song &s)
{
    id = s.id;
    file = s.file;
    time = s.time;
    album = s.album;
    artist = s.artist;
    albumartist = s.albumartist;
    title = s.title;
    track = s.track;
//     pos = s.pos;
    disc = s.disc;
    priority = s.priority;
    year = s.year;
    genre = s.genre;
    size = s.size;
    rating = s.rating;
    key = s.key;
    type = s.type;
    guessed = s.guessed;
    extra = s.extra;
    extraFields = s.extraFields;
    return *this;
}

bool Song::operator==(const Song &o) const
{
    return 0==compareTo(o);
}

bool Song::operator<(const Song &o) const
{
    return compareTo(o)<0;
}

int Song::compareTo(const Song &o) const
{
    //
    // NOTE: This compare DOES NOT use XXXSORT tags
    //

    if (type!=o.type) {
        return type<o.type ? -1 : 1;
    }

    // For playlists, we only need to compare filename...
    if (Playlist!=type) {
        if (SingleTracks==type) {
            int compare=artistSong().localeAwareCompare(artistSong());
            if (0!=compare) {
                return compare<0;
            }
        }

        int compare=artistOrComposer().localeAwareCompare(o.artistOrComposer());
        if (0!=compare) {
            return compare;
        }
        compare=album.localeAwareCompare(o.album);
        if (0!=compare) {
            return compare;
        }
        compare=mbAlbumId().compare(o.mbAlbumId());
        if (0!=compare) {
            return compare;
        }
        if (disc!=o.disc) {
            return disc<o.disc ? -1 : 1;
        }
        if (track!=o.track) {
            return track<o.track ? -1 : 1;
        }
        if (year!=o.year) {
            return year<o.year ? -1 : 1;
        }
        compare=title.localeAwareCompare(o.title);
        if (0!=compare) {
            return compare;
        }
        compare=name().compare(o.name());
        if (0!=compare) {
            return compare;
        }
        compare=genre.compare(o.genre);
        if (0!=compare) {
            return compare;
        }
        if (time!=o.time) {
            return time<o.time ? -1 : 1;
        }
    }
    return file.compare(o.file);
}

bool Song::isEmpty() const
{
    return (artist.isEmpty() && album.isEmpty() && title.isEmpty() && name().isEmpty()) || file.isEmpty();
}

void Song::guessTags()
{
    if (isEmpty() && !isStream()) {
        static const QLatin1String constAlbumArtistSep(" - ");
        guessed=true;
        QStringList parts = file.split("/", QString::SkipEmptyParts);
        if (3==parts.length()) {
            title=parts.at(2);
            album=parts.at(1);
            artist=parts.at(0);
        } if (2==parts.length() && parts.at(0).contains(constAlbumArtistSep)) {
            title=parts.at(1);
            QStringList albumArtistParts = parts.at(0).split(constAlbumArtistSep, QString::SkipEmptyParts);
            if (2==albumArtistParts.length()) {
                album=albumArtistParts.at(1);
                artist=albumArtistParts.at(0);
            }
        } else if (!parts.isEmpty()) {
            title=parts.at(parts.length()-1);
        }

        if (!title.isEmpty()) {
            int dot=title.lastIndexOf('.');
            if (dot>0 && dot<title.length()-2) {
                title=title.left(dot);
            }
            static const QSet<QChar> constSeparators=QSet<QChar>() << QLatin1Char(' ') << QLatin1Char('-') << QLatin1Char('_') << QLatin1Char('.');
            int separator=0;

            foreach (const QChar &sep, constSeparators) {
                separator=title.indexOf(sep);
                if (1==separator || 2==separator) {
                    break;
                }
            }

            if ( (1==separator && title[separator-1].isDigit()) ||
                 (2==separator && title[separator-2].isDigit() && title[separator-1].isDigit()) ) {
                if (0==track) {
                    track=title.left(separator).toInt();
                }
                title=title.mid(separator+1);

                while (!title.isEmpty() && constSeparators.contains(title[0])) {
                    title=title.mid(1);
                }
            }
        }
    }
}

void Song::revertGuessedTags()
{
    title=artist=album=unknownStr;
}

void Song::fillEmptyFields()
{
    if (artist.isEmpty()) {
        artist = unknownStr;
    }
    if (album.isEmpty()) {
        album = unknownStr;
    }
    if (title.isEmpty()) {
        title = unknownStr;
    }
    if (genre.isEmpty()) {
        genre = unknownStr;
    }
}

struct KeyStore
{
    KeyStore() : currentKey(0) { }
    quint16 currentKey;
    QHash<QString, quint16> keys;
};

static QHash<int, KeyStore> storeMap;

void Song::clearKeyStore(int location)
{
    storeMap.remove(location);
}

QString Song::displayAlbum(const QString &albumName, quint16 albumYear)
{
    return albumYear>0 ? albumName+QLatin1String(" (")+QString::number(albumYear)+QLatin1Char(')') : albumName;
}

QString Song::combineGenres(const QSet<QString> &genres)
{
    if (1==genres.size()) {
        return *(genres.begin());
    }

    QStringList list=genres.toList();
    qSort(list);
    QString g;
    foreach (const QString &e, list) {
        if (!g.isEmpty()) {
            g+=constGenreSep;
        }
        g+=e;
    }
    return g;
}

void Song::setKey(int location)
{
    if (isStandardStream()) {
        key=0;
        return;
    }

    KeyStore &store=storeMap[location];
    QString songKey(albumKey());
    QHash<QString, quint16>::ConstIterator it=store.keys.find(songKey);
    if (it!=store.keys.end()) {
        key=it.value();
    } else {
        store.currentKey++; // Key 0 is for streams, so we need to increment before setting...
        store.keys.insert(songKey, store.currentKey);
        key=store.currentKey;
    }
}

bool Song::isUnknown() const
{
    return (artist.isEmpty() || artist==unknownStr) && (album.isEmpty() || album==unknownStr) && (title.isEmpty() || title==unknownStr);
}

void Song::clear()
{
    id = -1;
    file.clear();
    time = 0;
    album.clear();
    artist.clear();
    title.clear();
    track = 0;
//     pos = 0;
    disc = 0;
    year = 0;
    genre.clear();
    size = 0;
    extra.clear();
    type = Standard;
}

const QLatin1Char Song::constGenreSep(',');

void Song::addGenre(const QString &g)
{
    if (g.isEmpty()) {
        return;
    }
    if (!genre.isEmpty()) {
        genre+=constGenreSep;
    }
    genre+=g.trimmed();
}

QStringList Song::genres() const
{
    return genre.split(constGenreSep, QString::SkipEmptyParts);
}

void Song::orderGenres()
{
    QStringList g=genres();
    if (g.count()>1) {
        qSort(g);
        genre=g.join(QString()+constGenreSep);
    }
}

QString Song::entryName() const
{
    if (title.isEmpty()) {
        return file;
    }

    return i18nc("Song\nArtist\nAlbum", "%1\n%2\n%3", title, artist, album);
}

QString Song::artistOrComposer() const
{
    if (!compGenres.isEmpty()) {
        QString c=composer();
        if (compGenres.contains(genre) && !c.isEmpty()) {
            return c;
        }
    }

    return albumArtist();
}

QString Song::albumName() const
{
    if (!compGenres.isEmpty()) {
        QString c=composer();
        if (compGenres.contains(genre) && !c.isEmpty() && c!=albumArtist()) {
            return album+QLatin1String(" (")+albumArtist()+QLatin1Char(')');
        }
    }
    return album;
}

QString Song::albumId() const
{
    QString mb=mbAlbumId();
    return mb.isEmpty() ? album : mb;
}

QString Song::artistSong() const
{
    //return artist+QLatin1String(" - ")+title;
    return title+QLatin1String(" - ")+artist;
}

QString Song::trackAndTitleStr(bool showArtistIfDifferent) const
{
    #if !defined CANTATA_NO_UI_FUNCTIONS && defined ENABLE_ONLINE_SERVICES
    if ((OnlineSvrTrack==type || Song::CantataStream) && OnlineService::showLogoAsCover(*this)) {
        return artistSong();
    }
    #endif
//    if (isFromOnlineService()) {
//        return (disc>0 && disc!=constOnlineDiscId ? (QString::number(disc)+QLatin1Char('.')) : QString())+
//               (track>0 ? (track>9 ? QString::number(track) : (QLatin1Char('0')+QString::number(track))) : QString())+
//               QLatin1Char(' ')+(addArtist ? artistSong() : title);
//    }
    return //(disc>0 ? (QString::number(disc)+QLatin1Char('.')) : QString())+
           (track>0 ? (track>9 ? QString::number(track) : (QLatin1Char('0')+QString::number(track))) : QString())+
           QLatin1Char(' ')+(showArtistIfDifferent && diffArtist() ? artistSong() : title);
}

#ifndef CANTATA_NO_UI_FUNCTIONS
static void addField(const QString &name, const QString &val, QString &tt)
{
    if (!val.isEmpty()) {
        tt+=QString("<tr><td align=\"right\"><b>%1:&nbsp;&nbsp;</b></td><td>%2</td></tr>").arg(name).arg(val);
    }
}
#endif

QString Song::toolTip() const
{
    #ifdef CANTATA_NO_UI_FUNCTIONS
    return QString();
    #else
    QString toolTip=QLatin1String("<table>");
    addField(i18n("Title"), title, toolTip);
    addField(i18n("Artist"), artist, toolTip);
    if (albumartist!=artist) {
        addField(i18n("Album artist"), albumartist, toolTip);
    }
    addField(i18n("Composer"), composer(), toolTip);
    addField(i18n("Performer"), performer(), toolTip);
    addField(i18n("Album"), album, toolTip);
    if (track>0) {
        addField(i18n("Track number"), QString::number(track), toolTip);
    }
    if (disc>0) {
        addField(i18n("Disc number"), QString::number(disc), toolTip);
    }
    addField(i18n("Genre"), displayGenre(), toolTip);
    if (year>0) {
        addField(i18n("Year"), QString::number(year), toolTip);
    }
    if (time>0) {
        addField(i18n("Length"), Utils::formatTime(time, true), toolTip);
    }
    toolTip+=QLatin1String("</table>");

    if (isNonMPD()) {
        return toolTip;
    }
    return toolTip+QLatin1String("<br/><br/><small><i>")+filePath()+QLatin1String("</i></small>");
    #endif
}

void Song::setExtraField(quint16 f, const QString &v)
{
    if (v.isEmpty()) {
        extra.remove(f);
        extraFields&=~f;
    } else {
        extra[f]=v;
        extraFields|=f;
    }
}

bool Song::isVariousArtists(const QString &str)
{
    return QLatin1String("Various Artists")==str || variousArtistsStr==str;
}

bool Song::diffArtist() const
{
    return isVariousArtists() || (!albumartist.isEmpty() && !artist.isEmpty() && albumartist!=artist);
}

bool Song::fixVariousArtists()
{
    if (isVariousArtists()) {
        artist.replace(" - ", ", ");
        title=artistSong();
        artist=albumartist;
        return true;
    }
    return false;
}

bool Song::revertVariousArtists()
{
    if (artist==albumartist) { // Then real artist is embedded in track title...
        int sepPos=title.indexOf(QLatin1String(" - "));
        if (sepPos>0 && sepPos<title.length()-3) {
            artist=title.left(sepPos);
            title=title.mid(sepPos+3);
            return true;
        }
    }

    return false;
}

bool Song::setAlbumArtist()
{
    if (!artist.isEmpty() && albumartist.isEmpty()) {
        albumartist=artist;
        return true;
    }
    return false;
}

QString Song::capitalize(const QString &s)
{
    if (s.isEmpty()) {
        return s;
    }

    QStringList words = s.split(' ', QString::SkipEmptyParts);
    for (int i = 0; i < words.count(); i++) {
        QString word = words[i]; //.toLower();
        int j = 0;
        while ( ('('==word[j] || '['==word[j] || '{'==word[j]) && j < word.length()) {
            j++;
        }
        word[j] = word[j].toUpper();
        words[i] = word;
    }
    return words.join(" ");
}

bool Song::capitalise()
{
    QString origArtist=artist;
    QString origAlbumArtist=albumartist;
    QString origAlbum=album;
    QString origTitle=title;

    artist=capitalize(artist);
    albumartist=capitalize(albumartist);
    album=capitalize(album);
    title=capitalize(title);
    QString c=composer();
    if (!c.isEmpty()) {
        setComposer(capitalize(c));
    }
    /* Performer is not currently in tag editor...
    QString p=performer();
    if (!p.isEmpty()) {
        setPerformer(capitalize(p));
    }
    */

    return artist!=origArtist || albumartist!=origAlbumArtist || album!=origAlbum || title!=origTitle ||
           (!c.isEmpty() && c!=composer()) /*|| (!p.isEmpty() && p!=performer())*/ ;
}

QString Song::albumKey() const
{
    #if !defined CANTATA_NO_UI_FUNCTIONS && defined ENABLE_ONLINE_SERVICES
    if ((OnlineSvrTrack==type || Song::CantataStream) && OnlineService::showLogoAsCover(*this)) {
        return onlineService();
    }
    #endif
    return albumArtist()+QLatin1Char(':')+albumId()+QLatin1Char(':')+QString::number(disc);
}

QString Song::basicArtist() const
{
    if (!albumartist.isEmpty() && (artist.isEmpty() || albumartist==artist || (albumartist.length()<artist.length() && artist.startsWith(albumartist)))) {
        return albumartist;
    }

    QStringList toStrip=QStringList() << QLatin1String("ft. ") << QLatin1String("feat. ") << QLatin1String("featuring ") << QLatin1String("f. ");
    QStringList prefixes=QStringList() << QLatin1String(" ") << QLatin1String(" (") << QLatin1String(" [");

    foreach (const QString s, toStrip) {
        foreach (const QString p, prefixes) {
            int strip = artist.toLower().indexOf(p+s);
            if (-1!=strip) {
                return artist.mid(0, strip);
            }
        }
    }
    return artist;
}

QString Song::describe(bool withMarkup) const
{
    QString albumText=album.isEmpty() ? name() : displayAlbum(album, Song::albumYear(*this));

    if (title.isEmpty()) {
        return withMarkup ? albumText : (QLatin1String("<b>")+albumText+QLatin1String("</b>"));
    }
    QString descr=artist.isEmpty()
                    ? i18nc("Song on Album", "<b>%1</b> on <b>%2</b>", title, albumText)
                    : i18nc("Song by Artist on Album", "<b>%1</b> by <b>%2</b> on <b>%3</b>", title, artist, albumText);

    if (!withMarkup) {
        descr=descr.replace("<b>", "");
        descr=descr.replace("</b>", "");
    }
    return descr;
}

//QString Song::basicDescription() const
//{
//    return isStandardStream()
//            ? name
//            : title.isEmpty()
//                ? QString()
//                : artist.isEmpty()
//                    ? title
//                    : i18nc("track - artist", "%1 - %2", title, artist);
//}

#ifdef ENABLE_EXTERNAL_TAGS
QDataStream & operator<<(QDataStream &stream, const Song &song)
{
    stream << song.id << song.file << song.album << song.artist << song.albumartist << song.title
           << song.genre << song.disc << song.priority << song.time << song.track << (quint16)song.year
           << (quint16)song.type << (bool)song.guessed << song.size << song.extra;
    return stream;
}

QDataStream & operator>>(QDataStream &stream, Song &song)
{
    quint16 type;
    quint16 year;
    bool guessed;
    stream >> song.id >> song.file >> song.album >> song.artist >> song.albumartist >> song.title
           >> song.genre >> song.disc >> song.priority >> song.time >> song.track >> year
           >> type >> guessed >> song.size >> song.extra;
    song.type=(Song::Type)type;
    song.year=year;
    song.guessed=guessed;
    return stream;
}
#endif
