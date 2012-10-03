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

#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include "config.h"
#include "dialog.h"
#ifndef ENABLE_KDE_SUPPORT
class ProxySettings;
class ShortcutsSettingsPage;
#endif

class ServerSettings;
class ServerPlaybackSettings;
class PlaybackSettings;
class InterfaceSettings;
class LyricSettings;
class LyricsPage;
class ExternalSettings;
#ifdef TAGLIB_FOUND
class HttpServerSettings;
#endif
class MPDConnectionDetails;

class PreferencesDialog : public Dialog
{
    Q_OBJECT

public:
    PreferencesDialog(QWidget *parent, LyricsPage *lp);

private:
    void slotButtonClicked(int button);

private Q_SLOTS:
    void writeSettings();

Q_SIGNALS:
    void settingsSaved();
    void connectTo(const MPDConnectionDetails &details);

private:
    ServerSettings *server;
    ServerPlaybackSettings *serverplayback;
    PlaybackSettings *playback;
    InterfaceSettings *interface;
    ExternalSettings *ext;
    LyricSettings *lyrics;
    #ifdef TAGLIB_FOUND
    HttpServerSettings *http;
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    ProxySettings *proxy;
    ShortcutsSettingsPage *shortcuts;
    #endif
};

#endif
