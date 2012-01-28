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

#include "config.h"
#include "device.h"
#include "devicesmodel.h"
#include "umsdevice.h"
#ifdef MTP_FOUND
#include "mtpdevice.h"
#endif
#include "tagreader.h"
#include "song.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "covers.h"
#include <solid/portablemediaplayer.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <KDE/KLocale>
#include <KDE/KGlobal>
#include <KDE/KConfig>
#include <KDE/KConfigGroup>
#include <KDE/KStandardDirs>
#include <sys/types.h>
#include <sys/stat.h>

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

const QLatin1String Device::Options::constAlbumArtist("%albumartist%");
const QLatin1String Device::Options::constAlbumTitle("%album%");
const QLatin1String Device::Options::constTrackArtist("%artist%");
const QLatin1String Device::Options::constTrackTitle("%title%");
const QLatin1String Device::Options::constTrackNumber("%track%");
const QLatin1String Device::Options::constCdNumber("%discnumber%");
const QLatin1String Device::Options::constGenre("%genre%");
const QLatin1String Device::Options::constYear("%year%");
const QLatin1String Device::constNoCover("-");

Device::Options::Options()
    : scheme(constAlbumArtist+QChar('/')+
             constAlbumTitle+QChar('/')+
             constTrackNumber+QChar(' ')+
             constTrackTitle)
    , vfatSafe(true)
    , asciiOnly(false)
    , ignoreThe(false)
    , replaceSpaces(false)
    , fixVariousArtists(false)
    , transcoderValue(0)
{
}

void Device::Options::load(const QString &group)
{
    KConfigGroup grp(KGlobal::config(), group);
    scheme=grp.readEntry("scheme", scheme);
    vfatSafe=grp.readEntry("vfatSafe", vfatSafe);
    asciiOnly=grp.readEntry("asciiOnly", asciiOnly);
    ignoreThe=grp.readEntry("ignoreThe", ignoreThe);
    replaceSpaces=grp.readEntry("replaceSpaces", replaceSpaces);
    if (QLatin1String("mpd")!=group) {
        fixVariousArtists=grp.readEntry("fixVariousArtists", fixVariousArtists);
        transcoderCodec=grp.readEntry("transcoderCodec", transcoderCodec);
        transcoderValue=grp.readEntry("transcoderValue", transcoderValue);
    }
}

void Device::Options::save(const QString &group)
{
    KConfigGroup grp(KGlobal::config(), group);
    grp.writeEntry("scheme", scheme);
    grp.writeEntry("vfatSafe", vfatSafe);
    grp.writeEntry("asciiOnly", asciiOnly);
    grp.writeEntry("ignoreThe", ignoreThe);
    grp.writeEntry("replaceSpaces", replaceSpaces);
    if (QLatin1String("mpd")!=group) {
        grp.writeEntry("fixVariousArtists", fixVariousArtists);
        grp.writeEntry("transcoderCodec", transcoderCodec);
        grp.writeEntry("transcoderValue", transcoderValue);
    }
    grp.sync();
}

QString Device::Options::clean(const QString &str) const
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

Song Device::Options::clean(const Song &s) const
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

QString Device::Options::createFilename(const Song &s) const
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

Device * Device::create(DevicesModel *m, const QString &udi)
{
    Solid::Device device=Solid::Device(udi);

    if (device.is<Solid::PortableMediaPlayer>())
    {
        #ifdef MTP_FOUND
        Solid::PortableMediaPlayer *pmp = device.as<Solid::PortableMediaPlayer>();

        if (pmp->supportedProtocols().contains(QLatin1String("mtp"))) {
            return new MtpDevice(m, device);
        }
        #endif
    } else if (device.is<Solid::StorageAccess>()) {

        //HACK: ignore apple stuff until we have common MediaDeviceFactory.
        if (!device.vendor().contains("apple", Qt::CaseInsensitive)) {
//             Solid::StorageAccess *sa = device.as<Solid::StorageAccess>();
//             if (QLatin1String("usb")==sa->bus) {
                return new UmsDevice(m, device);
//             }
        }
    }
    return 0;
}

