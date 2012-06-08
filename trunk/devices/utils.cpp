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
#include <QtCore/QThread>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef Q_WS_WIN
#include <grp.h>
#include <pwd.h>
#endif

QString Utils::strippedText(QString s)
{
    s.remove(QString::fromLatin1("..."));
    int i = 0;
    while (i < s.size()) {
        ++i;
        if (s.at(i - 1) != QLatin1Char('&')) {
            continue;
        }

        if (i < s.size() && s.at(i) == QLatin1Char('&')) {
            ++i;
        }
        s.remove(i - 1, 1);
    }
    return s.trimmed();
}

QString Utils::dirSyntax(const QString &d)
{
    if(!d.isEmpty())
    {
        QString ds(d);

        ds.replace("//", "/");

        int slashPos(ds.lastIndexOf('/'));

        if(slashPos!=(((int)ds.length())-1))
            ds.append('/');

        return ds;
    }

    return d;
}

QString Utils::getDir(const QString &file)
{
    QString d(file);
    int slashPos(d.lastIndexOf('/'));

    if(slashPos!=-1)
        d.remove(slashPos+1, d.length());

    return dirSyntax(d);
}

QString Utils::changeExtension(const QString &file, const QString &extension)
{
    if (extension.isEmpty()) {
        return file;
    }

    QString f(file);
    int pos=f.lastIndexOf('.');
    if (pos>1) {
        f=f.left(pos+1);
    }

    if (f.endsWith('.')) {
        return f+(extension.startsWith('.') ? extension.mid(1) : extension);
    }
    return f+(extension.startsWith('.') ? extension : (QChar('.')+extension));
}

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

#ifndef Q_WS_WIN
gid_t Utils::getAudioGroupId()
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
#else
void Utils::setFilePerms(const QString &file)
{
    Q_UNUSED(file);
}
#endif

/*
 * Create directory, and set its permissions.
 * If user is a memeber of "audio" group, then set dir as owned by and writeable by "audio" group.
 */
bool Utils::createDir(const QString &dir, const QString &base)
{
    #ifdef Q_WS_WIN
    Q_UNUSED(base);
    #else
    //
    // Clear any umask before dir is created
    mode_t oldMask(umask(0000));
    gid_t gid=base.isEmpty() ? 0 : getAudioGroupId();
    #endif
    #ifdef ENABLE_KDE_SUPPORT
    bool status(KStandardDirs::makeDir(dir, 0==gid ? 0755 : 0775));
    #else
    bool status(QDir(dir).mkpath(dir));
    #endif
    #ifndef Q_WS_WIN
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
    #endif
    return status;
}

struct Thread : public QThread
{
    using QThread::msleep;
};

void Utils::msleep(int msecs)
{
    Thread::msleep(msecs);
}

void Utils::stopThread(QThread *thread)
{
    thread->quit();
    for(int i=0; i<10 && thread->isRunning(); ++i) {
        sleep();
    }
}

