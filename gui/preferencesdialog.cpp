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

#include "preferencesdialog.h"
#include "settings.h"
#include "widgets/icons.h"
#include "interfacesettings.h"
#include "serversettings.h"
#include "playbacksettings.h"
#include "context/contextsettings.h"
#include "cachesettings.h"
#include "customactionssettings.h"
#include "mpd-interface/mpdconnection.h"
#ifdef ENABLE_PROXY_CONFIG
#include "network/proxysettings.h"
#endif
#include "shortcutssettingspage.h"
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "devices/audiocdsettings.h"
#endif
#include "shortcutssettingspage.h"
#include "scrobbling/scrobblingsettings.h"
#include "apikeyssettings.h"
#include "support/monoicon.h"
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
    interface = new InterfaceSettings(this);
    context = new ContextSettings(this);
    cache = new CacheSettings(this);
    scrobbling = new ScrobblingSettings(this);
    custom = new CustomActionsSettings(this);
    apiKeys = new ApiKeysSettings(this);
    server->load();
    playback->load();
    interface->load();
    context->load();
    scrobbling->load();
    custom->load();
    QColor iconColor = Utils::clampColor(palette().text().color());
    addPage(QLatin1String("collection"), server, tr("Collection"), MonoIcon::icon(FontAwesome::music, iconColor), tr("Collection Settings"));
    addPage(QLatin1String("playback"), playback, tr("Playback"), MonoIcon::icon(FontAwesome::volumeup, iconColor), tr("Playback Settings"));
    addPage(QLatin1String("interface"), interface, tr("Interface"), MonoIcon::icon(FontAwesome::sliders, iconColor), tr("Interface Settings"));
    addPage(QLatin1String("info"), context, tr("Info"), MonoIcon::icon(FontAwesome::infocircle, iconColor), tr("Info View Settings"));
    addPage(QLatin1String("scrobbling"), scrobbling, tr("Scrobbling"), MonoIcon::icon(FontAwesome::lastfm, iconColor), tr("Scrobbling Settings"));
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd = new AudioCdSettings(0);
    audiocd->load();
    addPage(QLatin1String("cd"), audiocd, tr("Audio CD"), Icons::self()->albumMonoIcon, tr("Audio CD Settings"));
    #endif
    #ifdef ENABLE_PROXY_CONFIG
    proxy = new ProxySettings(0);
    proxy->load();
    addPage(QLatin1String("proxy"), proxy, tr("Proxy"), MonoIcon::icon(FontAwesome::globe, iconColor), tr("Proxy Settings"));
    #endif
    shortcuts = new ShortcutsSettingsPage(nullptr);
    addPage(QLatin1String("shortcuts"), shortcuts, tr("Shortcuts"), MonoIcon::icon(FontAwesome::keyboardo, iconColor), tr("Keyboard Shortcut Settings"));
    shortcuts->load();
    addPage(QLatin1String("cache"), cache, tr("Cache"), MonoIcon::icon(FontAwesome::foldero, iconColor), tr("Cached Items"));
    addPage(QLatin1String("custom"), custom, tr("Custom Actions"), MonoIcon::icon(FontAwesome::rocket, iconColor), tr("Custom Actions"));
    addPage(QLatin1String("apikeys"), apiKeys, tr("Service Keys"), MonoIcon::icon(FontAwesome::key, iconColor), tr("Service API Keys"));
    #ifdef Q_OS_MAC
    setCaption(tr("Cantata Preferences"));
    setMinimumWidth(800);
    #else
    setCaption(tr("Configure"));
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
    interface->save();
    #ifdef ENABLE_PROXY_CONFIG
    proxy->save();
    #endif
    shortcuts->save();
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd->save();
    #endif
    context->save();
    scrobbling->save();
    custom->save();
    apiKeys->save();
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

#include "moc_preferencesdialog.cpp"
