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

#include "deviceoptions.h"
#include "mpd-interface/song.h"
#include "support/configuration.h"
#include <QDir>
#include <QUrl>
#ifndef _MSC_VER 
#include <unistd.h>
#endif

static QString cleanPath(const QString &path)
{
    /* Unicode uses combining characters to form accented versions of other characters.
        * (Exception: Latin-1 table for compatibility with ASCII.)
        * Those can be found in the Unicode tables listed at:
        * http://en.wikipedia.org/w/index.php?title=Combining_character&oldid=255990982
        * Removing those characters removes accents. :)                                   */
    QString result = path;

    // German umlauts
    result.replace(QChar(0x00e4), "ae").replace(QChar(0x00c4), "Ae");
    result.replace(QChar(0x00f6), "oe").replace(QChar(0x00d6), "Oe");
    result.replace(QChar(0x00fc), "ue").replace(QChar(0x00dc), "Ue");
    result.replace(QChar(0x00df), "ss");

    // other special cases
    result.replace(QChar(0x00C6), "AE");
    result.replace(QChar(0x00E6), "ae");

    result.replace(QChar(0x00D8), "OE");
    result.replace(QChar(0x00F8), "oe");

    // normalize in a form where accents are separate characters
    result = result.normalized(QString::NormalizationForm_D);

    // remove accents from table "Combining Diacritical Marks"
    for (int i = 0x0300; i <= 0x036F; i++)  {
        result.remove(QChar(i));
    }

    return result;
}

static QString asciiPath(const QString &path)
{
    QString result = path;
    for (int i = 0; i < result.length(); i++) {
        QChar c = result[ i ];
        if (c > QChar(0x7f) || QChar(0)==c)  {
            c = '_';
        }
        result[ i ] = c;
    }
    return result;
}

QString vfatPath(const QString &path)
{
    QString s = path;

    if ('/'==QDir::separator()) {// we are on *nix, \ is a valid character in file or directory names, NOT the dir separator
        s.replace('\\', '_');
    } else {
        s.replace('/', '_'); // on windows we have to replace / instead
    }

    for (int i = 0; i < s.length(); i++)  {
        QChar c = s[ i ];
        if (c < QChar(0x20) || c == QChar(0x7F) // 0x7F = 127 = DEL control character
            || '*'==c || '?'==c || '<'==c || '>'==c || '|'==c || '"'==c || ':'==c) {
            c = '_';
        } else if ('['==c) {
            c = '(';
        } else if (']'==c) {
            c = ')';
        }
        s[ i ] = c;
    }

    /* beware of reserved device names */
    uint len = s.length();
    if (3==len || (len > 3 && '.'==s[3])) {
        QString l = s.left(3).toLower();
        if ("aux"==l || "con"==l || "nul"==l || "prn"==l) {
            s = '_' + s;
        }
    } else if (4==len || (len > 4 && '.'==s[4]))  {
        QString l = s.left(3).toLower();
        QString d = s.mid(3,1);
        if (("com"==l || "lpt"==l) &&
                ("0"==d || "1"==d || "2"==d || "3"==d || "4"==d || "5"==d || "6"==d || "7"==d || "8"==d || "9"==d)) {
            s = '_' + s;
        }
    }

    // "clock$" is only allowed WITH extension, according to:
    // http://en.wikipedia.org/w/index.php?title=Filename&oldid=303934888#Comparison_of_file_name_limitations
    if (0==QString::compare(s, "clock$", Qt::CaseInsensitive)) {
        s = '_' + s;
    }

    /* max path length of Windows API */
    s = s.left(255);

    /* whitespace at the end of folder/file names or extensions are bad */
    len = s.length();
    if (' '==s[len-1]) {
        s[len-1] = '_';
    }

    int extensionIndex = s.lastIndexOf('.'); // correct trailing spaces in file name itself
    if (s.length()>1 && extensionIndex>0 && ' '==s.at(extensionIndex-1)) {
        s[extensionIndex - 1] = '_';
    }

    for (int i = 1; i < s.length(); i++) {// correct trailing whitespace in folder names
        if ((s.at(i) == QDir::separator()) && ' '==s.at(i-1)) {
            s[i - 1] = '_';
        }
    }

    return s;
}

