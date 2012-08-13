/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "song.h"
#include "localize.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#include <KDE/KConfig>
#include <KDE/KConfigGroup>
#include <KDE/KStandardDirs>
#else
#include <QtCore/QSettings>
#endif
#include <QtCore/QDir>
#include <unistd.h>

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
        if (c > QChar(0x7f) || c == QChar(0))  {
            c = '_';
        }
        result[ i ] = c;
    }
    return result;
}

QString vfatPath(const QString &path)
{
    QString s = path;

    if (QDir::separator() == '/') {// we are on *nix, \ is a valid character in file or directory names, NOT the dir separator
        s.replace('\\', '_');
    } else {
        s.replace('/', '_'); // on windows we have to replace / instead
    }

    for (int i = 0; i < s.length(); i++)  {
        QChar c = s[ i ];
        if (c < QChar(0x20) || c == QChar(0x7F) // 0x7F = 127 = DEL control character
            || c=='*' || c=='?' || c=='<' || c=='>'
            || c=='|' || c=='"' || c==':') {
            c = '_';
        } else if(c == '[') {
            c = '(';
        } else if (c == ']') {
            c = ')';
        }
        s[ i ] = c;
    }

    /* beware of reserved device names */
    uint len = s.length();
    if (len == 3 || (len > 3 && s[3] == '.')) {
        QString l = s.left(3).toLower();
        if(l=="aux" || l=="con" || l=="nul" || l=="prn")
            s = '_' + s;
    } else if (len == 4 || (len > 4 && s[4] == '.'))  {
        QString l = s.left(3).toLower();
        QString d = s.mid(3,1);
        if ((l=="com" || l=="lpt") &&
                (d=="0" || d=="1" || d=="2" || d=="3" || d=="4" ||
                    d=="5" || d=="6" || d=="7" || d=="8" || d=="9")) {
            s = '_' + s;
        }
    }

    // "clock$" is only allowed WITH extension, according to:
    // http://en.wikipedia.org/w/index.php?title=Filename&oldid=303934888#Comparison_of_file_name_limitations
    if (QString::compare(s, "clock$", Qt::CaseInsensitive) == 0) {
        s = '_' + s;
    }

    /* max path length of Windows API */
    s = s.left(255);

    /* whitespace at the end of folder/file names or extensions are bad */
    len = s.length();
    if (s[len-1] == ' ') {
        s[len-1] = '_';
    }

    int extensionIndex = s.lastIndexOf('.'); // correct trailing spaces in file name itself
    if ((s.length() > 1) &&  (extensionIndex > 0)) {
        if(s.at(extensionIndex - 1) == ' ') {
            s[extensionIndex - 1] = '_';
        }
    }

    for (int i = 1; i < s.length(); i++) {// correct trailing whitespace in folder names
        if ((s.at(i) == QDir::separator()) && (s.at(i - 1) == ' ')) {
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
const QLatin1String DeviceOptions::constAlbumTitle("%album%");
const QLatin1String DeviceOptions::constTrackArtist("%artist%");
const QLatin1String DeviceOptions::constTrackTitle("%title%");
const QLatin1String DeviceOptions::constTrackNumber("%track%");
const QLatin1String DeviceOptions::constCdNumber("%discnumber%");
const QLatin1String DeviceOptions::constGenre("%genre%");
const QLatin1String DeviceOptions::constYear("%year%");

DeviceOptions::DeviceOptions()
    : scheme(constAlbumArtist+QChar('/')+
             constAlbumTitle+QChar('/')+
             constTrackNumber+QChar(' ')+
             constTrackTitle)
    , vfatSafe(true)
    , asciiOnly(false)
    , ignoreThe(false)
    , replaceSpaces(false)
    #ifdef ENABLE_KDE_SUPPORT
    , fixVariousArtists(false)
    , transcoderValue(0)
    , transcoderWhenDifferent(false)
    , useCache(false)
    , autoScan(false)
    #endif
{
}

static const QLatin1String constMpdGroup("mpd");

bool DeviceOptions::isConfigured(const QString &group, bool isMpd)
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup grp(KGlobal::config(), !isMpd || group.isEmpty() || KGlobal::config()->hasGroup(group) ? group : constMpdGroup);
    return grp.hasKey("scheme");
    #else
    QSettings cfg;
    QString sep=group.isEmpty() ? QString() : ((-1==cfg.childGroups().indexOf(group) ? alt : group)+"/");
    return cfg.contains(sep+"scheme");
    #endif
}

void DeviceOptions::load(const QString &group, bool isMpd)
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup grp(KGlobal::config(), !isMpd || group.isEmpty() || KGlobal::config()->hasGroup(group) ? group : constMpdGroup);
    scheme=grp.readEntry("scheme", scheme);
    vfatSafe=grp.readEntry("vfatSafe", vfatSafe);
    asciiOnly=grp.readEntry("asciiOnly", asciiOnly);
    ignoreThe=grp.readEntry("ignoreThe", ignoreThe);
    replaceSpaces=grp.readEntry("replaceSpaces", replaceSpaces);
    if (!isMpd) {
        fixVariousArtists=grp.readEntry("fixVariousArtists", fixVariousArtists);
        transcoderCodec=grp.readEntry("transcoderCodec", transcoderCodec);
        transcoderValue=grp.readEntry("transcoderValue", transcoderValue);
        transcoderWhenDifferent=grp.readEntry("transcoderWhenDifferent", transcoderWhenDifferent);
    }
    #else
    #define GET_STRING(KEY, DEF)     (cfg.contains(KEY) ? cfg.value(KEY).toString() : QString(DEF))
    #define GET_BOOL(KEY, DEF)       (cfg.contains(KEY) ? cfg.value(KEY).toBool() : DEF)

    QSettings cfg;
    QString sep=group.isEmpty() ? QString() : ((-1==cfg.childGroups().indexOf(group) ? alt : group)+"/");
    scheme=GET_STRING(sep+"scheme", scheme);
    vfatSafe=GET_BOOL(sep+"vfatSafe", vfatSafe);
    asciiOnly=GET_BOOL(sep+"asciiOnly", asciiOnly);
    ignoreThe=GET_BOOL(sep+"ignoreThe", ignoreThe);
    replaceSpaces=GET_BOOL(sep+"replaceSpaces", replaceSpaces);
    #endif
}

void DeviceOptions::save(const QString &group, bool isMpd)
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup grp(KGlobal::config(), group);
    grp.writeEntry("scheme", scheme);
    grp.writeEntry("vfatSafe", vfatSafe);
    grp.writeEntry("asciiOnly", asciiOnly);
    grp.writeEntry("ignoreThe", ignoreThe);
    grp.writeEntry("replaceSpaces", replaceSpaces);
    if (!isMpd) {
        grp.writeEntry("fixVariousArtists", fixVariousArtists);
        grp.writeEntry("transcoderCodec", transcoderCodec);
        grp.writeEntry("transcoderValue", transcoderValue);
        grp.writeEntry("transcoderWhenDifferent", transcoderWhenDifferent);
    }
    grp.sync();

    if (isMpd && KGlobal::config()->hasGroup(constMpdGroup)) {
        KGlobal::config()->deleteGroup(constMpdGroup);
    }
    #else
    QSettings cfg;
    QString sep=group.isEmpty() ? QString() : (group+"/");
    cfg.setValue(sep+"scheme", scheme);
    cfg.setValue(sep+"vfatSafe", vfatSafe);
    cfg.setValue(sep+"asciiOnly", asciiOnly);
    cfg.setValue(sep+"ignoreThe", ignoreThe);
    cfg.setValue(sep+"replaceSpaces", replaceSpaces);

    if (isMpd && -1!=cfg.childGroups().indexOf(constMpdGroup)) {
        cfg.remove(constMpdGroup);
    }
    #endif
}

