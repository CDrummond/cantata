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

#include "initialsettingswizard.h"
#include "messagebox.h"
#include "settings.h"
#include "utils.h"
#include "icon.h"
#include <QtCore/QDir>

static const int constIconSize=48;

InitialSettingsWizard::InitialSettingsWizard(QWidget *p)
    : QWizard(p)
{
    setupUi(this);
    connect(this, SIGNAL(currentIdChanged(int)), SLOT(pageChanged(int)));
    connect(this, SIGNAL(setDetails(const MPDConnectionDetails &)), MPDConnection::self(), SLOT(setDetails(const MPDConnectionDetails &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(const QString &, bool)), SLOT(showError(const QString &, bool)));
    connect(connectButton, SIGNAL(clicked(bool)), SLOT(connectToMpd()));
    MPDConnection::self()->start();
    statusLabel->setText(i18n("Not Connected"));

    MPDConnectionDetails det=Settings::self()->connectionDetails();
    host->setText(det.hostname);
    port->setValue(det.port);
    password->setText(det.password);
    dir->setText(det.dir);
    bool showGroupWarning=0==Utils::getGroupId();
    groupWarningLabel->setVisible(showGroupWarning);
    groupWarningIcon->setVisible(showGroupWarning);
    groupWarningIcon->setPixmap(Icon("dialog-warning").pixmap(constIconSize, constIconSize));
}

InitialSettingsWizard::~InitialSettingsWizard()
{
}

MPDConnectionDetails InitialSettingsWizard::getDetails()
{
    MPDConnectionDetails det;
    det.hostname=host->text().trimmed();
    det.port=port->value();
    det.password=password->text();
    det.dir=dir->text();
    det.dirReadable=det.dir.isEmpty() ? false : QDir(det.dir).isReadable();
    det.dynamizerPort=0;
    return det;
}

void InitialSettingsWizard::connectToMpd()
{
    emit setDetails(getDetails());
}

void InitialSettingsWizard::mpdConnectionStateChanged(bool c)
{
    statusLabel->setText(c ? i18n("Connection Established") : i18n("Connection Failed"));
    if (1==currentId()) {
        controlNextButton();
    }
}

void InitialSettingsWizard::showError(const QString &message, bool showActions)
{
    Q_UNUSED(showActions)
    MessageBox::error(this, message);
}

void InitialSettingsWizard::pageChanged(int p)
{
    if (1==p) {
        controlNextButton();
    } else {
        button(NextButton)->setEnabled(0==p);
    }
}

void InitialSettingsWizard::controlNextButton()
{
    bool isOk=MPDConnection::self()->isConnected();

    if (isOk) {
       MPDConnectionDetails det=getDetails();
       MPDConnectionDetails mpdDet=MPDConnection::self()->getDetails();
       isOk=det.hostname==mpdDet.hostname && det.dir==mpdDet.dir && (det.isLocal() || det.port==mpdDet.port);
    }

    button(NextButton)->setEnabled(isOk);
}