static void manipulateThe(QString &str, bool reverse)
{
    if (reverse)
    {
        if (!str.startsWith("the ", Qt::CaseInsensitive)) {
            return;
        }

        QString begin = str.left(3);
        str = str.append(", %1").arg(begin);
        str = str.mid(4);
        return;
    }

    if(!str.endsWith(", the", Qt::CaseInsensitive)) {
        return;
    }

    QString end = str.right(3);
    str = str.prepend("%1 ").arg(end);
    str.truncate(str.length() - end.length() - 2);
}

const QLatin1String DeviceOptions::constAlbumArtist("%albumartist%");
const QLatin1String DeviceOptions::constComposer("%composer%");
const QLatin1String DeviceOptions::constAlbumTitle("%album%");
const QLatin1String DeviceOptions::constTrackArtist("%artist%");
const QLatin1String DeviceOptions::constTrackTitle("%title%");
const QLatin1String DeviceOptions::constTrackArtistAndTitle("%artistandtitle%");
const QLatin1String DeviceOptions::constTrackNumber("%track%");
const QLatin1String DeviceOptions::constCdNumber("%discnumber%");
const QLatin1String DeviceOptions::constGenre("%genre%");
const QLatin1String DeviceOptions::constYear("%year%");

#ifdef ENABLE_DEVICES_SUPPORT
DeviceOptions::DeviceOptions(const QString &cvrName)
#else
DeviceOptions::DeviceOptions()
#endif
    : scheme(constAlbumArtist+QChar('/')+
             constAlbumTitle+QChar('/')+
             constTrackNumber+QChar(' ')+
             constTrackTitle)
    , vfatSafe(true)
    , asciiOnly(false)
    , ignoreThe(false)
    , replaceSpaces(false)
    #ifdef ENABLE_DEVICES_SUPPORT
    , fixVariousArtists(false)
    , transcoderValue(0)
    , transcoderWhen(TW_Always)
    , useCache(true)
    , autoScan(false)
    , coverName(cvrName)
    , coverMaxSize(0)
    #endif
{
}

static const QLatin1String constMpdGroup("mpd");
static const QLatin1String constSchemeKey("scheme");
static const QLatin1String constVFatKey("vfatSafe");
static const QLatin1String constAsciiKey("asciiOnly");
static const QLatin1String constIgnoreTheKey("ignoreThe");
static const QLatin1String constSpacesKey("replaceSpaces");
#ifdef ENABLE_DEVICES_SUPPORT
static const QLatin1String constTransCodecKey("transcoderCodec");
static const QLatin1String constTransValKey("transcoderValue");
static const QLatin1String constTransIfDiffKey("transcoderWhenDifferent");
static const QLatin1String constTransIfLosslessKey("transcoderWhenSourceIsLosssless");
static const QLatin1String constTransWhenKey("transcoderWhen");
#endif
static const QLatin1String constUseCacheKey("useCache");
static const QLatin1String constFixVaKey("fixVariousArtists");
static const QLatin1String constNameKey("name");
static const QLatin1String constCvrFileKey("coverFileName");
static const QLatin1String constCvrMaxKey("coverMaxSize");
static const QLatin1String constVolKey("volumeId");

bool DeviceOptions::isConfigured(const QString &group, bool isMpd)
{
    if (Configuration(group).hasEntry(constSchemeKey)) {
        return true;
    }
    if (isMpd) {
        return Configuration(constMpdGroup).hasEntry(constSchemeKey);
    }
    return false;
}

