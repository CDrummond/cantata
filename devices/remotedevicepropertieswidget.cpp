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

#include "remotedevicepropertieswidget.h"
#include "filenameschemedialog.h"
#include "covers.h"
#include "localize.h"
#include <QTabWidget>
#include <QIcon>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include "lineedit.h"
#include "config.h"

enum Type {
    #ifdef ENABLE_MOUNTER
    Type_Samba,
    #endif
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
    #ifdef ENABLE_MOUNTER
    type->addItem(i18n("Samba Share"), (int)Type_Samba);
    #endif
    type->addItem(i18n("Secure Shell (sshfs)"), (int)Type_SshFs);
    type->addItem(i18n("Locally Mounted Folder"), (int)Type_File);
}

void RemoteDevicePropertiesWidget::update(const RemoteFsDevice::Details &d, bool create, bool isConnected)
{
    #ifdef ENABLE_MOUNTER
    int t=create ? Type_Samba : (d.isLocalFile() ? Type_File : (d.url.scheme()==RemoteFsDevice::constSshfsProtocol ? Type_SshFs : Type_Samba));
    #else
    int t=create ? Type_SshFs : (d.isLocalFile() ? Type_File : Type_SshFs);
    #endif
    setEnabled(d.isLocalFile() || !isConnected);
    infoLabel->setVisible(create);
    orig=d;
    name->setText(d.name);
    sshPort->setValue(22);
    #ifdef ENABLE_MOUNTER
    smbPort->setValue(445);
    #endif

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
        break;
    }
    case Type_File:
        fileFolder->setText(d.url.path());
        break;
    #ifdef ENABLE_MOUNTER
    case Type_Samba: {
        smbShare->setText(d.url.path());
        if (0!=d.url.port()) {
            smbPort->setValue(d.url.port());
        }
        smbHost->setText(d.url.host());
        smbUser->setText(d.url.userName());
        smbPassword->setText(d.url.password());
        #if QT_VERSION < 0x050000
        if (d.url.hasQueryItem(RemoteFsDevice::constDomainQuery)) {
            smbDomain->setText(d.url.queryItemValue(RemoteFsDevice::constDomainQuery));
        } else {
            smbDomain->setText(QString());
        }
        #else
        QUrlQuery q(d.url);
        if (q.hasQueryItem(RemoteFsDevice::constDomainQuery)) {
            smbDomain->setText(q.queryItemValue(RemoteFsDevice::constDomainQuery));
        } else {
            smbDomain->setText(QString());
        }
        #endif
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
    #ifdef ENABLE_MOUNTER
    connect(smbHost, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(smbUser, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(smbPassword, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(smbDomain, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(smbShare, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(smbPort, SIGNAL(valueChanged(int)), this, SLOT(checkSaveable()));
    #endif
    modified=false;
    setType();
    checkSaveable();
}

void RemoteDevicePropertiesWidget::setType()
{
    if (Type_SshFs==type->itemData(type->currentIndex()).toInt() && 0==sshPort->value()) {
        sshPort->setValue(22);
    }
    #ifdef ENABLE_MOUNTER
    if (Type_Samba==type->itemData(type->currentIndex()).toInt() && 0==smbPort->value()) {
        smbPort->setValue(445);
    }
    #endif
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
        det.url.setPath(smbShare->text().trimmed());
        det.url.setPort(smbPort->value());
        det.url.setScheme(RemoteFsDevice::constSambaProtocol);
        det.url.setPassword(smbPassword->text().trimmed());
        if (!smbDomain->text().trimmed().isEmpty()) {
            #if QT_VERSION < 0x050000
            det.url.addQueryItem(RemoteFsDevice::constDomainQuery, smbDomain->text().trimmed());
            #else
            QUrlQuery q;
            q.addQueryItem(RemoteFsDevice::constDomainQuery, smbDomain->text().trimmed());
            det.url.setQuery(q);
            #endif
        }
        break;
    #endif
    }
    return det;
}
