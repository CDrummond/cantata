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

#include "remotedevicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "remotedevicepropertieswidget.h"
#include "devicesmodel.h"
#include <QtGui/QTabWidget>
#include <QtGui/QIcon>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KMessageBox>

RemoteDevicePropertiesDialog::RemoteDevicePropertiesDialog(QWidget *parent)
    : KDialog(parent)
    , isCreate(false)
{
    setButtons(KDialog::Ok|KDialog::Cancel);
    setCaption(i18n("Device Properties"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    QTabWidget *tab=new QTabWidget(this);
    remoteProp=new RemoteDevicePropertiesWidget(tab);
    devProp=new DevicePropertiesWidget(tab);
    tab->addTab(remoteProp, QIcon::fromTheme("network-server"), i18n("Connection"));
    tab->addTab(devProp, QIcon::fromTheme("audio-ac3"), i18n("Music Library"));
    setMainWidget(tab);
}

void RemoteDevicePropertiesDialog::show(const QString &coverName, const Device::Options &opts, const RemoteDevice::Details &det, int props, bool create)
{
    isCreate=create;
    if (isCreate) {
        setCaption(i18n("Add Device"));
    }
    devProp->update(QString(), coverName, opts, props);
    remoteProp->update(det, create);
    connect(devProp, SIGNAL(updated()), SLOT(enableOkButton()));
    connect(remoteProp, SIGNAL(updated()), SLOT(enableOkButton()));
    KDialog::show();
    enableButtonOk(false);
}

void RemoteDevicePropertiesDialog::enableOkButton()
{
    enableButtonOk(remoteProp->isSaveable() && devProp->isSaveable() &&
                   (isCreate || remoteProp->isModified() || devProp->isModified()));
}

void RemoteDevicePropertiesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case KDialog::Ok: {
        RemoteDevice::Details d=remoteProp->details();
        if (d.name!=remoteProp->origDetails().name && DevicesModel::self()->device(RemoteDevice::createUdi(d.name))) {
            KMessageBox::error(this, i18n("A remote device named \"%1\" already exists!\nPlease choose a different name", d.name));
        } else {
            emit updatedSettings(devProp->cover(), devProp->settings(), remoteProp->details());
            accept();
        }
        break;
    }
    case KDialog::Cancel:
        emit cancelled();
        reject();
        break;
    default:
        KDialog::slotButtonClicked(button);
        break;
    }
}