void DeviceOptions::load(const QString &group, bool isMpd)
{
    Configuration cfg(group);

    if (isMpd && !cfg.hasEntry(constSchemeKey)) {
        // Try old [mpd] group...
        Configuration mpdGrp(constMpdGroup);
        if (mpdGrp.hasEntry(constSchemeKey)) {
            scheme=mpdGrp.get(constSchemeKey, scheme);
            vfatSafe=mpdGrp.get(constVFatKey, vfatSafe);
            asciiOnly=mpdGrp.get(constAsciiKey, asciiOnly);
            ignoreThe=mpdGrp.get(constIgnoreTheKey, ignoreThe);
            replaceSpaces=mpdGrp.get(constSpacesKey, replaceSpaces);
            #ifdef ENABLE_DEVICES_SUPPORT
            transcoderCodec=mpdGrp.get(constTransCodecKey, (isMpd ? "lame" : transcoderCodec));
            transcoderValue=mpdGrp.get(constTransValKey, (isMpd ? 2 : transcoderValue));
            if (mpdGrp.hasEntry(constTransWhenKey)) {
                transcoderWhen=(TranscodeWhen)mpdGrp.get(constTransWhenKey, (int)transcoderWhen);
            } else {
                bool transcoderWhenDifferent=mpdGrp.get(constTransIfDiffKey, false);
                bool transcoderWhenSourceIsLosssless=mpdGrp.get(constTransIfLosslessKey, false);
                if (mpdGrp.get(constTransIfLosslessKey, false)) {
                    transcoderWhen=TW_IfLossess;
                } else if (mpdGrp.get(constTransIfDiffKey, false)) {
                    transcoderWhen=TW_IfDifferent;
                } else {
                    transcoderWhen=TW_Always;
                }
            }
            #endif
            // Save these settings into new group, [mpd] is left for other connections...
            save(group, true, true, true);
        }
    } else {
        scheme=cfg.get(constSchemeKey, scheme);
        vfatSafe=cfg.get(constVFatKey, vfatSafe);
        asciiOnly=cfg.get(constAsciiKey, asciiOnly);
        ignoreThe=cfg.get(constIgnoreTheKey, ignoreThe);
        replaceSpaces=cfg.get(constSpacesKey, replaceSpaces);
        #ifdef ENABLE_DEVICES_SUPPORT
        if (!isMpd) {
            useCache=cfg.get(constUseCacheKey, useCache);
            fixVariousArtists=cfg.get(constFixVaKey, fixVariousArtists);
            name=cfg.get(constNameKey, name);
            coverName=cfg.get(constCvrFileKey, coverName);
            coverMaxSize=cfg.get(constCvrMaxKey, coverMaxSize);
            volumeId=cfg.get(constVolKey, volumeId);
            checkCoverSize();
        }
        transcoderCodec=cfg.get(constTransCodecKey, (isMpd ? "lame" : transcoderCodec));
        transcoderValue=cfg.get(constTransValKey, (isMpd ? 2 : transcoderValue));
        if (cfg.hasEntry(constTransWhenKey)) {
            transcoderWhen=(TranscodeWhen)cfg.get(constTransWhenKey, (int)transcoderWhen);
        } else {
            if (cfg.get(constTransIfLosslessKey, false)) {
                transcoderWhen=TW_IfLossess;
            } else if (cfg.get(constTransIfDiffKey, false)) {
                transcoderWhen=TW_IfDifferent;
            } else {
                transcoderWhen=TW_Always;
            }
        }
        #endif
    }
}

