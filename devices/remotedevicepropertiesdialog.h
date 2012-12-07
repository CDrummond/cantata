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

#ifndef REMOTEDEVICEPROPERTIESDIALOG_H
#define REMOTEDEVICEPROPERTIESDIALOG_H

#include "dialog.h"
#include "remotefsdevice.h"

class FilenameSchemeDialog;
class DevicePropertiesWidget;
class RemoteDevicePropertiesWidget;

class RemoteDevicePropertiesDialog : public Dialog
{
    Q_OBJECT

public:
    RemoteDevicePropertiesDialog(QWidget *parent);
    void show(const DeviceOptions &opts, const RemoteFsDevice::Details &det, int props, bool creating=false, bool isConnected=false);

Q_SIGNALS:
    void updatedSettings(const DeviceOptions &opts, const RemoteFsDevice::Details &det);
    void cancelled();

private Q_SLOTS:
    void enableOkButton();

private:
    void slotButtonClicked(int button);

private:
    RemoteDevicePropertiesWidget *remoteProp;
    DevicePropertiesWidget *devProp;
    bool isCreate;
};

#endif
