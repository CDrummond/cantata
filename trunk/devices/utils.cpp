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

#include "utils.h"
#include "config.h"
#include "mpdparseutils.h"
#include "covers.h"
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QSet>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>

void Utils::moveDir(const QString &from, const QString &to, const QString &base, const QString &coverFile)
{
    QDir d(from);
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
            extraFiles.append(info.fileName());
        }

        foreach (const QString &cf, extraFiles) {
            if (!QFile::rename(from+'/'+cf, to+'/'+cf)) {
                return;
            }
        }
        cleanDir(from, base, coverFile);
    }
}

void Utils::cleanDir(const QString &dir, const QString &base, const QString &coverFile, int level)
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

        if (MPDParseUtils::fixPath(dir)==MPDParseUtils::fixPath(base)) {
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
        if (MPDParseUtils::fixPath(upDir)!=MPDParseUtils::fixPath(base)) {
            cleanDir(upDir, base, coverFile, level+1);
        }
    }
}

// Check if $USER is in group "audio", if so we can set files to be owned by this group :-)
static gid_t getAudioGroupId()
{
    static bool init=false;
    static gid_t gid=0;

    if (init) {
        return gid;
    }

    init=true;

    struct passwd *pw=getpwuid(geteuid());

    if (!pw) {
        return gid;
    }

    struct group *audioGroup=getgrnam("audio");

    if (audioGroup) {
        for (int i=0; audioGroup->gr_mem[i]; ++i) {
            if (0==strcmp(audioGroup->gr_mem[i], pw->pw_name)) {
                gid=audioGroup->gr_gid;
                return gid;
            }
        }
    }
    return gid;
}

/*
 * Set file permissions.
 * If user is a memeber of "audio" group, then set file as owned by and writeable by "audio" group.
 */
void Utils::setFilePerms(const QString &file)
{
    //
    // Clear any umask before setting file perms
    mode_t oldMask(umask(0000));
    gid_t gid=getAudioGroupId();
    QByteArray fn=QFile::encodeName(file);
    ::chmod(fn.constData(), 0==gid ? 0644 : 0664);
    if (0!=gid) {
        int rv=::chown(fn.constData(), geteuid(), gid);
        Q_UNUSED(rv);
    }
    // Reset umask
    ::umask(oldMask);
}

/*
 * Create directory, and set its permissions.
 * If user is a memeber of "audio" group, then set dir as owned by and writeable by "audio" group.
 */
bool Utils::createDir(const QString &dir, const QString &base)
{
    //
    // Clear any umask before dir is created
    mode_t oldMask(umask(0000));
    gid_t gid=base.isEmpty() ? 0 : getAudioGroupId();
    #ifdef ENABLE_KDE_SUPPORT
    bool status(KStandardDirs::makeDir(dir, 0==gid ? 0755 : 0775));
    #else
    bool status(QDir(dir).mkpath(dir));
    #endif
    if (status && 0!=gid && dir.startsWith(base)) {
        QStringList parts=dir.mid(base.length()).split('/');
        QString d(base);

        foreach (const QString &p, parts) {
            d+='/'+p;
            int rv=::chown(QFile::encodeName(d).constData(), geteuid(), gid);
            Q_UNUSED(rv);
        }
    }
    // Reset umask
    ::umask(oldMask);
    return status;
}