QString fixDir(const QString &d)
{
    QString dir=d;
    dir.replace(QLatin1String("//"), QLatin1String("/"));
    if (!dir.endsWith('/')){
        dir+='/';
    }
    return dir;
}

void Device::cleanDir(const QString &dir, const QString &base, const QString &coverFile, int level)
{
    QDir d(dir);
    if (d.exists()) {
        QFileInfoList entries=d.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::Dirs|QDir::NoDotAndDotDot);
        QList<QString> extraFiles;
        QSet<QString> others=Covers::standardNames().toSet();
        others << coverFile << "albumart.pamp";

        foreach (const QFileInfo &info, entries) {
            if (info.isDir()) {
                return;
            }
            if (!others.contains(info.fileName())) {
                return;
            }
            extraFiles.append(info.absoluteFilePath());
        }

        foreach (const QString &cf, extraFiles) {
            if (!QFile::remove(cf)) {
                return;
            }
        }

        if (fixDir(dir)==fixDir(base)) {
            return;
        }
        QString dirName=d.dirName();
        if (dirName.isEmpty()) {
            return;
        }
        d.cdUp();
        if (!d.rmdir(dirName)) {
            return;
        }
        if (level>=3) {
            return;
        }
        QString upDir=d.absolutePath();
        if (fixDir(upDir)!=fixDir(base)) {
            cleanDir(upDir, base, coverFile, level+1);
        }
    }
}

void Device::setFilePerms(const QString &file)
{
    //
    // Clear any umask before setting file perms
    mode_t oldMask(umask(0000));
    ::chmod(QFile::encodeName(file).constData(), 0644);
    // Reset umask
    ::umask(oldMask);
}

bool Device::createDir(const QString &dir)
{
    //
    // Clear any umask before dir is created
    mode_t oldMask(umask(0000));
    bool status(KStandardDirs::makeDir(dir, 0755));
    // Reset umask
    ::umask(oldMask);
    return status;
}

bool Device::fixVariousArtists(const QString &file, Song &song, bool applyFix)
{
//     qWarning() << "FIX:" << file << song.artist << song.albumartist << song.title;
    Song orig=song;
    if (!file.isEmpty() && song.albumartist.isEmpty()) {
        song=TagReader::read(file);
    }

    if (song.artist.isEmpty() || song.albumartist.isEmpty() || !Song::isVariousArtists(song.albumartist)) {
        song=orig;
//         qWarning() << "NOT VA";
        return false;
    }

    bool needsUpdating=false;

    if (!applyFix && song.artist==song.albumartist) { // Then real artist is embedded in track title...
        int sepPos=song.title.indexOf(QLatin1String(" - "));
        if (sepPos>0 && sepPos<song.title.length()-3) {
            song.artist=song.title.left(sepPos);
            song.title=song.title.mid(sepPos+3);
            needsUpdating=true;
//             qWarning() << "UNFIXING" << "A:" << song.artist << "T:" << song.title;
        }
    } else if (applyFix) { // We must be copying to device, and need to place song artist into title...
        QString artist=song.artist;
        artist.replace(" - ", ", ");
        song.title=song.artistSong();
        song.artist=song.albumartist;
        needsUpdating=true;
//         qWarning() << "FIXING";
    }

    if (needsUpdating && (file.isEmpty() || TagReader::updateArtistAndTitleTags(file, song))) {
//         qWarning() << "SAVED:" << file;
        return true;
    }
    song=orig;
    return false;
}

