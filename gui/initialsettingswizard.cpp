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

#include "initialsettingswizard.h"
#include "support/messagebox.h"
#include "settings.h"
#include "support/utils.h"
#include "support/icon.h"
#include "support/monoicon.h"
#include "widgets/icons.h"
#include "models/mpdlibrarymodel.h"
#ifdef ENABLE_SIMPLE_MPD_SUPPORT
#include "mpd-interface/mpduser.h"
#endif
#ifdef AVAHI_FOUND
#include "findmpddialog.h"
#endif
#include <QDir>
#include <QStandardPaths>
#include <QTimer>

enum Pages {
    PAGE_INTRO,
    PAGE_CONNECTION,
    PAGE_COVERS,
    PAGE_END
};

InitialSettingsWizard::InitialSettingsWizard(QWidget *p)
    : QWizard(p)
{
    setupUi(this);
    connect(this, SIGNAL(currentIdChanged(int)), SLOT(pageChanged(int)));
    connect(this, SIGNAL(setDetails(MPDConnectionDetails)), MPDConnection::self(), SLOT(setDetails(MPDConnectionDetails)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(QString, bool)), SLOT(showError(QString)));
    connect(MpdLibraryModel::self(), SIGNAL(error(QString)), SLOT(dbError(QString)));
    connect(connectButton, SIGNAL(clicked(bool)), SLOT(connectToMpd()));
    connect(basicDir, SIGNAL(textChanged(QString)), SLOT(controlNextButton()));
    MPDConnection::self()->start();
    statusLabel->setText(tr("Not Connected"));

    MPDConnectionDetails det=Settings::self()->connectionDetails();
    host->setText(det.hostname);
    port->setValue(det.port);
    password->setText(det.password);
    dir->setText(det.dir);
    introPage->setBackground(Icons::self()->appIcon);
    connectionPage->setBackground(Icons::self()->audioListIcon);
    coversPage->setBackground(Icons::self()->albumMonoIcon);
    finishedPage->setBackground(MonoIcon::icon(FontAwesome::check, Utils::monoIconColor()));
    fetchCovers->setChecked(Settings::self()->fetchCovers());
    storeCoversInMpdDir->setChecked(Settings::self()->storeCoversInMpdDir());

    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    introStack->setCurrentIndex(MPDUser::self()->isSupported() ? 1 : 0);
    basic->setChecked(false);
    advanced->setChecked(true);
    #else
    introStack->setCurrentIndex(0);
    basic->setChecked(false);
    advanced->setChecked(true);
    #endif

    #ifndef Q_OS_WIN
    ensurePolished();
    QSize sz=size()+QSize(64, 32);
    // Adjust size for high-DPI setups...
    bool highDpi=fontMetrics().height()>20;
    if (highDpi) {
        for (int id: pageIds()) {
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

    #ifdef AVAHI_FOUND
    discoveryButton = new QPushButton(tr("Discover..."), this);
    hostLayout->insertWidget(hostLayout->count(), discoveryButton);
    connect(discoveryButton, &QPushButton::clicked, this, &InitialSettingsWizard::detectMPDs);
    #endif
}

InitialSettingsWizard::~InitialSettingsWizard()
{
}

#ifdef AVAHI_FOUND
void InitialSettingsWizard::adoptServerSettings(QString ip, QString p)
{
    host->setText(ip);
    port->setValue(p.toInt());
}

void InitialSettingsWizard::detectMPDs()
{
    FindMpdDialog findMpdDlg(this);
    QObject::connect(&findMpdDlg, &FindMpdDialog::serverChosen, this, &InitialSettingsWizard::adoptServerSettings);
    findMpdDlg.exec();
}
#endif

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
    statusLabel->setText(c ? tr("Connection Established") : tr("Connection Failed"));
    if (PAGE_CONNECTION==currentId()) {
        controlNextButton();
    }
}

void InitialSettingsWizard::showError(const QString &message)
{
    MessageBox::error(this, message);
}

void InitialSettingsWizard::dbError(const QString &message)
{
    MessageBox::error(this, message+QLatin1String("<br/><br/>")+tr("Cantata will now terminate"));
    reject();
}

void InitialSettingsWizard::pageChanged(int p)
{
    if (PAGE_CONNECTION==p) {
        connectionStack->setCurrentIndex(basic->isChecked() ? 1 : 0);
        if (basic->isChecked() && basicDir->text().isEmpty()) {
            QString dir=QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
            if (dir.isEmpty()) {
                QString dir=QDir::homePath()+"/Music";
                dir=dir.replace("//", "/");
            }
            basicDir->setText(dir);
        }
        controlNextButton();
        return;
    }
    if (PAGE_COVERS==p) {
        if (dir->text().trimmed().startsWith(QLatin1String("http:/"))) {
            storeCoversInMpdDir->setChecked(false);
            fetchCovers->setChecked(true);
        } else {
            storeCoversInMpdDir->setChecked(Settings::self()->storeCoversInMpdDir());
            fetchCovers->setChecked(Settings::self()->fetchCovers());
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
    Settings::self()->saveFetchCovers(fetchCovers->isChecked());
    Settings::self()->saveStoreCoversInMpdDir(storeCoversInMpdDir->isChecked());
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
    ThreadCleaner::self()->stopAll();
    QTimer::singleShot(0, qApp, SLOT(quit()));
    QDialog::reject();
}

#include "moc_initialsettingswizard.cpp"
