/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#if !defined CANTATA_NO_UI_FUNCTIONS
#include "online/onlineservice.h"
#include "online/podcastservice.h"
#endif
#include <QStringList>
#include <QSet>
#include <QChar>
#include <QLatin1Char>
#include <QtAlgorithms>
#include <QUrl>
#include <QMutex>
#include <QMutexLocker>
#include <algorithm>

//static const quint8 constOnlineDiscId=0xEE;

const QString Song::constCddaProtocol=QLatin1String("/[cantata-cdda]/");
const QString Song::constMopidyLocal=QLatin1String("local:track:");
const QString Song::constForkedDaapdLocal=QLatin1String("file:");

static QString unknownStr;
static QString variousArtistsStr;
static QString singleTracksStr;
static bool useOrigYear = false;

const QString & Song::unknown()
{
    return unknownStr;
}

const QString & Song::variousArtists()
{
    return variousArtistsStr;
}

const QString & Song::singleTracks()
{
    return singleTracksStr;
}

void Song::initTranslations()
{
    unknownStr=QObject::tr("Unknown");
    variousArtistsStr=QObject::tr("Various Artists");
    singleTracksStr=QObject::tr("Single Tracks");
}

// When displaying albums, we use the 1st track's year as the year of the album.
// The map below stores the mapping from artist+album to year.
// This way the grouped view can find this quickly...
static QHash<QString, quint16> albumYears;

void Song::storeAlbumYear(const Song &s)
{
    albumYears.insert(s.albumKey(), s.displayYear());
}

int Song::albumYear(const Song &s)
{
    QHash<QString, quint16>::ConstIterator it=albumYears.find(s.albumKey());
    return it==albumYears.end() ? s.displayYear() : it.value();
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
    std::sort(songs.begin(), songs.end(), songTypeSort);
}

