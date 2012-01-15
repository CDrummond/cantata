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

#include "device.h"
#include "devicesmodel.h"
#include "umsdevice.h"
#include "song.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include <solid/portablemediaplayer.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <KDE/KLocale>

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

const QLatin1String Device::NameOptions::constAlbumArtist("%albumartist%");
const QLatin1String Device::NameOptions::constAlbumTitle("%album%");
const QLatin1String Device::NameOptions::constTrackArtist("%artist%");
const QLatin1String Device::NameOptions::constTrackTitle("%title%");
const QLatin1String Device::NameOptions::constTrackNumber("%track%");
const QLatin1String Device::NameOptions::constCdNumber("%discnumber%");
const QLatin1String Device::NameOptions::constGenre("%genre%");
const QLatin1String Device::NameOptions::constYear("%year%");

Device::NameOptions::NameOptions()
    : scheme(constAlbumArtist+QChar('/')+
             constAlbumTitle+QChar('/')+
             constTrackNumber+QChar(' ')+
             constTrackTitle)
    , vfatSafe(true)
    , asciiOnly(false)
    , ignoreThe(false)
    , replaceSpaces(false)
{
}

QString Device::NameOptions::clean(const QString &str) const
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

Song Device::NameOptions::clean(const Song &s) const
{
    Song copy=s;
    copy.albumartist=clean(copy.albumartist);
    copy.artist=clean(copy.artist);
    copy.album=clean(copy.album);
    copy.title=clean(copy.title);
    copy.genre=clean(copy.genre);
    return copy;
}

QString Device::NameOptions::createFilename(const Song &s) const
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
        // TODO!!!
//         Solid::PortableMediaPlayer *pmp = device.as<Solid::PortableMediaPlayer>();
//
//         if (pmp->supportedProtocols().contains(QLatin1String(""mtp"))) {
//             return new MtpDevice(m, udi, name);
//         }
    } else if (device.is<Solid::StorageAccess>()) {

        // TODO: Check bus type - only want USB devices!
        if (device.is<Solid::StorageDrive>()) {
            qWarning() << "DRIVE";
        }
        //HACK: ignore apple stuff until we have common MediaDeviceFactory.
        if (!device.vendor().contains("apple", Qt::CaseInsensitive)) {
//             Solid::StorageAccess *sa = device.as<Solid::StorageAccess>();
//             if (QLatin1String("usb")==sa->bus) {
    qWarning() << "UMS" << udi;
                return new UmsDevice(m, device);
//             }
        }
    }
    return 0;
}

void Device::applyUpdate()
{
    if (m_childItems.count() && update && update->childCount()) {
        qWarning() << "INCREMENTAL UPDATE";
        QSet<Song> currentSongs=allSongs();
        QSet<Song> updateSongs=update->allSongs();
        QSet<Song> removed=currentSongs-updateSongs;
        QSet<Song> added=updateSongs-currentSongs;

        qWarning() << "ADDED:" << added.count() << "REMOVED:" << removed.count();
        foreach (const Song &s, added) {
            addSongToList(s);
        }
        foreach (const Song &s, removed) {
            removeSongFromList(s);
        }
        delete update;
    } else {
        qWarning() << "FULL UPDATE";
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
        QModelIndex idx=model->index(model->devices.indexOf(this), 0, QModelIndex());
        model->beginInsertRows(idx, childCount(), childCount());
        artistItem = createArtist(s);
        model->endInsertRows();
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        QModelIndex idx=model->index(children().indexOf(artistItem), 0, model->index(model->devices.indexOf(this), 0, QModelIndex()));
        model->beginInsertRows(idx, artistItem->childCount(), artistItem->childCount());
        albumItem = artistItem->createAlbum(s);
        model->endInsertRows();
    }
    foreach (const MusicLibraryItem *songItem, albumItem->children()) {
        if (songItem->data()==s.title) {
            return;
        }
    }

    QModelIndex idx=model->index(artistItem->children().indexOf(albumItem), 0,
                        model->index(children().indexOf(artistItem), 0,
                            model->index(model->devices.indexOf(this), 0, QModelIndex())));
    model->beginInsertRows(idx, albumItem->childCount(), albumItem->childCount());
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
        if (songItem->data()==s.title) {
            songItem=song;
            break;
        }
        songRow++;
    }
    if (!songItem) {
        return;
    }

    if (1==albumItem->childCount()) {
        if (1==artistItem->childCount()) {
            // Remove artist
            QModelIndex idx=model->index(model->devices.indexOf(this), 0, QModelIndex());
            int row=m_childItems.indexOf(artistItem);
            model->beginRemoveRows(idx, row, row);
            remove(artistItem);
            model->endRemoveRows();
        } else {
            // Remove album from artist
            QModelIndex idx=model->index(children().indexOf(artistItem), 0,
                                model->index(model->devices.indexOf(this), 0, QModelIndex()));
            int row=artistItem->children().indexOf(albumItem);
            model->beginRemoveRows(idx, row, row);
            artistItem->remove(albumItem);
            model->endRemoveRows();
        }
    } else {
        // Remove song frmo album
        QModelIndex idx=model->index(artistItem->children().indexOf(albumItem), 0,
                            model->index(children().indexOf(artistItem), 0,
                                model->index(model->devices.indexOf(this), 0, QModelIndex())));
        model->beginRemoveRows(idx, songRow, songRow);
        albumItem->remove(songRow);
        model->endRemoveRows();
    }
}
