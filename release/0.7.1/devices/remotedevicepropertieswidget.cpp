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
#include <KDE/KDirSelectDialog>
#include <QtGui/QTabWidget>
#include <QtGui/QIcon>
#include "lineedit.h"
#include "config.h"

enum Type {
    Type_SshFs,
    Type_File
    #ifdef ENABLE_KIO_REMOTE_DEVICES
    , Type_Kio
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
    #ifdef ENABLE_KIO_REMOTE_DEVICES
    type->addItem(i18n("Other Protocol"), (int)Type_Kio);
    #endif
    sshFolderButton->setIcon(QIcon::fromTheme("document-open"));
    fileFolder->setMode(KFile::Directory|KFile::ExistingOnly|KFile::LocalOnly);
    kioUrl->setMode(KFile::Directory|KFile::ExistingOnly);
}

void RemoteDevicePropertiesWidget::update(const RemoteFsDevice::Details &d, bool create, bool isConnected)
{
    #ifdef ENABLE_KIO_REMOTE_DEVICES
    int t=d.url.isLocalFile() ? Type_File : (d.isEmpty() || RemoteFsDevice::constSshfsProtocol==d.url.protocol() ? Type_SshFs : Type_Kio);
    #else
    int t=d.url.isLocalFile() ? Type_File : Type_SshFs;
    #endif
    setEnabled(d.url.isLocalFile() || !isConnected);
    infoLabel->setVisible(create);
    orig=d;
    name->setText(d.name);
    sshPort->setValue(22);
    sshFolder->setText(QString());
    sshHost->setText(QString());
    sshUser->setText(QString());
    fileFolder->setText(QString());
    kioUrl->setUrl(KUrl());
    switch (t) {
    case Type_SshFs:
        sshFolder->setText(d.url.path());
        sshPort->setValue(d.url.port());
        sshHost->setText(d.url.host());
        sshUser->setText(d.url.user());
        break;
    case Type_File:
        fileFolder->setText(d.url.path());
        break;
    #ifdef ENABLE_KIO_REMOTE_DEVICES
    case Type_Kio:
        kioUrl->setUrl(d.url);
    #endif
    }

    name->setEnabled(d.url.isLocalFile() || !isConnected);

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
    connect(sshFolderButton, SIGNAL(clicked()), this, SLOT(browseSftpFolder()));
    connect(sshPort, SIGNAL(valueChanged(int)), this, SLOT(checkSaveable()));
    connect(fileFolder, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(kioUrl, SIGNAL(urlSelected(const KUrl&)), this, SLOT(checkSaveable()));
    connect(kioUrl, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));

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

void RemoteDevicePropertiesWidget::browseSftpFolder()
{
    RemoteFsDevice::Details det=details();
    KUrl start=det.url;
    start.setProtocol(QLatin1String("sftp"));
    KUrl url=KDirSelectDialog::selectDirectory(start, false, this, i18n("Select Music Folder"));

    if (url.isValid() && QLatin1String("sftp")==url.protocol()) {
        sshHost->setText(url.host());
        sshUser->setText(url.user());
        sshPort->setValue(url.port());
        sshFolder->setText(url.path());
    }
}

void RemoteDevicePropertiesWidget::checkSaveable()
{
    RemoteFsDevice::Details det=details();
    modified=det!=orig;
    saveable=!det.isEmpty();
    sshFolderButton->setEnabled(!det.url.host().isEmpty() && det.url.port()>0);
    emit updated();
}

RemoteFsDevice::Details RemoteDevicePropertiesWidget::details()
{
    int t=type->itemData(type->currentIndex()).toInt();
    RemoteFsDevice::Details det;

    det.name=name->text().trimmed();
    switch (t) {
    case Type_SshFs: {
        QString h=sshHost->text().trimmed();
        QString u=sshUser->text().trimmed();
        QString f=sshFolder->text().trimmed();
        int p=sshPort->value();
        det.url=KUrl(RemoteFsDevice::constSshfsProtocol+QLatin1String("://")+ (u.isEmpty() ? QString() : (u+QChar('@')))
                                                             + h + (p<=0 ? QString() : QString(QChar(':')+QString::number(p)))
                                                             + (f.startsWith("/") ? f : (f.isEmpty() ? QString("/") : f)));
        break;
    }
    case Type_File: {
        QString f=fileFolder->text().trimmed();
        det.url=KUrl(QLatin1String("file://")+(f.startsWith("/") ? f : (f.isEmpty() ? QString("/") : f)));
        break;
    }
    #ifdef ENABLE_KIO_REMOTE_DEVICES
    case Type_Kio:
        det.url=KUrl(kioUrl->url());
        break;
    #endif
    }
    return det;
}
