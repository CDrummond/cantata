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

#ifndef UTILS_H
#define UTILS_H

#include <math.h>
#include <sys/types.h>
#include <qglobal.h>
#include <QString>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KStandardDirs>
#endif

class QString;

namespace Utils
{
    inline bool equal(double d1, double d2, double precision=0.0001)
    {
        return (fabs(d1 - d2) < precision);
    }

    extern QString strippedText(QString s);
    extern QString fixPath(const QString &d);
    extern QString getDir(const QString &file);
    extern QString getFile(const QString &file);
    extern QString changeExtension(const QString &file, const QString &extension);
    #ifndef Q_OS_WIN
    extern gid_t getGroupId(const char *groupName="users"); // Return 0 if user is not in group, otherwise returns group ID
    #endif
    extern void setFilePerms(const QString &file, const char *groupName="users");
    extern bool createDir(const QString &dir, const QString &base, const char *groupName="users");
    extern void msleep(int msecs);
    inline void sleep() { msleep(100); }

    #ifdef ENABLE_KDE_SUPPORT
    inline QString findExe(const QString &appname, const QString &pathstr=QString()) { return KStandardDirs::findExe(appname, pathstr); }
    inline QString formatByteSize(double size) { return KGlobal::locale()->formatByteSize(size, 1); }
    #else
    extern QString findExe(const QString &appname, const QString &pathstr=QString());
    extern QString formatByteSize(double size);
    #endif

    extern QString cleanPath(const QString &p);
    extern QString configDir(const QString &sub=QString(), bool create=false);
    extern QString cacheDir(const QString &sub=QString(), bool create=true);
    extern void clearOldCache(const QString &sub, int maxAge);
    extern void touchFile(const QString &fileName);
};

#endif
