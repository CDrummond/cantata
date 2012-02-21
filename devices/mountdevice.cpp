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

#include "mountdevice.h"
#include "mpdparseutils.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <KDE/KUrl>
#include <KDE/KDiskFreeSpaceInfo>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KConfigGroup>
#include <kde_file.h>
#include <stdio.h>
#include <mntent.h>

static const QLatin1String constCantataCacheFile("/.cache.xml");

MountDevice::MountDevice(DevicesModel *m, const QString &mp, const QString &dp)
    : FsDevice(m)
    , lastCheck(0)
    , isMounted(false)
    , mountPoint(mp)
    , devPath(dp)
{
    setup();
}

MountDevice::~MountDevice() {
    stopScanner();
}

bool MountDevice::isConnected() const
{
    KDE_struct_stat info;
    bool statOk=0==KDE_lstat("/etc/mtab", &info);
    if (!statOk || info.st_mtime>lastCheck) {
        if (statOk) {
            lastCheck=info.st_mtime;
        }

        FILE *mtab = setmntent ("/etc/mtab", "r");
        struct mntent *part = 0;
        isMounted = false;

        if (mtab) {
            while ((part=getmntent(mtab)) && !isMounted) {
                if ((part->mnt_fsname) && part->mnt_fsname==devPath) {
                    isMounted = true;
                }
            }
            endmntent(mtab);
        }
    }

    return isMounted;
}

double MountDevice::usedCapacity()
{
    if (!isConnected()) {
        return -1.0;
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(mountPoint);
    return inf.size()>0 ? (inf.used()*1.0)/(inf.size()*1.0) : -1.0;
}

QString MountDevice::capacityString()
{
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(mountPoint);
    return i18n("%1 free", KGlobal::locale()->formatByteSize(inf.size()-inf.used()), 1);
}

qint64 MountDevice::freeSpace()
{
    if (!isConnected()) {
        return 0;
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(mountPoint);
    return inf.size()-inf.used();
}

void MountDevice::setup()
{
    QString cfgKey=key();
    opts.load(cfgKey);
    KConfigGroup grp(KGlobal::config(), cfgKey);
    opts.useCache=grp.readEntry("useCache", true);
    coverFileName=grp.readEntry("coverFileName", "cover.jpg");
    audioFolder=grp.readEntry("audioFolder", "Music/");
    KUrl url = KUrl(mountPoint);
    url.addPath(audioFolder);
    url.cleanPath();
    audioFolder=url.toLocalFile();
    if (!audioFolder.endsWith('/')) {
        audioFolder+='/';
    }
    configured=KGlobal::config()->hasGroup(cfgKey);

    if (opts.useCache) {
        MusicLibraryItemRoot *root=new MusicLibraryItemRoot;
        if (root->fromXML(cacheFileName(), audioFolder)) {
            update=root;
            QTimer::singleShot(0, this, SLOT(cacheRead()));
            return;
        }
        delete root;
    }
    rescan();
}

static inline QString toString(bool b)
{
    return b ? QLatin1String("true") : QLatin1String("false");
}

void MountDevice::saveProperties()
{
    saveProperties(audioFolder, coverFileName, opts);
}

void MountDevice::saveProperties(const QString &newPath, const QString &newCoverFileName, const Device::Options &newOpts)
{
    QString nPath=MPDParseUtils::fixPath(newPath);
    if (configured && opts==newOpts && nPath==audioFolder && newCoverFileName==coverFileName) {
        return;
    }

    configured=true;
    bool diffCacheSettings=opts.useCache!=newOpts.useCache;
    QString oldPath=audioFolder;
    opts=newOpts;
    if (diffCacheSettings) {
        if (opts.useCache) {
            saveCache();
        } else {
            removeCache();
        }
    }
    coverFileName=newCoverFileName;

    QString cfgKey=key();
    opts.save(cfgKey);
    KConfigGroup grp(KGlobal::config(), cfgKey);
    grp.writeEntry("useCache", opts.useCache);
    grp.writeEntry("coverFileName", coverFileName);
    QString fixedPath(audioFolder);
    if (fixedPath.startsWith(mountPoint)) {
        fixedPath=QLatin1String("./")+fixedPath.mid(mountPoint.length());
    }
    grp.writeEntry("audioFolder", fixedPath);

    if (oldPath!=audioFolder) {
        rescan();
    }
}
