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
#include "mainwindow.h"
#include "settings.h"
#include "icons.h"
#include "interfacesettings.h"
#ifdef ENABLE_STREAMS
#include "streamssettings.h"
#endif
#ifdef ENABLE_ONLINE_SERVICES
#include "onlinesettings.h"
#endif
#include "serversettings.h"
#include "playbacksettings.h"
#include "filesettings.h"
#ifdef ENABLE_HTTP_SERVER
#include "httpserversettings.h"
#endif
#include "contextsettings.h"
#include "cachesettings.h"
#include "localize.h"
#include "mpdconnection.h"
#ifdef ENABLE_PROXY_CONFIG
#include "proxysettings.h"
#endif
#ifndef ENABLE_KDE_SUPPORT
#include "shortcutssettingspage.h"
#endif
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "audiocdsettings.h"
#endif
#ifndef ENABLE_KDE_SUPPORT
#include "shortcutssettingspage.h"
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

    pageWidget = new PageWidget(this);
    server = new ServerSettings(0);
    playback = new PlaybackSettings(0);
    files = new FileSettings(0);
    interface = new InterfaceSettings(0);
    context = new ContextSettings(0);
    cache = new CacheSettings(0);
    server->load();
    playback->load();
    files->load();
    interface->load();
    context->load();
    pages.insert(QLatin1String("collection"), pageWidget->addPage(server, i18n("Collection"), Icons::self()->libraryIcon, i18n("Collection Settings")));
    pageWidget->addPage(playback, i18n("Playback"), Icon("media-playback-start"), i18n("Playback Settings"));
    pageWidget->addPage(files, i18n("Files"), Icons::self()->filesIcon, i18n("File Settings"));
    pages.insert(QLatin1String("interface"), pageWidget->addPage(interface, i18n("Interface"), Icon("preferences-other"), i18n("Interface Settings")));
    #ifdef ENABLE_STREAMS
    streams = new StreamsSettings(0);
    pages.insert(QLatin1String("streams"), pageWidget->addPage(streams, i18n("Streams"), Icons::self()->radioStreamIcon, i18n("Streams Settings")));
    streams->load();
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    online = new OnlineSettings(0);
    pages.insert(QLatin1String("online"), pageWidget->addPage(online, i18n("Online"), Icon("applications-internet"), i18n("Online Providers")));
    online->load();
    #endif
    pageWidget->addPage(context, i18n("Context"), Icons::self()->contextIcon, i18n("Context View Settings"));
    #ifdef ENABLE_HTTP_SERVER
    http = new HttpServerSettings(0);
    if (http->haveMultipleInterfaces()) {
        http->load();
        Icon icon("network-server");
        if (icon.isNull()) {
            icon=Icons::self()->audioFileIcon;
        }
        pageWidget->addPage(http, i18n("HTTP Server"), icon, i18n("HTTP Server Settings"));
    } else {
        http->deleteLater();
        http=0;
    }
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd = new AudioCdSettings(0);
    audiocd->load();
    pageWidget->addPage(audiocd, i18n("Audio CD"), Icon("media-optical"), i18n("Audio CD Settings"));
    #endif
    #ifdef ENABLE_PROXY_CONFIG
    proxy = new ProxySettings(0);
    proxy->load();
    pageWidget->addPage(proxy, i18n("Proxy"), Icon("preferences-system-network"), i18nc("Qt-only", "Proxy Settings"));
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    shortcuts = new ShortcutsSettingsPage(pageWidget);
    pageWidget->addPage(shortcuts, i18nc("Qt-only", "Shortcuts"), Icons::self()->shortcutsIcon, i18nc("Qt-only", "Keyboard Shortcut Settings"));
    shortcuts->load();
    #endif
    pageWidget->addPage(cache, i18n("Cache"), Icon("folder"), i18n("Cached Items"));
    setCaption(i18n("Configure"));
    setMainWidget(pageWidget);
    setAttribute(Qt::WA_DeleteOnClose);
    int h=sizeHint().height();
    setMinimumHeight(h);
    setMinimumWidth(h*1.1);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

PreferencesDialog::~PreferencesDialog()
{
    iCount--;
}

void PreferencesDialog::showPage(const QString &page)
{
    QStringList parts=page.split(QLatin1Char(':'));
    if (pages.contains(parts.at(0))) {
        pageWidget->setCurrentPage(pages[parts.at(0)]);
        if (parts.count()>1 && qobject_cast<InterfaceSettings *>(pages[parts.at(0)]->widget())) {
            static_cast<InterfaceSettings *>(pages[parts.at(0)]->widget())->showPage(parts.at(1));
        }
    }
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
        server->cancel();
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
