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
    introPage->setBackground(Icons::appIcon);
    #endif
    connectionPage->setBackground(Icons::libraryIcon);
    filesPage->setBackground(Icons::filesIcon);
    finishedPage->setBackground(Icon("dialog-ok"));

    // Adjust size for high-DPI setups...
    if (fontMetrics().height()>20) {
        QSize sz=size();
        qWarning() << sz.width() << sz.height();
        foreach (int id, pageIds()) {
            QWizardPage *p=page(id);
            p->adjustSize();
            QSize ps=p->size();
            if (ps.width()>sz.width()) {
                sz.setWidth(ps.width());
            }
            if (ps.height()>sz.height()) {
                sz.setHeight(ps.height());
            }
        }

        // For some reason, the above seems to give us very tall dialogs. If this is the case,
        // then try to make it square-ish again...
        if (sz.height()>(sz.width()+200)) {
            sz+=QSize(150, -150);
        }
        resize(sz);
    }
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
    bool isOk=MPDConnection::self()->isConnected();

    if (isOk) {
       MPDConnectionDetails det=getDetails();
       MPDConnectionDetails mpdDet=MPDConnection::self()->getDetails();
       isOk=det.hostname==mpdDet.hostname && (det.isLocal() || det.port==mpdDet.port);
    }

    button(NextButton)->setEnabled(isOk);
}

void InitialSettingsWizard::accept()
{
    Settings::self()->saveConnectionDetails(getDetails());
    Settings::self()->saveStoreCoversInMpdDir(storeCoversInMpdDir->isChecked());
    Settings::self()->saveStoreLyricsInMpdDir(storeLyricsInMpdDir->isChecked());
    Settings::self()->saveStoreStreamsInMpdDir(storeStreamsInMpdDir->isChecked());
    Settings::self()->save(true);
    QDialog::accept();
}