QString DeviceOptions::clean(const QString &str) const
{
    QString result(str);

    if (asciiOnly) {
        result = cleanPath(result);
        result = asciiPath(result);
    }

    result.simplified();
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
    copy.genre=clean(copy.genre);
    return copy;
}

QString DeviceOptions::createFilename(const Song &s) const
{
    QString path=scheme;
    static QStringList ignore;

    if (ignore.isEmpty()) {
        ignore << QLatin1String("%composer%")
               << QLatin1String("%comment%")
               << QLatin1String("%filetype%")
               << QLatin1String("%ignore%")
               << QLatin1String("%folder%")
               << QLatin1String("%initial%");
    }

    foreach (const QString &i, ignore) {
        path.replace(i, QLatin1String(""));
    }

    Song copy=clean(s);

    path.replace(constAlbumArtist, copy.albumArtist());
    path.replace(constAlbumTitle, copy.album);
    path.replace(constTrackArtist, copy.artist);
    path.replace(constTrackTitle, copy.title);
    QString track(QString::number(copy.track));
    if (copy.track<10) {
        track=QChar('0')+track;
    }
    path.replace(constTrackNumber, track);
    path.replace(constCdNumber, copy.disc<1 ? QLatin1String("") : QString::number(copy.disc));
    path.replace(constGenre, copy.genre.isEmpty() ? i18n("Unknown") : copy.genre);
    path.replace(constYear, copy.year<1 ? QLatin1String("") : QString::number(copy.year));
    int extPos=s.file.lastIndexOf('.');
    if (-1!=extPos) {
        path+=s.file.mid(extPos);
    }
    return path;
}
