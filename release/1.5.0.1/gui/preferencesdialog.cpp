/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "settings.h"
#include "widgets/icons.h"
#include "interfacesettings.h"
#ifdef ENABLE_STREAMS
#include "streams/streamssettings.h"
#endif
#ifdef ENABLE_ONLINE_SERVICES
#include "online/onlinesettings.h"
#endif
#include "serversettings.h"
#include "playbacksettings.h"
#include "filesettings.h"
#ifdef ENABLE_HTTP_SERVER
#include "http/httpserversettings.h"
#endif
#include "context/contextsettings.h"
#include "cachesettings.h"
#include "support/localize.h"
#include "mpd/mpdconnection.h"
#ifdef ENABLE_PROXY_CONFIG
#include "network/proxysettings.h"
#endif
#ifndef ENABLE_KDE_SUPPORT
#include "shortcutssettingspage.h"
#endif
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "devices/audiocdsettings.h"
#endif
#ifndef ENABLE_KDE_SUPPORT
#include "shortcutssettingspage.h"
#endif
#include "scrobbling/scrobblingsettings.h"
#include <QDesktopWidget>
#include <QTimer>

static int iCount=0;

int PreferencesDialog::instanceCount()
{
    return iCount;
}

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : ConfigDialog(parent, "PreferencesDialog")
{
    iCount++;
    server = new ServerSettings(0);
    playback = new PlaybackSettings(0);
    files = new FileSettings(0);
    interface = new InterfaceSettings(0);
    context = new ContextSettings(0);
    cache = new CacheSettings(0);
    scrobbling = new ScrobblingSettings(0);
    server->load();
    playback->load();
    files->load();
    interface->load();
    context->load();
    scrobbling->load();
    addPage(QLatin1String("collection"), server, i18n("Collection"), Icons::self()->audioFileIcon, i18n("Collection Settings"));
    addPage(QLatin1String("playback"), playback, i18n("Playback"), Icon("media-playback-start"), i18n("Playback Settings"));
    addPage(QLatin1String("files"), files, i18n("Files"), Icons::self()->filesIcon, i18n("File Settings"));
    addPage(QLatin1String("interface"), interface, i18n("Interface"), Icon("preferences-other"), i18n("Interface Settings"));
    #ifdef ENABLE_STREAMS
    streams = new StreamsSettings(0);
    addPage(QLatin1String("streams"), streams, i18n("Streams"), Icons::self()->radioStreamIcon, i18n("Streams Settings"));
    streams->load();
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    online = new OnlineSettings(0);
    addPage(QLatin1String("online"), online, i18n("Online"), Icon("applications-internet"), i18n("Online Providers"));
    online->load();
    #endif
    addPage(QLatin1String("context"), context, i18n("Context"), Icons::self()->contextIcon, i18n("Context View Settings"));
    addPage(QLatin1String("scrobbling"), scrobbling, i18n("Scrobbling"), Icons::self()->lastFmIcon, i18n("Scrobbling Settings"));
    #ifdef ENABLE_HTTP_SERVER
    http = new HttpServerSettings(0);
    if (http->haveMultipleInterfaces()) {
        http->load();
        Icon icon("network-server");
        if (icon.isNull()) {
            icon=Icons::self()->streamIcon;
        }
        addPage(QLatin1String("http"), http, i18n("HTTP Server"), icon, i18n("HTTP Server Settings"));
    } else {
        http->deleteLater();
        http=0;
    }
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd = new AudioCdSettings(0);
    audiocd->load();
    addPage(QLatin1String("cd"), audiocd, i18n("Audio CD"), Icon("media-optical"), i18n("Audio CD Settings"));
    #endif
    #ifdef ENABLE_PROXY_CONFIG
    proxy = new ProxySettings(0);
    proxy->load();
    addPage(QLatin1String("proxy"), proxy, i18n("Proxy"), Icon("preferences-system-network"), i18nc("Qt-only", "Proxy Settings"));
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    shortcuts = new ShortcutsSettingsPage(0);
    addPage(QLatin1String("shortcuts"), shortcuts, i18nc("Qt-only", "Shortcuts"), Icons::self()->shortcutsIcon, i18nc("Qt-only", "Keyboard Shortcut Settings"));
    shortcuts->load();
    #endif
    addPage(QLatin1String("cache"), cache, i18n("Cache"), Icons::self()->folderIcon, i18n("Cached Items"));
    #ifdef Q_OS_MAC
    setCaption(i18n("Cantata Preferences"));
    #else
    setCaption(i18n("Configure"));
    #endif
    setAttribute(Qt::WA_DeleteOnClose);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setCurrentPage(QLatin1String("collection"));
}

PreferencesDialog::~PreferencesDialog()
{
    iCount--;
}

void PreferencesDialog::showPage(const QString &page)
{
    QStringList parts=page.split(QLatin1Char(':'));
    if (setCurrentPage(parts.at(0))) {
        if (parts.count()>1) {
            QWidget *cur=getPage(parts.at(0));
            if (qobject_cast<InterfaceSettings *>(cur)) {
                static_cast<InterfaceSettings *>(cur)->showPage(parts.at(1));
            }
        }
    }
    Utils::raiseWindow(this);
}

void PreferencesDialog::writeSettings()
{
    // *Must* save server settings first, so that MPD settings go to the correct instance!
    server->save();
    playback->save();
    files->save();
    interface->save();
    #ifdef ENABLE_STREAMS
    streams->save();
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    online->save();
    #endif
    #ifdef ENABLE_HTTP_SERVER
    if (http) {
        http->save();
    }
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    #ifdef ENABLE_PROXY_CONFIG
    proxy->save();
    #endif
    shortcuts->save();
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd->save();
    #endif
    context->save();
    scrobbling->save();
    Settings::self()->save();
    emit settingsSaved();
}

void PreferencesDialog::save()
{
    writeSettings();
}

void PreferencesDialog::cancel()
{
    server->cancel();
}