void Device::applyUpdate()
{
    /*if (m_childItems.count() && update && update->childCount()) {
        QSet<Song> currentSongs=allSongs();
        QSet<Song> updateSongs=update->allSongs();
        QSet<Song> removed=currentSongs-updateSongs;
        QSet<Song> added=updateSongs-currentSongs;

        foreach (const Song &s, added) {
            addSongToList(s);
        }
        foreach (const Song &s, removed) {
            removeSongFromList(s);
        }
        delete update;
    } else*/ {
        int oldCount=childCount();
        if (oldCount>0) {
            model->beginRemoveRows(model->createIndex(model->devices.indexOf(this), 0, this), 0, oldCount-1);
            m_childItems.clear();
            qDeleteAll(m_childItems);
            model->endRemoveRows();
        }
        int newCount=newRows();
        if (newCount>0) {
            model->beginInsertRows(model->createIndex(model->devices.indexOf(this), 0, this), 0, newCount-1);
            foreach (MusicLibraryItem *item, update->children()) {
                item->setParent(this);
            }
            delete update;
            refreshIndexes();
            model->endInsertRows();
        }
    }
    update=0;
}

bool Device::songExists(const Song &s) const
{
    MusicLibraryItemArtist *artistItem = ((MusicLibraryItemRoot *)this)->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (const MusicLibraryItem *songItem, albumItem->children()) {
                if (songItem->data()==s.title) {
                    return true;
                }
            }
        }
    }

    return false;
}

void Device::addSongToList(const Song &s)
{
    MusicLibraryItemArtist *artistItem = artist(s, false);
    if (!artistItem) {
        model->beginInsertRows(model->createIndex(model->devices.indexOf(this), 0, this), childCount(), childCount());
        artistItem = createArtist(s);
        model->endInsertRows();
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        model->beginInsertRows(model->createIndex(children().indexOf(artistItem), 0, artistItem), artistItem->childCount(), artistItem->childCount());
        albumItem = artistItem->createAlbum(s);
        model->endInsertRows();
    }
    foreach (const MusicLibraryItem *songItem, albumItem->children()) {
        if (songItem->data()==s.title) {
            return;
        }
    }

    model->beginInsertRows(model->createIndex(artistItem->children().indexOf(albumItem), 0, albumItem), albumItem->childCount(), albumItem->childCount());
    MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
    albumItem->append(songItem);
    model->endInsertRows();
}

void Device::removeSongFromList(const Song &s)
{
    MusicLibraryItemArtist *artistItem = artist(s, false);
    if (!artistItem) {
        return;
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        return;
    }
    MusicLibraryItem *songItem=0;
    int songRow=0;
    foreach (MusicLibraryItem *song, albumItem->children()) {
        if (static_cast<MusicLibraryItemSong *>(song)->song().title==s.title) {
            songItem=song;
            break;
        }
        songRow++;
    }
    if (!songItem) {
        return;
    }

    if (1==artistItem->childCount() && 1==albumItem->childCount()) {
        // 1 album with 1 song - so remove whole artist
        int row=m_childItems.indexOf(artistItem);
        model->beginRemoveRows(model->createIndex(model->devices.indexOf(this), 0, this), row, row);
        remove(artistItem);
        model->endRemoveRows();
        return;
    }

    if (1==albumItem->childCount()) {
        // multiple albums, but this album only has 1 song - remove album
        int row=artistItem->children().indexOf(albumItem);
        model->beginRemoveRows(model->createIndex(children().indexOf(artistItem), 0, artistItem), row, row);
        artistItem->remove(albumItem);
        model->endRemoveRows();
        return;
    }

    // Just remove particular song
    model->beginRemoveRows(model->createIndex(artistItem->children().indexOf(albumItem), 0, albumItem), songRow, songRow);
    albumItem->remove(songRow);
    model->endRemoveRows();
}

void Device::setStatusMessage(const QString &msg)
{
    statusMsg=msg;
    QModelIndex modelIndex=model->createIndex(model->devices.indexOf(this), 0, this);
    emit model->dataChanged(modelIndex, modelIndex);
}
