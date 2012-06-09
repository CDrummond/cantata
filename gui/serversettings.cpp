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

#include "serversettings.h"
#include "settings.h"
#include "localize.h"
#include "inputdialog.h"
#include "messagebox.h"
#include <QtCore/QDir>
#include <QtGui/QComboBox>
#include <QtGui/QPushButton>
#include <QtGui/QIcon>

ServerSettings::ServerSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_DEVICES_SUPPORT
    musicFolderNoteLabel->setText(musicFolderNoteLabel->text()+
                                  i18n("<i> This folder will also be used to locate music files "
                                       "for transferring to (and from) devices.</i>"));
    #endif
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), this, SLOT(mpdConnectionStateChanged(bool)));
    connect(combo, SIGNAL(activated(int)), SLOT(showDetails(int)));
    connect(saveButton, SIGNAL(clicked(bool)), SLOT(saveAs()));
    connect(removeButton, SIGNAL(clicked(bool)), SLOT(remove()));
    connect(connectButton, SIGNAL(clicked(bool)), SLOT(toggleConnection()));
    saveButton->setIcon(QIcon::fromTheme("document-save-as"));
    removeButton->setIcon(QIcon::fromTheme("edit-delete"));
};

void ServerSettings::load()
{
    QList<MPDConnectionDetails> all=Settings::self()->allConnections();
    QString currentCon=Settings::self()->currentConnection();
    MPDConnectionDetails current=MPDConnection::self()->getDetails();

    qSort(all);
    combo->clear();
    int idx=0;
    int cur=0;
    foreach (const MPDConnectionDetails &d, all) {
        combo->addItem(d.name.isEmpty() ? i18n("Default") : d.name, d.name);
        if (d.name==currentCon) {
            cur=idx;
        }
        idx++;
    }
    showDetails(cur);
}

void ServerSettings::save()
{
    Settings::self()->saveConnectionDetails(getDetails());
}

void ServerSettings::mpdConnectionStateChanged(bool c)
{
    enableWidgets(!c || MPDConnection::self()->getDetails()!=getDetails());
    connectButton->setEnabled(true);
}

void ServerSettings::showDetails(int index)
{
    MPDConnectionDetails d=Settings::self()->connectionDetails(combo->itemData(index).toString());
    setDetails(d);
    enableWidgets(!MPDConnection::self()->isConnected() || MPDConnection::self()->getDetails()!=d);
}

void ServerSettings::toggleConnection()
{
    bool con=removeButton->isEnabled();
    enableWidgets(!con);
    if (con) {
        emit connectTo(getDetails());
    } else {
        emit disconnectFromMpd();
    }
    connectButton->setEnabled(false);
}

void ServerSettings::saveAs()
{
    bool ok=false;
    int currentIndex=combo->currentIndex();
    QString name=combo->itemText(currentIndex);

    for (;;) {
        name=InputDialog::getText(i18n("Save As"), i18n("Enter name for settings:"), name, &ok, this).trimmed();
        if (!ok || name.isEmpty()) {
            return;
        }

        bool found=false;
        int idx=0;
        for (idx=0; idx<combo->count() && !found; ++idx) {
            if (combo->itemText(idx)==name || combo->itemData(idx).toString()==name) {
                found=true;
                break;
            }
        }

        if (found && idx!=currentIndex) {
            switch (MessageBox::warningYesNoCancel(this, i18n("A setting named %1 already exists!\nOverwrite?").arg(name))) {
            case MessageBox::No:
                continue;
            case MessageBox::Cancel:
                return;
            case MessageBox::Yes:
                break;
            }
        }

        MPDConnectionDetails details=getDetails();
        details.name=name==combo->itemText(0) && combo->itemData(0).toString().isEmpty() ? QString() : name;
        bool needToReconnect=MPDConnection::self()->isConnected() && MPDConnection::self()->getDetails()==Settings::self()->connectionDetails(details.name);

        Settings::self()->saveConnectionDetails(details);
        if (found) {
            if (idx!=currentIndex) {
                combo->setCurrentIndex(idx);
            }
        } else {
            combo->addItem(details.name, details.name);
            combo->setCurrentIndex(combo->count()-1);
        }

        if (needToReconnect) {
            emit disconnectFromMpd();
            emit connectTo(details);
        }
        break;
    }
}

void ServerSettings::remove()
{
    int index=combo->currentIndex();
    QString name=combo->itemData(index).toString();
    if (combo->count()>1 && MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Delete %1?").arg(name))) {
        bool isLast=index==(combo->count()-1);
        Settings::self()->removeConnectionDetails(combo->itemData(index).toString());
        combo->removeItem(index);
        combo->setCurrentIndex(isLast ? index-1 : index);
        showDetails(combo->currentIndex());
    }
}

void ServerSettings::enableWidgets(bool e)
{
//     host->setEnabled(e);
//     port->setEnabled(e);
//     password->setEnabled(e);
//     dir->setEnabled(e);
//     hostLabel->setEnabled(e);
//     portLabel->setEnabled(e);
//     passwordLabel->setEnabled(e);
//     dirLabel->setEnabled(e);
    connectButton->setText(e ? i18n("Connect") : i18n("Disconnect"));
    connectButton->setIcon(QIcon::fromTheme(e ? "network-connect" : "network-disconnect"));
    removeButton->setEnabled(e);
//     saveButton->setEnabled(e);
}

void ServerSettings::setDetails(const MPDConnectionDetails &details)
{
    host->setText(details.hostname);
    port->setValue(details.port);
    password->setText(details.password);
    dir->setText(details.dir);
}

MPDConnectionDetails ServerSettings::getDetails() const
{
    MPDConnectionDetails details;
    details.name=combo->itemData(combo->currentIndex()).toString();
    details.hostname=host->text();
    details.port=port->value();
    details.password=password->text();
    details.dir=dir->text();
    details.dirReadable=details.dir.isEmpty() ? false : QDir(details.dir).isReadable();
    return details;
}
