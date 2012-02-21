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

#include "remotedevicepropertieswidget.h"
#include "filenameschemedialog.h"
#include "covers.h"
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KMessageBox>
#include <QtGui/QTabWidget>
#include "lineedit.h"

RemoteDevicePropertiesWidget::RemoteDevicePropertiesWidget(QWidget *parent)
    : QWidget(parent)
    , isCreating(false)
{
    setupUi(this);
    if (qobject_cast<QTabWidget *>(parent)) {
        formLayout->setMargin(4);
    }
}

void RemoteDevicePropertiesWidget::update(const RemoteDevice::Details &d, bool create)
{
    isCreating=create;
    orig=d;
    name->setText(d.name);
    host->setText(d.host);
    user->setText(d.user);
    folder->setText(d.folder);
    port->setValue(d.port);

    connect(name, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(host, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(user, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(folder, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(port, SIGNAL(valueChanged(int)), this, SLOT(checkSaveable()));
    checkSaveable();
}

void RemoteDevicePropertiesWidget::checkSaveable()
{
    RemoteDevice::Details det=details();
    saveable=isCreating ? !det.isEmpty() : (!det.isEmpty() && det!=orig);
    emit updated();
}

RemoteDevice::Details RemoteDevicePropertiesWidget::details()
{
    RemoteDevice::Details det;
    det.name=name->text().trimmed();
    det.host=host->text().trimmed();
    det.user=user->text().trimmed();
    det.folder=folder->text().trimmed();
    det.port=port->value();
    det.protocol=RemoteDevice::Prot_Sshfs; // Only sshfs at the moment!!!
    return det;
}
