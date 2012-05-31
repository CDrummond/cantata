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

#ifndef UTILS_H
#define UTILS_H

#include <math.h>
#include <sys/types.h>
#include <QtCore/qglobal.h>

class QString;
class QThread;

namespace Utils
{
    inline bool equal(double d1, double d2, double precision=0.0001)
    {
        return (fabs(d1 - d2) < precision);
    }

    extern QString dirSyntax(const QString &d);
    extern QString getDir(const QString &file);
    extern QString changeExtension(const QString &file, const QString &extension);
    extern void moveDir(const QString &from, const QString &to, const QString &base, const QString &coverFile);
    extern void cleanDir(const QString &dir, const QString &base, const QString &coverFile, int level=0);
    #ifndef Q_WS_WIN
    extern gid_t getAudioGroupId(); // Return 0 if user is not in audio group, otherwise returns audio group ID
    #endif
    extern void setFilePerms(const QString &file);
    extern bool createDir(const QString &dir, const QString &base);
    extern void msleep(int msecs);
    inline void sleep() { msleep(100); }
    extern void stopThread(QThread *thread);
};

#endif
