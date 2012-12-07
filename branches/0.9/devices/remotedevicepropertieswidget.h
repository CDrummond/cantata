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

#ifndef REMOTEDEVICEPROPERTIESWIDGET_H
#define REMOTEDEVICEPROPERTIESWIDGET_H

#include "ui_remotedevicepropertieswidget.h"
#include "remotefsdevice.h"

class RemoteDevicePropertiesWidget : public QWidget, Ui::RemoteDevicePropertiesWidget
{
    Q_OBJECT

public:

    RemoteDevicePropertiesWidget(QWidget *parent);
    virtual ~RemoteDevicePropertiesWidget() { }
    void update(const RemoteFsDevice::Details &d, bool create, bool isConnected);
    RemoteFsDevice::Details details();
    const RemoteFsDevice::Details & origDetails() const { return orig; }
    bool isModified() const { return modified; }
    bool isSaveable() const { return saveable; }

Q_SIGNALS:
    void updated();

private Q_SLOTS:
    void checkSaveable();
    void setType();
    #ifdef ENABLE_KDE_SUPPORT
    void browseSftpFolder();
    #endif

private:
    RemoteFsDevice::Details orig;
    bool modified;
    bool saveable;
};

#endif
