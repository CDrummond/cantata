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

#include "remotedevicepropertieswidget.h"
#include "filenameschemedialog.h"
#include "gui/covers.h"
#include <QTabWidget>
#include <QIcon>
#include <QUrlQuery>
#include "support/lineedit.h"
#include "config.h"

enum Type {
    Type_SshFs,
    Type_File
};

RemoteDevicePropertiesWidget::RemoteDevicePropertiesWidget(QWidget *parent)
    : QWidget(parent)
    , modified(false)
    , saveable(false)
{
    setupUi(this);
    if (qobject_cast<QTabWidget *>(parent)) {
        verticalLayout->setMargin(4);
    }
    type->addItem(tr("Secure Shell (sshfs)"), (int)Type_SshFs);
    type->addItem(tr("Locally Mounted Folder"), (int)Type_File);
}

void RemoteDevicePropertiesWidget::update(const RemoteFsDevice::Details &d, bool create, bool isConnected)
{
    int t=d.isLocalFile() ? Type_File : Type_SshFs;
    setEnabled(d.isLocalFile() || !isConnected);
    infoLabel->setVisible(create);
    orig=d;
    name->setText(d.name);
    sshPort->setValue(22);

    connectionNote->setVisible(!d.isLocalFile() && isConnected);
    sshFolder->setText(QString());
    sshHost->setText(QString());
    sshUser->setText(QString());
    fileFolder->setText(QString());

    switch (t) {
    case Type_SshFs: {
        sshFolder->setText(d.url.path());
        if (0!=d.url.port()) {
            sshPort->setValue(d.url.port());
        }
        sshHost->setText(d.url.host());
        sshUser->setText(d.url.userName());
        sshExtra->setText(d.extraOptions);
        break;
    }
    case Type_File:
        fileFolder->setText(d.url.path());
        break;
    }

    name->setEnabled(d.isLocalFile() || !isConnected);

    connect(type, SIGNAL(currentIndexChanged(int)), this, SLOT(setType()));
    for (int i=1; i<type->count(); ++i) {
        if (type->itemData(i).toInt()==t) {
            type->setCurrentIndex(i);
            stackedWidget->setCurrentIndex(i);
            break;
        }
    }
    connect(name, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(sshHost, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(sshUser, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(sshFolder, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(sshPort, SIGNAL(valueChanged(int)), this, SLOT(checkSaveable()));
    connect(sshExtra, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(fileFolder, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    modified=false;
    setType();
    checkSaveable();
}

void RemoteDevicePropertiesWidget::setType()
{
    if (Type_SshFs==type->itemData(type->currentIndex()).toInt() && 0==sshPort->value()) {
        sshPort->setValue(22);
    }
}

void RemoteDevicePropertiesWidget::checkSaveable()
{
    RemoteFsDevice::Details det=details();
    modified=det!=orig;
    saveable=!det.isEmpty();
    emit updated();
}

RemoteFsDevice::Details RemoteDevicePropertiesWidget::details()
{
    int t=type->itemData(type->currentIndex()).toInt();
    RemoteFsDevice::Details det;

    det.name=name->text().trimmed();
    switch (t) {
    case Type_SshFs: {
        det.url.setHost(sshHost->text().trimmed());
        det.url.setUserName(sshUser->text().trimmed());
        det.url.setPath(sshFolder->text().trimmed());
        det.url.setPort(sshPort->value());
        det.url.setScheme(RemoteFsDevice::constSshfsProtocol);
        det.extraOptions=sshExtra->text().trimmed();
        break;
    }
    case Type_File: {
        QString path=fileFolder->text().trimmed();
        if (path.isEmpty()) {
            path="/";
        }
        det.url.setPath(path);
        det.url.setScheme(RemoteFsDevice::constFileProtocol);
        break;
    }

    }
    return det;
}

#include "moc_remotedevicepropertieswidget.cpp"
