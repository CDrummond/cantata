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

#include "mountpoints.h"
#include <QtCore/QSocketNotifier>
#include <QtCore/QFile>
#include <QtCore/QStringList>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(MountPoints, instance)
#endif

MountPoints * MountPoints::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static MountPoints *instance=0;
    if(!instance) {
        instance=new MountPoints;
    }
    return instance;
    #endif
}

MountPoints::MountPoints()
    : token(0)
{
    mounts=new QFile("/proc/mounts", this);
    if (mounts && mounts->open(QIODevice::ReadOnly)) {
        QSocketNotifier *notifier = new QSocketNotifier(mounts->handle(), QSocketNotifier::Exception, mounts);
        connect(notifier,  SIGNAL(activated(int)), this, SLOT(updateMountPoints()));
        updateMountPoints();
    } else if (mounts) {
        mounts->deleteLater();
        mounts=0;
    }
}

void MountPoints::updateMountPoints()
{
    QSet<QString> entries;

    QFile f("/proc/mounts");
    if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QStringList lines=QString(f.readAll()).split("\n");
        foreach (const QString &l, lines) {
            QStringList parts = l.split(' ');
            if (parts.size()>=2) {
                entries.insert(QString(parts.at(1)).replace("\\040", " "));
            }
        }
    }

    if (entries!=current) {
        token++;
        current=entries;
        emit updated();
    }
}


bool MountPoints::isMounted(const QString &mp) const
{
    return current.contains(mp.endsWith('/') ? mp.left(mp.length()-1) : mp);
}
