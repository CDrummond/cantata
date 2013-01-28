/*
    Copyright 2007 Kevin Ottens <ervin@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include "halfstabhandling.h"

#include <QFile>
#include <QMultiHash>
#include <QObject>
#include <QProcess>
#include <QTextStream>
#include <QTime>

#include <solid-lite/soliddefs_p.h>

#ifdef HAVE_MNTENT_H
#include <mntent.h>
#elif defined(HAVE_SYS_MNTENT_H)
#include <sys/mntent.h>
#endif

#ifdef Q_OS_SOLARIS
#define FSTAB "/etc/vfstab"
#else
#define FSTAB "/etc/fstab"
#endif

typedef QMultiHash<QString, QString> QStringMultiHash;
SOLID_GLOBAL_STATIC(QStringMultiHash, globalMountPointsCache)

QString _k_resolveSymLink(const QString &filename)
{
    QString resolved = filename;
    QString tmp = QFile::symLinkTarget(filename);

    while (!tmp.isEmpty()) {
        resolved = tmp;
        tmp = QFile::symLinkTarget(resolved);
    }

    return resolved;
}

bool _k_isNetworkFileSystem(const QString &fstype, const QString &devName)
{
    if (fstype == "nfs"
     || fstype == "nfs4"
     || fstype == "smbfs"
     || fstype == "cifs"
     || devName.startsWith(QLatin1String("//"))) {
        return true;
    }
    return false;
}


void _k_updateMountPointsCache()
{
    static bool firstCall = true;
    static QTime elapsedTime;

    if (firstCall) {
        firstCall = false;
        elapsedTime.start();
    } else if (elapsedTime.elapsed()>10000) {
        elapsedTime.restart();
    } else {
        return;
    }

    globalMountPointsCache->clear();

#ifdef HAVE_SETMNTENT

    struct mntent *fstab;
    if ((fstab = setmntent(FSTAB, "r")) == 0) {
        return;
    }

    struct mntent *fe;
    while ((fe = getmntent(fstab)) != 0) {
        if (!_k_isNetworkFileSystem(fe->mnt_type, fe->mnt_fsname)) {
            const QString device = _k_resolveSymLink(QFile::decodeName(fe->mnt_fsname));
            const QString mountpoint = _k_resolveSymLink(QFile::decodeName(fe->mnt_dir));

            globalMountPointsCache->insert(device, mountpoint);
        }
    }

    endmntent(fstab);

#else

    QFile fstab(FSTAB);
    if (!fstab.open(QIODevice::ReadOnly)) {
        return;
    }

    QTextStream stream(&fstab);
    QString line;

    while (!stream.atEnd()) {
        line = stream.readLine().simplified();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        // not empty or commented out by '#'
        const QStringList items = line.split(' ');

#ifdef Q_OS_SOLARIS
        if (items.count() < 5) {
            continue;
        }
#else
        if (items.count() < 4) {
            continue;
        }
#endif
        //prevent accessing a blocking directory
        if (!_k_isNetworkFileSystem(items.at(2), items.at(0))) {
            const QString device = _k_resolveSymLink(items.at(0));
            const QString mountpoint = _k_resolveSymLink(items.at(1));

            globalMountPointsCache->insert(device, mountpoint);
        }
    }

    fstab.close();
#endif
}

bool Solid::Backends::Hal::FstabHandling::isInFstab(const QString &device)
{
    _k_updateMountPointsCache();
    const QString deviceToFind = _k_resolveSymLink(device);

    return globalMountPointsCache->contains(deviceToFind);
}

QStringList Solid::Backends::Hal::FstabHandling::possibleMountPoints(const QString &device)
{
    _k_updateMountPointsCache();
    const QString deviceToFind = _k_resolveSymLink(device);

    return globalMountPointsCache->values(deviceToFind);
}

QProcess *Solid::Backends::Hal::FstabHandling::callSystemCommand(const QString &commandName,
                                                                 const QStringList &args,
                                                                 QObject *obj, const char *slot)
{
    QStringList env = QProcess::systemEnvironment();
    env.replaceInStrings(QRegExp("^PATH=(.*)", Qt::CaseInsensitive), "PATH=/sbin:/bin:/usr/sbin/:/usr/bin");

    QProcess *process = new QProcess(obj);

    QObject::connect(process, SIGNAL(finished(int,QProcess::ExitStatus)),
                     obj, slot);

    process->setEnvironment(env);
    process->start(commandName, args);

    if (process->waitForStarted()) {
        return process;
    } else {
        delete process;
        return 0;
    }
}

QProcess *Solid::Backends::Hal::FstabHandling::callSystemCommand(const QString &commandName,
                                                                 const QString &device,
                                                                 QObject *obj, const char *slot)
{
    return callSystemCommand(commandName, QStringList() << device, obj, slot);
}