void DeviceOptions::save(const QString &group, bool isMpd, bool saveTrans, bool saveFileName) const
{
    Configuration cfg(group);

    if (saveFileName) {
        cfg.set(constSchemeKey, scheme);
        cfg.set(constVFatKey, vfatSafe);
        cfg.set(constAsciiKey, asciiOnly);
        cfg.set(constIgnoreTheKey, ignoreThe);
        cfg.set(constSpacesKey, replaceSpaces);
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    if (!isMpd) {
        cfg.set(constUseCacheKey, useCache);
        cfg.set(constFixVaKey, fixVariousArtists);
        cfg.set(constNameKey, name);
        cfg.set(constCvrFileKey, coverName);
        cfg.set(constCvrMaxKey, coverMaxSize);
        cfg.set(constVolKey, volumeId);
    }
    if (saveTrans) {
        cfg.set(constTransCodecKey, transcoderCodec);
        cfg.set(constTransValKey, transcoderValue);
        cfg.set(constTransWhenKey, (int)transcoderWhen);
    }
    #else
    Q_UNUSED(isMpd)
    Q_UNUSED(saveTrans)
    #endif
}

QString DeviceOptions::clean(const QString &str) const
{
    QString result(str);

    if (asciiOnly) {
        result = cleanPath(result);
        result = asciiPath(result);
    }

    result=result.simplified();
    if (replaceSpaces) {
        result.replace(QRegExp("\\s"), "_");
    }
    if (vfatSafe) {
        result = vfatPath(result);
    }

    result.replace('/', '-');

    return result;
}

Song DeviceOptions::clean(const Song &s) const
{
    Song copy=s;
    if (ignoreThe) {
        manipulateThe(copy.albumartist, true);
        manipulateThe(copy.artist, true);
    }
    copy.albumartist=clean(copy.albumartist);
    copy.artist=clean(copy.artist);
    copy.album=clean(copy.album);
    copy.title=clean(copy.title);
    for (int i=0; i<Song::constNumGenres && !s.genres[i].isEmpty(); ++i) {
        copy.addGenre(clean(s.genres[i]));
    }

    return copy;
}

QString DeviceOptions::createFilename(const Song &s) const
{
    QString path=scheme;
    static QStringList ignore;

    if (ignore.isEmpty()) {
        ignore << QLatin1String("%comment%")
               << QLatin1String("%filetype%")
               << QLatin1String("%ignore%")
               << QLatin1String("%folder%")
               << QLatin1String("%initial%");
    }

    for (const QString &i: ignore) {
        path.replace(i, QLatin1String(""));
    }
    if (replaceSpaces) {
        path.replace(QRegExp("\\s"), "_");
    }

    Song copy=clean(s);

    path.replace(constAlbumArtist, copy.albumArtist());
    path.replace(constComposer, copy.composer());
    path.replace(constAlbumTitle, copy.album);
    path.replace(constTrackArtist, copy.artist);
    path.replace(constTrackTitle, copy.title);
    path.replace(constTrackArtistAndTitle, clean(copy.displayTitle()));
    QString track(QString::number(copy.track));
    if (copy.track<10) {
        track=QChar('0')+track;
    }
    path.replace(constTrackNumber, track);
    path.replace(constCdNumber, copy.disc<1 ? QLatin1String("") : QString::number(copy.disc));
    path.replace(constGenre, copy.genres[0].isEmpty() ? Song::unknown() : copy.genres[0]);
    path.replace(constYear, copy.year<1 ? QLatin1String("") : QString::number(copy.year));

    // For songs about to be downloaded from streams, we hide the filetype in genre...
    if (s.file.startsWith("http:/")) {
        if (s.genres[0]==QLatin1String("mp3") || s.genres[0]==QLatin1String("ogg") || s.genres[0]==QLatin1String("flac") || s.genres[0]==QLatin1String("wav")) {
            path+='.'+s.genres[0];
        } else if (s.genres[0]==QLatin1String("vbr")) {
            path+=QLatin1String(".mp3");
        } else {
            QString f=QUrl(s.file).path();
            int extPos=f.lastIndexOf('.');
            if (-1!=extPos) {
                path+=f.mid(extPos);
            }
        }
    } else {
        int extPos=s.file.lastIndexOf('.');
        if (-1!=extPos) {
            path+=s.file.mid(extPos);
        }
    }
    return path;
}
