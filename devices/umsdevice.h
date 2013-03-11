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

#ifndef UMSDEVICE_H
#define UMSDEVICE_H

#include "fsdevice.h"
#ifdef ENABLE_KDE_SUPPORT
#include <solid/storageaccess.h>
#else
#include "solid-lite/storageaccess.h"
#endif

class UmsDevice : public FsDevice
{
    Q_OBJECT

public:
    UmsDevice(DevicesModel *m, Solid::Device &dev);
    virtual ~UmsDevice();

    void connectionStateChanged();
    void toggle();
    bool isConnected() const;
    double usedCapacity();
    QString capacityString();
    qint64 freeSpace();
    DevType devType() const { return Ums; }
    void saveOptions();
    void configure(QWidget *parent);
    bool supportsDisconnect() const { return true; }

private:
    void setup();

private Q_SLOTS:
    void saveProperties();
    void saveProperties(const QString &newPath, const DeviceOptions &opts);

private:
    Solid::StorageAccess *access;
    QStringList unusedParams;
};

#endif
