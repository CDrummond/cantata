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

#include "preferencesdialog.h"
#include "settings.h"
#include "widgets/icons.h"
#include "interfacesettings.h"
#include "serversettings.h"
#include "playbacksettings.h"
#include "filesettings.h"
#include "context/contextsettings.h"
#include "cachesettings.h"
#include "customactionssettings.h"
#include "support/localize.h"
#include "mpd-interface/mpdconnection.h"
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
    server = new ServerSettings(this);
    playback = new PlaybackSettings(this);
    files = new FileSettings(this);
    interface = new InterfaceSettings(this);
    context = new ContextSettings(this);
    cache = new CacheSettings(this);
    scrobbling = new ScrobblingSettings(this);
    custom = new CustomActionsSettings(this);
    server->load();
    playback->load();
    files->load();
    interface->load();
    context->load();
    scrobbling->load();
    custom->load();
    addPage(QLatin1String("collection"), server, i18n("Collection"), Icons::self()->audioFileIcon, i18n("Collection Settings"));
    addPage(QLatin1String("playback"), playback, i18n("Playback"), Icons::self()->speakerIcon, i18n("Playback Settings"));
    addPage(QLatin1String("files"), files, i18n("Downloaded Files"), Icons::self()->filesIcon, i18n("Downloaded Files Settings"));
    addPage(QLatin1String("interface"), interface, i18n("Interface"), Icon("preferences-other"), i18n("Interface Settings"));
    addPage(QLatin1String("info"), context, i18n("Info"), Icons::self()->contextIcon, i18n("Info View Settings"));
    addPage(QLatin1String("scrobbling"), scrobbling, i18n("Scrobbling"), Icons::self()->lastFmIcon, i18n("Scrobbling Settings"));
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd = new AudioCdSettings(0);
    audiocd->load();
    addPage(QLatin1String("cd"), audiocd, i18n("Audio CD"), Icons::self()->albumIcon(32), i18n("Audio CD Settings"));
    #endif
    #ifdef ENABLE_PROXY_CONFIG
    proxy = new ProxySettings(0);
    proxy->load();
    addPage(QLatin1String("proxy"), proxy, i18n("Proxy"), Icon("preferences-system-network"), i18nc("Qt-only", "Proxy Settings"));
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    shortcuts = new ShortcutsSettingsPage(0);
    addPage(QLatin1String("shortcuts"), shortcuts, i18nc("Qt-only", "Shortcuts"), Icon(QStringList() << "preferences-desktop-keyboard" << "keyboard"),
            i18nc("Qt-only", "Keyboard Shortcut Settings"));
    shortcuts->load();
    #endif
    addPage(QLatin1String("cache"), cache, i18n("Cache"), Icon(QStringList() << "folder-temp" << "folder"), i18n("Cached Items"));
    addPage(QLatin1String("custom"), custom, i18n("Custom Actions"), Icon(QStringList() << "fork" << "gtk-execute"), i18n("Custom Actions"));
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
    custom->save();
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
