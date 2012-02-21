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

#ifndef MOUNTDEVICE_H
#define MOUNTDEVICE_H

#include "fsdevice.h"
#include <sys/types.h>

class MountDevice : public FsDevice
{
    Q_OBJECT

public:
    MountDevice(DevicesModel *m, const QString &mp, const QString &dp);
    virtual ~MountDevice();

    virtual bool mount()=0;
    virtual bool unmount()=0;
    virtual QString key()=0;
    bool isConnected() const;
    double usedCapacity();
    QString capacityString();
    qint64 freeSpace();

    virtual QString icon() const {
        return QLatin1String("network-server");
    }

protected:
    void setup();

protected Q_SLOTS:
    void saveProperties();
    void saveProperties(const QString &newPath, const QString &newCoverFileName, const Device::Options &opts);

protected:
    mutable time_t lastCheck;
    mutable bool isMounted;
    QString mountPoint;
    QString devPath;
};

#endif
