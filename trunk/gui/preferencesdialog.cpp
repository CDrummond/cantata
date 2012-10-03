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

#include "preferencesdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "icons.h"
#include "interfacesettings.h"
#include "externalsettings.h"
#include "serversettings.h"
#include "serverplaybacksettings.h"
#include "playbacksettings.h"
#ifdef TAGLIB_FOUND
#include "httpserversettings.h"
#endif
#include "lyricsettings.h"
#include "lyricspage.h"
#include "localize.h"
#include "mpdconnection.h"
#include "pagewidget.h"
#ifndef ENABLE_KDE_SUPPORT
#include "proxysettings.h"
#endif

PreferencesDialog::PreferencesDialog(QWidget *parent, LyricsPage *lp)
    : Dialog(parent)
{
    setButtons(Ok|Apply|Cancel);

    PageWidget *widget = new PageWidget(this);
    server = new ServerSettings(widget);
    serverplayback = new ServerPlaybackSettings(widget);
    playback = new PlaybackSettings(widget);
    interface = new InterfaceSettings(widget);
    ext = new ExternalSettings(widget);
    #ifdef TAGLIB_FOUND
    http = new HttpServerSettings(widget);
    #endif
    lyrics = new LyricSettings(widget);
    server->load();
    serverplayback->load();
    playback->load();
    interface->load();
    ext->load();
    #ifdef TAGLIB_FOUND
    http->load();
    #endif
    const QList<UltimateLyricsProvider *> &lprov=lp->getProviders();
    lyrics->Load(lprov);
    widget->addPage(server, i18n("Connection"), Icon("network-server"), i18n("Connection Settings"));
    widget->addPage(serverplayback, i18n("Output"), Icons::speakerIcon, i18n("Output Settings"));
    widget->addPage(playback, i18n("Playback"), Icon("media-playback-start"), i18n("Playback Settings"));
    widget->addPage(interface, i18n("Interface"), Icon("preferences-other"), i18n("Interface Settings"));
    widget->addPage(ext, i18n("External"), Icon("video-display"), i18n("External Settings"));
    #ifdef TAGLIB_FOUND
    widget->addPage(http, i18n("HTTP Server"), Icon("network-wired"), i18n("HTTP Server Settings"));
    #endif
    widget->addPage(lyrics, i18n("Lyrics"), Icons::lyricsIcon, i18n("Lyrics Settings"));

    #ifndef ENABLE_KDE_SUPPORT
    proxy = new ProxySettings(widget);
    proxy->load();
    widget->addPage(proxy, i18nc("Qt-only", "Proxy"), Icon("preferences-system-network"), i18nc("Qt-only", "Proxy Settings"));
    #endif
    widget->allPagesAdded();
    setCaption(i18n("Configure"));
    setMainWidget(widget);
    connect(server, SIGNAL(connectTo(const MPDConnectionDetails &)), SIGNAL(connectTo(const MPDConnectionDetails &)));
    connect(server, SIGNAL(disconnectFromMpd()), MPDConnection::self(), SLOT(disconnectMpd()));
}

void PreferencesDialog::writeSettings()
{
    // *Must* save server settings first, so that MPD settings go to the correct instance!
    server->save();
    serverplayback->save();
    playback->save();
    interface->save();
    ext->save();
    #ifdef TAGLIB_FOUND
    http->save();
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    proxy->save();
    #endif
    Settings::self()->saveLyricProviders(lyrics->EnabledProviders());
    Settings::self()->save();
    emit settingsSaved();
}

void PreferencesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
    case Apply:
        writeSettings();
        break;
    case Cancel:
        reject();
        break;
    default:
        break;
    }

    if (Ok==button) {
        accept();
    }

    Dialog::slotButtonClicked(button);
}

