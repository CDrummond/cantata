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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KDirSelectDialog>
#endif
#include <QtGui/QTabWidget>
#include <QtGui/QIcon>
#include <QtCore/QUrl>
#include "lineedit.h"
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
    type->addItem(i18n("Secure Shell (sshfs)"), (int)Type_SshFs);
    type->addItem(i18n("Locally Mounted Folder"), (int)Type_File);
    #ifdef ENABLE_KDE_SUPPORT
    sshFolderButton->setIcon(QIcon::fromTheme("document-open"));
    fileFolder->setMode(KFile::Directory|KFile::ExistingOnly|KFile::LocalOnly);
    kioUrl->setMode(KFile::Directory|KFile::ExistingOnly);
    #else
    sshFolderButton->setVisible(false);
    #endif
}

void RemoteDevicePropertiesWidget::update(const RemoteFsDevice::Details &d, bool create, bool isConnected)
{
    int t=d.isLocalFile() ? Type_File : Type_SshFs;
    setEnabled(d.isLocalFile() || !isConnected);
    infoLabel->setVisible(create);
    orig=d;
    name->setText(d.name);
    sshPort->setValue(22);
    sshFolder->setText(QString());
    sshHost->setText(QString());
    sshUser->setText(QString());
    fileFolder->setText(QString());
    #ifdef ENABLE_KDE_SUPPORT
    kioUrl->setUrl(KUrl());
    #else
    kioUrl->setText(QString());
    #endif
    switch (t) {
    case Type_SshFs: {
        QUrl url(d.url);
        sshFolder->setText(url.path());
        sshPort->setValue(url.port());
        sshHost->setText(url.host());
        sshUser->setText(url.userName());
        break;
    }
    case Type_File:
        fileFolder->setText(d.url);
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

#ifdef ENABLE_KDE_SUPPORT
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
#endif

void RemoteDevicePropertiesWidget::checkSaveable()
{
    RemoteFsDevice::Details det=details();
    modified=det!=orig;
    saveable=!det.isEmpty();
    #ifdef ENABLE_KDE_SUPPORT
    sshFolderButton->setEnabled(!det.url.isEmpty());
    #endif
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
        det.url=QString(RemoteFsDevice::constSshfsProtocol+QLatin1String("://")+ (u.isEmpty() ? QString() : (u+QChar('@')))
                                                             + h + (p<=0 ? QString() : QString(QChar(':')+QString::number(p)))
                                                             + (f.startsWith("/") ? f : (f.isEmpty() ? QString("/") : f)));
        break;
    }
    case Type_File: {
        QString f=fileFolder->text().trimmed();
        det.url=QString(f.startsWith("/") ? f : (f.isEmpty() ? QString("/") : f));
        break;
    }
    }
    return det;
}
