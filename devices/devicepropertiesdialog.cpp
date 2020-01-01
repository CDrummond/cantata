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

#include "devicepropertiesdialog.h"
#include "devicepropertieswidget.h"

DevicePropertiesDialog::DevicePropertiesDialog(QWidget *parent)
    : Dialog(parent)
{
    setButtons(Ok|Cancel);
    setCaption(tr("Device Properties"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    devProp=new DevicePropertiesWidget(this);
    devProp->showRemoteConnectionNote(false);
    setMainWidget(devProp);
    setMinimumWidth(600);
}

void DevicePropertiesDialog::show(const QString &path, const DeviceOptions &opts, const QList<DeviceStorage> &storage, int props, int disabledProps)
{
    devProp->update(path, opts, storage, props, disabledProps);
    connect(devProp, SIGNAL(updated()), SLOT(enableOkButton()));
    Dialog::show();
    enableButtonOk(false);
}

void DevicePropertiesDialog::enableOkButton()
{
    enableButtonOk(devProp->isSaveable() && devProp->isModified());
}

void DevicePropertiesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
        emit updatedSettings(devProp->music(), devProp->settings());
        break;
    case Cancel:
        emit cancelled();
        reject();
        break;
    default:
        break;
    }

    if (Ok==button) {
        accept();
    }

    Dialog::slotButtonClicked(button);
}

#include "moc_devicepropertiesdialog.cpp"
