/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/messagebox.h"
#include "settings.h"
#include "support/utils.h"
#include "support/icon.h"
#include "widgets/icons.h"
#ifdef ENABLE_SIMPLE_MPD_SUPPORT
#include "mpd-interface/mpduser.h"
#endif
#include <QDir>
#if QT_VERSION > 0x050000
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif

enum Pages {
    PAGE_INTRO,
    PAGE_CONNECTION,
    PAGE_FILES,
    PAGE_END
};

InitialSettingsWizard::InitialSettingsWizard(QWidget *p)
    : QWizard(p)
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
    #if defined Q_OS_WIN || defined Q_OS_MAC
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
    storeBackdropsInMpdDir->setChecked(Settings::self()->storeBackdropsInMpdDir());
    #ifdef ENABLE_KDE_SUPPORT
    introPage->setBackground(Icon("cantata"));
    #else
    introPage->setBackground(Icons::self()->appIcon);
    #endif
    connectionPage->setBackground(Icons::self()->audioFileIcon);
    filesPage->setBackground(Icons::self()->filesIcon);
    finishedPage->setBackground(Icon("dialog-ok"));

    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    introStack->setCurrentIndex(MPDUser::self()->isSupported() ? 1 : 0);
    basic->setChecked(false);
    advanced->setChecked(true);
    #else
    introStack->setCurrentIndex(0);
    basic->setChecked(true);
    advanced->setChecked(false);
    #endif

    #ifndef Q_OS_WIN
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
    setMinimumSize(sz);
    #endif
    httpNote->setOn(true);
}

InitialSettingsWizard::~InitialSettingsWizard()
{
}

MPDConnectionDetails InitialSettingsWizard::getDetails()
{
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    if (basic->isChecked()) {
        MPDUser::self()->setMusicFolder(basicDir->text().trimmed());
        return MPDUser::self()->details(true);
    }
    #endif
    MPDConnectionDetails det;
    det.hostname=host->text().trimmed();
    det.port=port->value();
    det.password=password->text();
    det.dir=dir->text().trimmed();
    det.setDirReadable();
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
            #if QT_VERSION > 0x050000
            QString dir=QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
            #else
            QString dir=QDesktopServices::storageLocation(QDesktopServices::MusicLocation);
            #endif
            if (dir.isEmpty()) {
                QString dir=QDir::homePath()+"/Music";
                dir=dir.replace("//", "/");
            }
            basicDir->setText(dir);
        }
        controlNextButton();
        return;
    }
    if (PAGE_FILES==p) {
        if (dir->text().trimmed().startsWith(QLatin1String("http:/"))) {
            storeCoversInMpdDir->setChecked(false);
            storeLyricsInMpdDir->setChecked(false);
            storeBackdropsInMpdDir->setChecked(false);
            httpNote->setVisible(true);
        } else {
            storeCoversInMpdDir->setChecked(Settings::self()->storeCoversInMpdDir());
            storeLyricsInMpdDir->setChecked(Settings::self()->storeLyricsInMpdDir());
            storeBackdropsInMpdDir->setChecked(Settings::self()->storeBackdropsInMpdDir());
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

    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    if (basic->isChecked()) {
        Settings::self()->saveCurrentConnection(MPDUser::constName);
        Settings::self()->saveStopOnExit(true);
        emit setDetails(MPDUser::self()->details());
    } else {
        MPDUser::self()->cleanup();
    }
    #endif
    Settings::self()->save();
    QDialog::accept();
}

void InitialSettingsWizard::reject()
{
    // Clear version number - so that wizard is shown next time Cantata is started.
    Settings::self()->clearVersion();
    QDialog::reject();
}
