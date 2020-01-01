/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "solid-lite/storageaccess.h"

class UmsDevice : public FsDevice
{
    Q_OBJECT

public:
    UmsDevice(MusicLibraryModel *m, Solid::Device &dev);
    ~UmsDevice() override;

    void connectionStateChanged() override;
    void toggle() override;
    bool isConnected() const override;
    double usedCapacity() override;
    QString capacityString() override;
    qint64 freeSpace() override;
    DevType devType() const override { return Ums; }
    void saveOptions() override;
    void configure(QWidget *parent) override;
    bool supportsDisconnect() const override { return true; }

private:
    void setup();

private Q_SLOTS:
    void saveProperties();
    void saveProperties(const QString &newPath, const DeviceOptions &opts);

private:
    QString defaultName;
    Solid::StorageAccess *access;
    QStringList unusedParams;
};

#endif