QString Song::decodePath(const QString &file, bool cdda)
{
    if (cdda) {
        return QString(file).replace("/", "_").replace(":", "_");
    }
    if (file.startsWith(constMopidyLocal)) {
        return QUrl::fromPercentEncoding(file.mid(constMopidyLocal.length()).toLatin1());
    }
    if (file.startsWith(constForkedDaapdLocal)) {
        return file.mid(constForkedDaapdLocal.length());
    }
    return file;
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
    , priority(0)
    , disc(0)
    , blank(0)
    , time(0)
    , track(0)
    , origYear(0)
    , year(0)
    , type(Standard)
    , guessed(false)
    , id(-1)
    , size(0)
    , rating(Rating_Null)
    , lastModified(0)
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
    blank = s.blank;
    priority = s.priority;
    origYear = s.origYear;
    year = s.year;
    for (int i=0; i<constNumGenres; ++i) {
        genres[i]=s.genres[i];
    }
    size = s.size;
    rating = s.rating;
    key = s.key;
    type = s.type;
    guessed = s.guessed;
    extra = s.extra;
    extraFields = s.extraFields;
    lastModified = s.lastModified;
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

        int compare=albumArtistOrComposer().localeAwareCompare(o.albumArtistOrComposer());
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
        if (useOrigYear) {
            if (origYear!=o.origYear) {
                return origYear<o.origYear ? -1 : 1;
            }
            if (year!=o.year) {
                return year<o.year ? -1 : 1;
            }
        } else {
            if (year!=o.year) {
                return year<o.year ? -1 : 1;
            }
            if (origYear!=o.origYear) {
                return origYear<o.origYear ? -1 : 1;
            }
        }
        compare=title.localeAwareCompare(o.title);
        if (0!=compare) {
            return compare;
        }
        compare=name().compare(o.name());
        if (0!=compare) {
            return compare;
        }
        compare=compareGenres(o);
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

bool Song::sameMetadata(const Song &o) const
{
    return disc == o.disc && track == o.track && time == o.time && year == o.year && origYear == o.origYear &&
           artist == o.artist && albumartist == o.albumartist && album == o.album && title == o.title;
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
        } else if (2==parts.length() && parts.at(0).contains(constAlbumArtistSep)) {
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

            for (const QChar &sep: constSeparators) {
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
        blank |= BlankArtist;
    }
    if (album.isEmpty()) {
        album = unknownStr;
        blank |= BlankAlbum;
    }
    if (title.isEmpty()) {
        title = unknownStr;
        blank |= BlankTitle;
    }
    if (genres[0].isEmpty()) {
        genres[0]=unknownStr;
    }
}

struct KeyStore
{
    KeyStore() : currentKey(0) { }
    quint16 currentKey;
    QHash<QString, quint16> keys;
};

static QHash<int, KeyStore> storeMap;
static QMutex storeMapMutex;

void Song::clearKeyStore(int location)
{
    QMutexLocker locker(&storeMapMutex);
    storeMap.remove(location);
}

QString Song::displayAlbum(const QString &albumName, quint16 albumYear)
{
    return albumYear>0 ? albumName+QLatin1String(" (")+QString::number(albumYear)+QLatin1Char(')') : albumName;
}

static QSet<QString> prefixesToIngore=QSet<QString>() << QLatin1String("The");

QSet<QString> Song::ignorePrefixes()
{
    return prefixesToIngore;
}

void Song::setIgnorePrefixes(const QSet<QString> &prefixes)
{
    prefixesToIngore=prefixes;
}

static QString ignorePrefix(const QString &str)
{
    for (const QString &p: prefixesToIngore) {
        if (str.startsWith(p+QLatin1Char(' '))) {
            return str.mid(p.length()+1)+QLatin1String(", ")+p;
        }
    }
    return QString();
}

QString Song::sortString(const QString &str)
{
    QString sort=ignorePrefix(str);

    if (sort.isEmpty()) {
        sort=str;
    }
    sort=sort.remove('.');
    return sort==str ? QString() : sort;
}

bool Song::useOriginalYear()
{
    return useOrigYear;
}

void Song::setUseOriginalYear(bool u)
{
    useOrigYear = u;
}

quint16 Song::setKey(int location)
{
    if (isStandardStream()) {
        key=0;
        return 0;
    }

    QString songKey(albumKey());
    QMutexLocker locker(&storeMapMutex);
    KeyStore &store=storeMap[location];
    QHash<QString, quint16>::ConstIterator it=store.keys.find(songKey);

    if (it!=store.keys.end()) {
        key=it.value();
    } else {
        store.currentKey++; // Key 0 is for streams, so we need to increment before setting...
        store.keys.insert(songKey, store.currentKey);
        key=store.currentKey;
    }
    return key;
}

bool Song::isUnknownAlbum() const
{
    return (album.isEmpty() || album==unknownStr) && (albumArtist().isEmpty() || albumArtist()==unknownStr);
}

bool Song::isInvalid() const
{
    return 0==time && guessed && !file.contains("://") && (genres[0].isEmpty() || genres[0]==unknownStr) && name().isEmpty();
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
    blank = 0;
    year = 0;
    origYear = 0;
    for (int i=0; i<constNumGenres; ++i) {
        genres[i]=QString();
    }
    size = 0;
    extra.clear();
    type = Standard;
}

const QLatin1Char Song::constFieldSep('\001');
const QString Song::constSep=QString::fromUtf8(" \u2022 ");

void Song::addGenre(const QString &g)
{
    for (int i=0; i<constNumGenres; ++i) {
        if (genres[i].isEmpty()) {
            genres[i]=g;
            break;
        }
    }
}

quint16 Song::displayYear() const
{
    return useOrigYear && origYear>0 ? origYear : year;
}

QString Song::entryName() const
{
    if (title.isEmpty()) {
        return file;
    }

    return title+constFieldSep+artist+constFieldSep+album;
}

QString Song::albumArtistOrComposer() const
{
    if (useComposer()) {
        QString c=composer();
        if (!c.isEmpty()) {
            return c;
        }
    }
    return albumArtist();
}

QString Song::trackArtistOrComposer() const
{
    if (useComposer()) {
        QString c=composer();
        if (!c.isEmpty()) {
            return c;
        }
    }
    return artist;
}

QString Song::albumName() const
{
    if (useComposer()) {
        QString c=composer();
        if (!c.isEmpty() && c!=albumArtist()) {
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
    //return artist+constSep+title;
    return title+constSep+artist;
}

QString Song::trackAndTitleStr(bool showArtistIfDifferent) const
{
    #if !defined CANTATA_NO_UI_FUNCTIONS
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
           (track>0 && SingleTracks!=type ? (track>9 ? QString::number(track)+QLatin1Char(' ') : (QLatin1Char('0')+QString::number(track)+QLatin1Char(' '))) : QString())+
           (showArtistIfDifferent && diffArtist() ? artistSong() : title) +
           (origYear>0 && origYear != year ? QLatin1String(" (")+QString::number(origYear)+QLatin1Char(')') : QString());
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
    addField(QObject::tr("Title"), title, toolTip);
    addField(QObject::tr("Artist"), artist, toolTip);
    if (albumartist!=artist) {
        addField(QObject::tr("Album artist"), albumartist, toolTip);
    }
    addField(QObject::tr("Composer"), composer(), toolTip);
    addField(QObject::tr("Performer"), performer(), toolTip);
    addField(QObject::tr("Album"), albumName(), toolTip);
    if (track>0) {
        addField(QObject::tr("Track number"), QString::number(track), toolTip);
    }
    if (disc>0) {
        addField(QObject::tr("Disc number"), QString::number(disc), toolTip);
    }
    addField(QObject::tr("Genre"), displayGenre(), toolTip);
    if (year>0) {
        addField(QObject::tr("Year"), QString::number(year), toolTip);
    }
    if (origYear>0) {
        addField(QObject::tr("Original Year"), QString::number(origYear), toolTip);
    }
    if (time>0) {
        addField(QObject::tr("Length"), Utils::formatTime(time, true), toolTip);
    }
    toolTip+=QLatin1String("</table>");

    if (isNonMPD()) {
        return toolTip;
    }
    return toolTip+QLatin1String("<br/><br/><small><i>")+filePath()+QLatin1String("</i></small>");
    #endif
}

QString Song::displayGenre() const
{
    QString g=genres[0];
    for (int i=1; i<constNumGenres&& !genres[i].isEmpty(); ++i) {
        g+=QLatin1String(", ")+genres[i];
    }
    return g;
}

int Song::compareGenres(const Song &o) const
{
    for (int i=0; i<constNumGenres; ++i) {
        int compare=genres[i].compare(o.genres[i]);
        if (0!=compare) {
            return compare;
        }
    }
    return 0;
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
    return /*isVariousArtists() || */(!albumartist.isEmpty() && !artist.isEmpty() && albumartist!=artist);
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
        while ( j < word.length() && ('('==word[j] || '['==word[j] || '{'==word[j]) ) {
            j++;
        }
        if (j < word.length()) {
            word[j] = word[j].toUpper();
        }
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
    #if !defined CANTATA_NO_UI_FUNCTIONS
    if ((OnlineSvrTrack==type || Song::CantataStream) && OnlineService::showLogoAsCover(*this)) {
        return onlineService();
    }
    #endif
    return albumArtistOrComposer()+QLatin1Char(':')+albumId(); //+QLatin1Char(':')+QString::number(disc);
}

static QString basic(const QString &str, const QStringList &extraToStrip=QStringList())
{
    QStringList toStrip=QStringList() << QLatin1String("ft. ") << QLatin1String("feat. ") << QLatin1String("featuring ") << QLatin1String("f. ")
                                      << extraToStrip;
    QStringList prefixes=QStringList() << QLatin1String(" ") << QLatin1String(" (") << QLatin1String(" [");

    for (const QString &s: toStrip) {
        for (const QString &p: prefixes) {
            int strip = str.toLower().indexOf(p+s);
            if (-1!=strip) {
                return str.mid(0, strip);
            }
        }
    }
    return str;
}

QString Song::basicArtist(bool orComposer) const
{
    if (orComposer && useComposer()) {
        QString c=composer();
        if (!c.isEmpty()) {
            return c;
        }
    }

    if (!albumartist.isEmpty() && (artist.isEmpty() || albumartist==artist || (albumartist.length()<artist.length() && artist.startsWith(albumartist)))) {
        return albumartist;
    }

    return basic(artist);
}

QString Song::basicTitle() const
{
    return basic(title, QStringList() << QLatin1String("prod. ") << QLatin1String("prod ") << QLatin1String("producer ") << QLatin1String("produced by "));
}

QString Song::filePath(const QString &base) const
{
    if (isCantataStream()) {
        QString p = localPath();
        return p.isEmpty() ? QUrl(file).path() : p;
    }
    if (isLocalFile()) {
        return file;
    }
    QString fileName=decodePath(file, isCdda());
    if (!base.isEmpty()) {
        bool haveAbsPath=fileName.startsWith("/"); // Utils::constDirSep
        if (!haveAbsPath) {
            return QString(base+fileName);
        }
    }
    return fileName;
}

QString Song::describe() const
{
    #if !defined CANTATA_NO_UI_FUNCTIONS
    if (OnlineSvrTrack==type && PodcastService::constName==onlineService()) {
        return title;
    }
    #endif

    QString albumText=album.isEmpty() ? name() : displayAlbum(album, Song::albumYear(*this));

    if (title.isEmpty()) {
        return QLatin1String("<b>")+albumText+QLatin1String("</b>");
    }
    return artist.isEmpty()
                    ? QObject::tr("<b>%1</b> on <b>%2</b>", "Song on Album").arg(title).arg(albumText)
                    : QObject::tr("<b>%1</b> by <b>%2</b> on <b>%3</b>", "Song by Artist on Album").arg(title).arg(artist).arg(albumText);
}

QString Song::mainText() const
{
    if (isEmpty()) {
        return QString();
    }

    QString n = name();
    if (isStream() && !isCantataStream() && !isCdda() && !isDlnaStream()) {
        return n.isEmpty() ? Song::unknown() : n;
    } else if (title.isEmpty() && artist.isEmpty() && (!n.isEmpty() || !file.isEmpty())) {
        return n.isEmpty() ? file : n;
    } else {
        return title+(origYear>0 && !Song::useOriginalYear() && origYear!=year ? QLatin1String(" (")+QString::number(origYear)+QLatin1Char(')') : QString());
    }
}

QString Song::subText() const
{
    if (isEmpty()) {
        return QString();
    }

    if (isStream() && !isCantataStream() && !isCdda() && !isDlnaStream()) {
        if (artist.isEmpty() && title.isEmpty() && !name().isEmpty()) {
            return QObject::tr("(Stream)");
        } else {
            return artist.isEmpty() ? title : artistSong();
        }
    } else if (album.isEmpty() && artist.isEmpty() && (!useComposer() || composer().isEmpty())) {
        return mainText().isEmpty() ? QString() : Song::unknown();
    } else {
        QString comp = useComposer() ? composer() : QString();
        if (album.isEmpty()) {
            return artist.isEmpty() ? comp : comp.isEmpty() ? artist : (comp + constSep + artist);
        } else {
            // Artist here is always artist (or 'composer - artist'), and not album artist
            return (artist.isEmpty() ? comp : comp.isEmpty() ? artist : (comp + constSep + artist)) + constSep+displayAlbum(!comp.isEmpty());
        }
    }
}

bool Song::useComposer() const
{
    if (compGenres.isEmpty()) {
        return false;
    }

    for (int i=0; i<constNumGenres && !genres[i].isEmpty(); ++i) {
        if (compGenres.contains(genres[i])) {
            return true;
        }
    }
    return false;
}

void Song::populateSorts()
{
    if (!hasArtistSort()) {
        QString val=sortString(artist);
        if (val!=artist) {
            setArtistSort(val);
        }
    }
    if (!albumartist.isEmpty() && !hasAlbumArtistSort()) {
        QString val=sortString(albumartist);
        if (val!=albumartist) {
            setAlbumArtistSort(val);
        }
    }
    if (!hasAlbumSort()) {
        QString val=sortString(album);
        if (val!=album) {
            setAlbumSort(val);
        }
    }
}

void Song::setFromSingleTracks()
{
    albumartist=variousArtists();
    album=singleTracks();
    type=SingleTracks;
    setAlbumArtistSort(QString());
    setAlbumSort(QString());
    setMbAlbumId(QString());
}

//QString Song::basicDescription() const
//{
//    return isStandardStream()
//            ? name
//            : title.isEmpty()
//                ? QString()
//                : artist.isEmpty()
//                    ? title
//                    : trc("track – artist", "%1 – %2", title, artist);
//}

QDataStream & operator<<(QDataStream &stream, const Song &song)
{
    stream << song.id << song.file << song.album << song.artist << song.albumartist << song.title
           << song.disc << song.priority << song.time << song.track << (quint16)song.year // << song.origYear
           << (quint16)song.type << (bool)song.guessed << song.size << song.extra << song.extraFields;
    for (int i=0; i<Song::constNumGenres; ++i) {
        stream << song.genres[i];
    }
    return stream;
}

QDataStream & operator>>(QDataStream &stream, Song &song)
{
    quint16 type;
    quint16 year;
    quint8 disc;
    bool guessed;
    stream >> song.id >> song.file >> song.album >> song.artist >> song.albumartist >> song.title
           >> disc >> song.priority >> song.time >> song.track >> year // >> song.origYear
           >> type >> guessed >> song.size >> song.extra >> song.extraFields;
    song.type=(Song::Type)type;
    song.year=year;
    song.guessed=guessed;
    song.disc=disc;
    for (int i=0; i<Song::constNumGenres; ++i) {
        stream >> song.genres[i];
    }

    return stream;
}
