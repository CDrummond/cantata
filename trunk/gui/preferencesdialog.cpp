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

#include "preferencesdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "icons.h"
#include "interfacesettings.h"
#include "serversettings.h"
#include "serverplaybacksettings.h"
#include "playbacksettings.h"
#include "filesettings.h"
#ifdef TAGLIB_FOUND
#include "httpserversettings.h"
#endif
#include "lyricsettings.h"
#include "cachesettings.h"
#include "localize.h"
#include "mpdconnection.h"
#include "pagewidget.h"
#ifndef ENABLE_KDE_SUPPORT
#include "proxysettings.h"
#include "shortcutssettingspage.h"
#include "actioncollection.h"
#endif
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "audiocdsettings.h"
#endif

static int iCount=0;

int PreferencesDialog::instanceCount()
{
    return iCount;
}

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : Dialog(parent, "PreferencesDialog")
{
    iCount++;
    setButtons(Ok|Apply|Cancel);

    PageWidget *widget = new PageWidget(this);
    server = new ServerSettings(widget);
    serverplayback = new ServerPlaybackSettings(widget);
    playback = new PlaybackSettings(widget);
    files = new FileSettings(widget);
    interface = new InterfaceSettings(widget);
    lyrics = new LyricSettings(widget);
    cache = new CacheSettings(widget);
    server->load();
    serverplayback->load();
    playback->load();
    files->load();
    interface->load();
    lyrics->load();
    widget->addPage(server, i18n("Connection"), Icons::libraryIcon, i18n("Connection Settings"));
    widget->addPage(serverplayback, i18n("Output"), Icons::speakerIcon, i18n("Output Settings"));
    widget->addPage(playback, i18n("Playback"), Icon("media-playback-start"), i18n("Playback Settings"));
    widget->addPage(files, i18n("Files"), Icons::filesIcon, i18n("File Settings"));
    widget->addPage(interface, i18n("Interface"), Icon("preferences-other"), i18n("Interface Settings"));
    #ifdef TAGLIB_FOUND
    http = new HttpServerSettings(widget);
    http->load();
    widget->addPage(http, i18n("HTTP Server"), Icon("network-server"), i18n("HTTP Server Settings"));
    #endif
    widget->addPage(lyrics, i18n("Lyrics"), Icons::lyricsIcon, i18n("Lyrics Settings"));
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd = new AudioCdSettings(widget);
    audiocd->load();
    widget->addPage(audiocd, i18n("Audio CD"), Icons::albumIcon, i18n("Audio CD Settings"));
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    proxy = new ProxySettings(widget);
    proxy->load();
    widget->addPage(proxy, i18nc("Qt-only", "Proxy"), Icon("preferences-system-network"), i18nc("Qt-only", "Proxy Settings"));
    QHash<QString, ActionCollection *> map;
    map.insert("Cantata", ActionCollection::get());
    shortcuts = new ShortcutsSettingsPage(map, widget);
    widget->addPage(shortcuts, i18nc("Qt-only", "Shortcuts"), Icons::shortcutsIcon, i18nc("Qt-only", "Keyboard Shortcut Settings"));
    #endif
    widget->addPage(cache, i18n("Cache"), Icon("folder"), i18n("Cached Items"));
    widget->allPagesAdded();
    setCaption(i18n("Configure"));
    setMainWidget(widget);
    setAttribute(Qt::WA_DeleteOnClose);
    connect(server, SIGNAL(connectTo(const MPDConnectionDetails &)), SIGNAL(connectTo(const MPDConnectionDetails &)));
    connect(server, SIGNAL(disconnectFromMpd()), MPDConnection::self(), SLOT(disconnectMpd()));
    connect(files, SIGNAL(reloadStreams()), SIGNAL(reloadStreams()));
    widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

PreferencesDialog::~PreferencesDialog()
{
    iCount--;
}

void PreferencesDialog::writeSettings()
{
    // *Must* save server settings first, so that MPD settings go to the correct instance!
    server->save();
    serverplayback->save();
    playback->save();
    files->save();
    interface->save();
    #ifdef TAGLIB_FOUND
    http->save();
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    proxy->save();
    shortcuts->save();
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd->save();
    #endif
    lyrics->save();
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
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }

    if (Ok==button) {
        accept();
    }

    Dialog::slotButtonClicked(button);
}
