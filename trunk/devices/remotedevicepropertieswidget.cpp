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
#include "localize.h"
#include <QtGui/QTabWidget>
#include <QtGui/QIcon>
#include "lineedit.h"
#include "config.h"

enum Type {
    Type_SshFs,
    Type_File
    #ifdef ENABLE_MOUNTER
    , Type_Samba
    #endif
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
    type->addItem(i18n("Secure Shell (sshfs)"), (int)Type_SshFs);
    type->addItem(i18n("Locally Mounted Folder"), (int)Type_File);
    #ifdef ENABLE_MOUNTER
    type->addItem(i18n("Samba Shares"), (int)Type_Samba);
    #endif
}

void RemoteDevicePropertiesWidget::update(const RemoteFsDevice::Details &d, bool create, bool isConnected)
{
    #ifdef ENABLE_MOUNTER
    int t=d.isLocalFile() ? Type_File : (d.url.scheme()==RemoteFsDevice::constSshfsProtocol ? Type_SshFs : Type_Samba);
    #else
    int t=d.isLocalFile() ? Type_File : Type_SshFs;
    #endif
    setEnabled(d.isLocalFile() || !isConnected);
    infoLabel->setVisible(create);
    orig=d;
    name->setText(d.name);
    sshPort->setValue(22);
    #ifdef ENABLE_MOUNTER
    smbPort->setValue(445);
    #endif

    sshFolder->setText(QString());
    sshHost->setText(QString());
    sshUser->setText(QString());
    fileFolder->setText(QString());

    switch (t) {
    case Type_SshFs: {
        sshFolder->setText(d.url.path());
        sshPort->setValue(d.url.port());
        sshHost->setText(d.url.host());
        sshUser->setText(d.url.userName());
        break;
    }
    case Type_File:
        fileFolder->setText(d.url.path());
        break;
    #ifdef ENABLE_MOUNTER
    case Type_Samba: {
        smbFolder->setText(d.url.path());
        smbPort->setValue(d.url.port());
        smbHost->setText(d.url.host());
        smbUser->setText(d.url.userName());
        smbPassword->setText(d.url.password());
        if (d.url.hasQueryItem(RemoteFsDevice::constDomainQuery)) {
            smbDomain->setText(d.url.queryItemValue(RemoteFsDevice::constDomainQuery));
        } else {
            smbDomain->setText(QString());
        }
        break;
    }
    #endif
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
    #ifdef ENABLE_MOUNTER
    case Type_Samba:
        det.url.setHost(smbHost->text().trimmed());
        det.url.setUserName(smbUser->text().trimmed());
        det.url.setPath(smbFolder->text().trimmed());
        det.url.setPort(smbPort->value());
        det.url.setScheme(RemoteFsDevice::constSambaProtocol);
        det.url.setPassword(smbPassword->text().trimmed());
        if (!smbDomain->text().trimmed().isEmpty()) {
            det.url.addQueryItem(RemoteFsDevice::constDomainQuery, smbDomain->text().trimmed());
        }
        break;
    #endif
    }
    return det;
}
