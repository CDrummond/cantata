/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "preferencesdialog.h"
#include "mainwindow.h"
#include "settings.h"

#ifdef ENABLE_KDE_SUPPORT
PreferencesDialog::PreferencesDialog(QWidget *parent)
    : KDialog(parent),
#else
PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent),
#endif
      xfade(MPDStatus::self()->xfade())
{
#ifdef ENABLE_KDE_SUPPORT
    QWidget *widget = new QWidget(this);
    setupUi(widget);

    setMainWidget(widget);

    setButtons(KDialog::Ok | KDialog::Apply | KDialog::Cancel);
    setCaption(i18n("Configure"));
#else
    setupUi(this);
#endif

#ifndef ENABLE_KDE_SUPPORT
    connect(buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonPressed(QAbstractButton *)));
#endif
    loadSettings();

    crossfading->setValue(MPDStatus::self()->xfade());
    resize(380, 180);
}

void PreferencesDialog::setCrossfading(int crossfade)
{
    xfade = crossfade;
    crossfading->setValue(xfade);
}

void PreferencesDialog::loadSettings()
{
    hostLineEdit->setText(Settings::self()->connectionHost());
    portSpinBox->setValue(Settings::self()->connectionPort());
    passwordLineEdit->setText(Settings::self()->connectionPasswd());
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    systemTrayPopup->setChecked(Settings::self()->showPopups());
//     systemTrayPopup->setEnabled(systemTrayCheckBox->isChecked());
    mpdDir->setText(Settings::self()->mpdDir());
    crossfading->setValue(xfade);
}

void PreferencesDialog::writeSettings()
{
    Settings::self()->saveConnectionHost(hostLineEdit->text());
    Settings::self()->saveConnectionPort(portSpinBox->value());
    Settings::self()->saveConnectionPasswd(passwordLineEdit->text());
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    Settings::self()->saveMpdDir(mpdDir->text());
    if (crossfading->value() != xfade)
        emit crossfadingChanged(crossfading->value());
    emit systemTraySet(systemTrayCheckBox->isChecked());
}

#ifdef ENABLE_KDE_SUPPORT
void PreferencesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case KDialog::Ok:
    case KDialog::Apply:
        writeSettings();
        break;
    case KDialog::Cancel:
        reject();
        break;
    default:
        // unhandled button
        break;
    }

    if (button == KDialog::Ok) {
        accept();
    }

    KDialog::slotButtonClicked(button);
}
#endif

/**
 * TODO:MERGE it all with the kde version... (submission part..)
 */
#ifndef ENABLE_KDE_SUPPORT
void PreferencesDialog::buttonPressed(QAbstractButton *button)
{
    switch (buttonBox->buttonRole(button)) {
    case QDialogButtonBox::AcceptRole:
    case QDialogButtonBox::ApplyRole:
        writeSettings();
        break;
    case QDialogButtonBox::RejectRole:
        break;
    default:
        // unhandled button
        break;
    }
}
#endif
