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

#include "devicepropertiesdialog.h"
#include "filenameschemedialog.h"
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KMessageBox>

DevicePropertiesDialog::DevicePropertiesDialog(QWidget *parent)
    : KDialog(parent)
    , schemeDlg(0)
{
    setButtons(KDialog::Ok|KDialog::Cancel);
    setCaption(i18n("Device Properties"));
    setWindowModality(Qt::WindowModal);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    configFilename->setIcon(QIcon::fromTheme("configure"));
    connect(configFilename, SIGNAL(pressed()), SLOT(configureFilenameScheme()));
    connect(filenameScheme, SIGNAL(textChanged(const QString &)), this, SLOT(enableOkButton()));
    connect(vfatSafe, SIGNAL(stateChanged(int)), this, SLOT(enableOkButton()));
    connect(asciiOnly, SIGNAL(stateChanged(int)), this, SLOT(enableOkButton()));
    connect(ignoreThe, SIGNAL(stateChanged(int)), this, SLOT(enableOkButton()));
    connect(musicFolder, SIGNAL(textChanged(const QString &)), this, SLOT(enableOkButton()));
}

void DevicePropertiesDialog::show(const QString &path, const Device::NameOptions &opts, bool pathEditable)
{
    filenameScheme->setText(opts.scheme);
    vfatSafe->setChecked(opts.vfatSafe);
    asciiOnly->setChecked(opts.asciiOnly);
    ignoreThe->setChecked(opts.ignoreThe);
    replaceSpaces->setChecked(opts.replaceSpaces);
    musicFolder->setText(path);
    musicFolder->setEnabled(pathEditable);
    origOpts=opts;
    origMusicFolder=path;
    KDialog::show();
    enableButtonOk(false);
}

void DevicePropertiesDialog::enableOkButton()
{
    Device::NameOptions opts=settings();
    enableButtonOk(musicFolder->text().trimmed()!=origMusicFolder || (!opts.scheme.isEmpty() && opts!=origOpts));
}

void DevicePropertiesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case KDialog::Ok:
        emit updatedSettings(musicFolder->text().trimmed(), settings());
        break;
    case KDialog::Cancel:
        reject();
        break;
    default:
        break;
    }

    if (KDialog::Ok==button) {
        accept();
    }

    KDialog::slotButtonClicked(button);
}

void DevicePropertiesDialog::configureFilenameScheme()
{
    if (!schemeDlg) {
        schemeDlg=new FilenameSchemeDialog(this);
        connect(schemeDlg, SIGNAL(scheme(const QString &)), filenameScheme, SLOT(setText(const QString &)));
    }
    schemeDlg->show(settings());
}

Device::NameOptions DevicePropertiesDialog::settings()
{
    Device::NameOptions opts;
    opts.scheme=filenameScheme->text().trimmed();
    opts.vfatSafe=vfatSafe->isChecked();
    opts.asciiOnly=asciiOnly->isChecked();
    opts.ignoreThe=ignoreThe->isChecked();
    opts.replaceSpaces=replaceSpaces->isChecked();
    return opts;
}
