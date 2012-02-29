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

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KDialog>
#else
#include <QtGui/QDialog>
class ProxySettings;
class QDialogButtonBox;
class QAbstractButton;
#endif

class ServerSettings;
class PlaybackSettings;
class InterfaceSettings;
class LyricSettings;
class LyricsPage;
class ExternalSettings;
class HttpServerSettings;

#ifdef ENABLE_KDE_SUPPORT
class PreferencesDialog : public KDialog
#else
class PreferencesDialog : public QDialog
#endif
{
    Q_OBJECT

public:
    PreferencesDialog(QWidget *parent, LyricsPage *lp);

private:
#ifdef ENABLE_KDE_SUPPORT
    void slotButtonClicked(int button);
#endif

private Q_SLOTS:
    void writeSettings();
#ifndef ENABLE_KDE_SUPPORT
    void buttonPressed(QAbstractButton *button);
#endif

Q_SIGNALS:
    void settingsSaved();

private:
    ServerSettings *server;
    PlaybackSettings *playback;
    InterfaceSettings *interface;
    ExternalSettings *ext;
    LyricSettings *lyrics;
    HttpServerSettings *http;
#ifndef ENABLE_KDE_SUPPORT
    QDialogButtonBox *buttonBox;
    ProxySettings *proxy;
#endif
};

#endif
