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

#include "freespaceinfo.h"
#include "utils.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KDiskFreeSpaceInfo>
#elif defined(Q_OS_UNIX)
#include <sys/statvfs.h>
#elif defined Q_OS_WIN
#include "windows.h"
#endif

FreeSpaceInfo::FreeSpaceInfo(const QString &path)
    : location(path)
    , isDirty(true)
    , totalSize(0)
    , usedSpace(0)
{
}

void FreeSpaceInfo::setPath(const QString &path)
{
    if (location!=path) {
        location=Utils::fixPath(path);
        isDirty=true;
    }
}

qulonglong FreeSpaceInfo::size()
{
    if (isDirty) {
        update();
    }
    return totalSize;
}

qulonglong FreeSpaceInfo::used()
{
    if (isDirty) {
        update();
    }
    return usedSpace;
}

void FreeSpaceInfo::update()
{
    #ifdef ENABLE_KDE_SUPPORT
    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(location);
    totalSize=inf.size();
    usedSpace=inf.used();
    #elif defined(Q_OS_UNIX)
    struct statvfs fs_info;
    if (0==statvfs(location.toLocal8Bit().constData(), &fs_info)) {
        totalSize=quint64(fs_info.f_blocks) * quint64(fs_info.f_bsize);
        usedSpace=totalSize-(quint64(fs_info.f_bavail) * quint64(fs_info.f_bsize));
    }
    #elif defined(Q_OS_WIN32)
    _ULARGE_INTEGER totalRet;
    _ULARGE_INTEGER freeRet;
    if (0!=GetDiskFreeSpaceEx(QDir::toNativeSeparators(location).toLocal8Bit().constData(), &freeRet, &totalRet, NULL)) {
        totalSize=totalRet.QuadPart;
        usedSpace=totalRet.QuadPart-freeRet.QuadPart;
    }
    #endif
    isDirty=false;
}
