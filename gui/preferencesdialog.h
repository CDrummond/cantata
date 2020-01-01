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

#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include "config.h"
#include "support/configdialog.h"

class ProxySettings;
class ShortcutsSettingsPage;
class ServerSettings;
class PlaybackSettings;
class InterfaceSettings;
class ContextSettings;
struct MPDConnectionDetails;
class CacheSettings;
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
class AudioCdSettings;
#endif
class QStringList;
#ifdef ENABLE_PROXY_CONFIG
class ProxySettings;
#endif
class ScrobblingSettings;
class CustomActionsSettings;
class ApiKeysSettings;

class PreferencesDialog : public ConfigDialog
{
    Q_OBJECT

public:
    static int instanceCount();

    PreferencesDialog(QWidget *parent);
    ~PreferencesDialog() override;

private:
    void save() override;
    void cancel() override;

public Q_SLOTS:
    void showPage(const QString &page);

private Q_SLOTS:
    void writeSettings();

Q_SIGNALS:
    void settingsSaved();

private:
    ServerSettings *server;
    PlaybackSettings *playback;
    InterfaceSettings *interface;
    ContextSettings *context;
    #ifdef ENABLE_PROXY_CONFIG
    ProxySettings *proxy;
    #endif
    ShortcutsSettingsPage *shortcuts;
    CacheSettings *cache;
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    AudioCdSettings *audiocd;
    #endif
    ScrobblingSettings *scrobbling;
    CustomActionsSettings *custom;
    ApiKeysSettings *apiKeys;
};

#endif
