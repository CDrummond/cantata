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
#include "interfacesettings.h"
#include "playbacksettings.h"
#include "outputsettings.h"
#include "serversettings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KPageWidget>
#include <KDE/KIcon>
#else
#include <QtGui/QTabWidget>
#endif

#ifdef ENABLE_KDE_SUPPORT
PreferencesDialog::PreferencesDialog(QWidget *parent)
    : KDialog(parent)
#else
PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
#endif
{
#ifdef ENABLE_KDE_SUPPORT
    KPageWidget *widget = new KPageWidget(this);
#else
    QTabWidget *widget = new QTabWidget(this);
#endif

    server = new ServerSettings(widget);
    playback = new PlaybackSettings(widget);
    output = new OutputSettings(widget);
    interface = new InterfaceSettings(widget);
    server->load();
    playback->load();
    output->load();
    interface->load();
#ifdef ENABLE_KDE_SUPPORT
    KPageWidgetItem *page=widget->addPage(server, i18n("Server"));
    page->setHeader(i18n("MPD Backend Settings"));
    page->setIcon(KIcon("server-database"));
    page=widget->addPage(playback, i18n("Playback"));
    page->setHeader(i18n("Playback Settings"));
    page->setIcon(KIcon("media-playback-start"));
    page=widget->addPage(output, i18n("Output"));
    page->setHeader(i18n("Control Active Outputs"));
    page->setIcon(KIcon("speaker"));
    page=widget->addPage(interface, i18n("Interface"));
    page->setHeader(i18n("Interface Settings"));
    page->setIcon(KIcon("preferences-desktop-color"));

    setMainWidget(widget);

    setButtons(KDialog::Ok | KDialog::Apply | KDialog::Cancel);
    setCaption(i18n("Configure"));
#else
    widget->addTab(server, QIcon::fromTheme("server-database"), tr("Server"));
    widget->addTab(playback, QIcon::fromTheme("media-playback-start"), tr("Playback"));
    widget->addTab(output, QIcon::fromTheme("speaker"), tr("Output"));
    widget->addTab(interface, QIcon::fromTheme("preferences-desktop-color"), tr("Interface"));
    setCaption(tr("Configure"));
    connect(buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonPressed(QAbstractButton *)));
#endif
    resize(450, 300);
}

void PreferencesDialog::writeSettings()
{
    server->save();
    playback->save();
    output->save();
    interface->save();
    emit systemTraySet(interface->sysTrayEnabled());
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
