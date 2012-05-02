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

RemoteDevicePropertiesWidget::RemoteDevicePropertiesWidget(QWidget *parent)
    : QWidget(parent)
    , modified(false)
    , saveable(false)
{
    setupUi(this);
    if (qobject_cast<QTabWidget *>(parent)) {
        verticalLayout->setMargin(4);
    }
    type->addItem(i18n("Secure Shell (sshfs)"), (int)RemoteFsDevice::Prot_Sshfs);
    type->addItem(i18n("Locally Mounted Folder"), (int)RemoteFsDevice::Prot_File);
    folderButton->setIcon(QIcon::fromTheme("document-open"));
}

void RemoteDevicePropertiesWidget::update(const RemoteFsDevice::Details &d, bool create, bool isConnected)
{
    setEnabled(!isConnected);
    infoLabel->setVisible(create);
    orig=d;
    name->setText(d.name);
    host->setText(d.host);
    user->setText(d.user);
    folder->setText(d.folder);
    port->setValue(d.port);

    for (int i=1; i<type->count(); ++i) {
        if (type->itemData(i).toInt()==d.protocol) {
            type->setCurrentIndex(i);
            break;
        }
    }

    name->setEnabled(d.protocol==RemoteFsDevice::Prot_File || !isConnected);
    connect(name, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(host, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(user, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(folder, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(port, SIGNAL(valueChanged(int)), this, SLOT(checkSaveable()));
    connect(type, SIGNAL(currentIndexChanged(int)), this, SLOT(setType()));
    connect(folderButton, SIGNAL(clicked()), this, SLOT(browseSftpFolder()));

    modified=false;
    setType();
    checkSaveable();
}

void RemoteDevicePropertiesWidget::setType()
{
    int t=type->itemData(type->currentIndex()).toInt();
    hostLabel->setEnabled(RemoteFsDevice::Prot_File!=t);
    host->setEnabled(RemoteFsDevice::Prot_File!=t);
    userLabel->setEnabled(RemoteFsDevice::Prot_File!=t);
    user->setEnabled(RemoteFsDevice::Prot_File!=t);
    portLabel->setEnabled(RemoteFsDevice::Prot_File!=t);
    port->setEnabled(RemoteFsDevice::Prot_File!=t);
    folder->setButtonVisible(RemoteFsDevice::Prot_File==t);

    folder->lineEdit()->setCompletionModeDisabled(KGlobalSettings::CompletionAuto, RemoteFsDevice::Prot_File!=t);
    folder->lineEdit()->setCompletionModeDisabled(KGlobalSettings::CompletionMan, RemoteFsDevice::Prot_File!=t);
    folder->lineEdit()->setCompletionModeDisabled(KGlobalSettings::CompletionShell, RemoteFsDevice::Prot_File!=t);
    folder->lineEdit()->setCompletionModeDisabled(KGlobalSettings::CompletionPopup, RemoteFsDevice::Prot_File!=t);
    folder->lineEdit()->setCompletionModeDisabled(KGlobalSettings::CompletionPopupAuto, RemoteFsDevice::Prot_File!=t);
    folder->lineEdit()->setCompletionMode(RemoteFsDevice::Prot_File==t ? KGlobalSettings::completionMode() : KGlobalSettings::CompletionNone);

    if (RemoteFsDevice::Prot_Sshfs==t && 0==port->value()) {
        port->setValue(22);
    }
    folderButton->setVisible(RemoteFsDevice::Prot_Sshfs==t);
}

void RemoteDevicePropertiesWidget::browseSftpFolder()
{
    RemoteFsDevice::Details det=details();
    KUrl url=KDirSelectDialog::selectDirectory(KUrl("sftp://"+ (det.user.isEmpty() ? QString() : (det.user+QChar('@')))
                                                             + det.host + (det.port<=0 ? QString() : QString(QChar(':')+QString::number(det.port)))
                                                             + (det.folder.startsWith("/") ? det.folder : (det.folder.isEmpty() ? QString("/") : det.folder))),
                                               false, this, i18n("Select Music Folder"));

    if (url.isValid() && QLatin1String("sftp")==url.protocol()) {
        host->setText(url.host());
        user->setText(url.user());
        port->setValue(url.port());
        folder->setText(url.path());
    }
}

void RemoteDevicePropertiesWidget::checkSaveable()
{
    RemoteFsDevice::Details det=details();
    modified=det!=orig;
    saveable=!det.isEmpty();
    folderButton->setEnabled(!det.host.isEmpty() && det.port>0);
    emit updated();
}

RemoteFsDevice::Details RemoteDevicePropertiesWidget::details()
{
    RemoteFsDevice::Details det;
    det.name=name->text().trimmed();
    det.host=host->text().trimmed();
    det.user=user->text().trimmed();
    det.folder=folder->text().trimmed();
    det.port=port->value();
    det.protocol=(RemoteFsDevice::Protocol)type->itemData(type->currentIndex()).toInt();
    return det;
}
