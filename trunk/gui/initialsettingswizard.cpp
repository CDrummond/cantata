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

#include "initialsettingswizard.h"
#include "messagebox.h"
#include "settings.h"
#include "utils.h"
#include "icon.h"
#include "icons.h"
#include "mpduser.h"
#include <QDir>
#include <QDebug>

enum Pages {
    PAGE_INTRO,
    PAGE_CONNECTION,
    PAGE_FILES,
    PAGE_END
};

InitialSettingsWizard::InitialSettingsWizard(QWidget *p)
    : QWizard(p)
    , singleUserSupported(MPDUser::self()->isSupported())
{
    setupUi(this);
    connect(this, SIGNAL(currentIdChanged(int)), SLOT(pageChanged(int)));
    connect(this, SIGNAL(setDetails(const MPDConnectionDetails &)), MPDConnection::self(), SLOT(setDetails(const MPDConnectionDetails &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(const QString &, bool)), SLOT(showError(const QString &, bool)));
    connect(connectButton, SIGNAL(clicked(bool)), SLOT(connectToMpd()));
    connect(basicDir, SIGNAL(textChanged(QString)), SLOT(controlNextButton()));
    MPDConnection::self()->start();
    statusLabel->setText(i18n("Not Connected"));

    MPDConnectionDetails det=Settings::self()->connectionDetails();
    host->setText(det.hostname);
    port->setValue(det.port);
    password->setText(det.password);
    dir->setText(det.dir);
    #if defined Q_OS_WIN
    bool showGroupWarning=false;
    #else
    bool showGroupWarning=0==Utils::getGroupId();
    #endif
    groupWarningLabel->setVisible(showGroupWarning);
    groupWarningIcon->setVisible(showGroupWarning);
    int iconSize=Icon::dlgIconSize();
    groupWarningIcon->setPixmap(Icon("dialog-warning").pixmap(iconSize, iconSize));
    storeCoversInMpdDir->setChecked(Settings::self()->storeCoversInMpdDir());
    storeLyricsInMpdDir->setChecked(Settings::self()->storeLyricsInMpdDir());
    storeStreamsInMpdDir->setChecked(Settings::self()->storeStreamsInMpdDir());
    #ifdef ENABLE_KDE_SUPPORT
    introPage->setBackground(Icon("cantata"));
    #else
    introPage->setBackground(Icons::self()->appIcon);
    #endif
    connectionPage->setBackground(Icons::self()->libraryIcon);
    filesPage->setBackground(Icons::self()->filesIcon);
    finishedPage->setBackground(Icon("dialog-ok"));

    introStack->setCurrentIndex(singleUserSupported ? 1 : 0);
    basic->setChecked(singleUserSupported);
    advanced->setChecked(!singleUserSupported);

    QSize sz=size();
    // Adjust size for high-DPI setups...
    bool highDpi=fontMetrics().height()>20;
    if (highDpi) {
        foreach (int id, pageIds()) {
            QWizardPage *p=QWizard::page(id);
            p->adjustSize();
            QSize ps=p->size();
            if (ps.width()>sz.width()) {
                sz.setWidth(ps.width());
            }
            if (ps.height()>sz.height()) {
                sz.setHeight(ps.height());
            }
        }
    }

    if (sz.height()>(sz.width()*(highDpi ? 1.125 : 1.2))) {
        sz+=QSize(sz.height()*(highDpi ? 0.4 : 0.25), -(sz.height()*(highDpi ? 0.1 : 0.25)));
    }
    resize(sz);
}

InitialSettingsWizard::~InitialSettingsWizard()
{
}

MPDConnectionDetails InitialSettingsWizard::getDetails()
{
    if (basic->isChecked()) {
        MPDUser::self()->setMusicFolder(basicDir->text());
        return MPDUser::self()->details(true);
    }
    MPDConnectionDetails det;
    det.hostname=host->text().trimmed();
    det.port=port->value();
    det.password=password->text();
    det.dir=dir->text().trimmed();
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
    if (PAGE_CONNECTION==currentId()) {
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
    if (PAGE_CONNECTION==p) {
        connectionStack->setCurrentIndex(basic->isChecked() ? 1 : 0);
        if (basic->isChecked() && basicDir->text().isEmpty()) {
            QString dir=QDir::homePath()+"/Music";
            dir=dir.replace("//", "/");
            basicDir->setText(dir);
        }
        controlNextButton();
        return;
    }
    if (PAGE_FILES==p) {
        if (dir->text().trimmed().startsWith(QLatin1String("http:/"))) {
            storeCoversInMpdDir->setChecked(false);
            storeLyricsInMpdDir->setChecked(false);
            storeStreamsInMpdDir->setChecked(false);
            httpNote->setVisible(true);
        } else {
            httpNote->setVisible(false);
        }
    }
    button(NextButton)->setEnabled(PAGE_END!=p);
}

void InitialSettingsWizard::controlNextButton()
{
    bool isOk=false;

    if (basic->isChecked()) {
        isOk=!basicDir->text().isEmpty();
        if (isOk) {
            QDir d(basicDir->text());
            isOk=d.exists() && d.isReadable();
        }
    } else {
        isOk=MPDConnection::self()->isConnected();

        if (isOk) {
            MPDConnectionDetails det=getDetails();
            MPDConnectionDetails mpdDet=MPDConnection::self()->getDetails();
            isOk=det.hostname==mpdDet.hostname && (det.isLocal() || det.port==mpdDet.port);
        }
    }

    button(NextButton)->setEnabled(isOk);
}

void InitialSettingsWizard::accept()
{
    Settings::self()->saveConnectionDetails(getDetails());
    Settings::self()->saveStoreCoversInMpdDir(storeCoversInMpdDir->isChecked());
    Settings::self()->saveStoreLyricsInMpdDir(storeLyricsInMpdDir->isChecked());
    Settings::self()->saveStoreStreamsInMpdDir(storeStreamsInMpdDir->isChecked());

    if (basic->isChecked()) {
        Settings::self()->saveCurrentConnection(MPDUser::constName);
        emit setDetails(MPDUser::self()->details());
    } else {
        MPDUser::self()->cleanup();
    }
    Settings::self()->save(true);
    QDialog::accept();
}
