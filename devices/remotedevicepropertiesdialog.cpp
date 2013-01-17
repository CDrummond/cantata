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

#include "remotedevicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "remotedevicepropertieswidget.h"
#include "devicesmodel.h"
#include "localize.h"
#include "messagebox.h"
#include "icon.h"
#include <QtGui/QTabWidget>
#include <QtGui/QIcon>

RemoteDevicePropertiesDialog::RemoteDevicePropertiesDialog(QWidget *parent)
    : Dialog(parent)
    , isCreate(false)
{
    setButtons(Ok|Cancel);
    setCaption(i18n("Device Properties"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    tab=new QTabWidget(this);
    remoteProp=new RemoteDevicePropertiesWidget(tab);
    devProp=new DevicePropertiesWidget(tab);
    tab->addTab(remoteProp, QIcon::fromTheme("network-server"), i18n("Connection"));
    tab->addTab(devProp, Icon("cantata-view-media-library"), i18n("Music Library"));
    setMainWidget(tab);
}

void RemoteDevicePropertiesDialog::show(const DeviceOptions &opts, const RemoteFsDevice::Details &det, int props, bool create, bool isConnected)
{
    isCreate=create;
    if (isCreate) {
        setCaption(i18n("Add Device"));
    }

    if (create) {
        devProp->setVisible(false);
    } else {
        tab->setCurrentIndex(isConnected ? 1 : 0);
    }
    devProp->setEnabled(!create && isConnected);
    devProp->showRemoteConnectionNote(!isConnected);
    devProp->update(QString(), opts, props);
    remoteProp->update(det, create, isConnected);
    connect(devProp, SIGNAL(updated()), SLOT(enableOkButton()));
    connect(remoteProp, SIGNAL(updated()), SLOT(enableOkButton()));
    Dialog::show();
    enableButtonOk(false);
}

void RemoteDevicePropertiesDialog::enableOkButton()
{
    bool useDevProp=devProp->isEnabled();
    enableButtonOk(remoteProp->isSaveable() && (!useDevProp || devProp->isSaveable()) &&
                   (isCreate || remoteProp->isModified() || !useDevProp || devProp->isModified()));
}

void RemoteDevicePropertiesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        RemoteFsDevice::Details d=remoteProp->details();
        if (d.name!=remoteProp->origDetails().name && DevicesModel::self()->device(RemoteFsDevice::createUdi(d.name))) {
            MessageBox::error(this, i18n("A remote device named \"%1\" already exists!\nPlease choose a different name").arg(d.name));
        } else {
            emit updatedSettings(devProp->settings(), remoteProp->details());
            accept();
        }
        break;
    }
    case Cancel:
        emit cancelled();
        reject();
        break;
    default:
        Dialog::slotButtonClicked(button);
        break;
    }
}
