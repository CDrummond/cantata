/*
 * Cantata
 *
 * Copyright 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KConfigGroup>
namespace KWallet {
class Wallet;
}
#else
#include <QtCore/QSettings>
#endif

class Settings
{
public:
    static Settings *self();

    Settings();
    ~Settings();

    QString connectionHost();
    QString connectionPasswd();
    int connectionPort();
    bool consumePlaylist();
    bool randomPlaylist();
    bool repeatPlaylist();
    bool showPlaylist();
    QByteArray playlistHeaderState();
    QByteArray splitterState();
#ifndef ENABLE_KDE_SUPPORT
    QSize mainWindowSize();
#endif
    bool useSystemTray();
    bool showPopups();
    QString mpdDir();
    int coverSize();
    int sidebar();
    QStringList lyricProviders();

    void saveConnectionHost(const QString &v);
    void saveConnectionPasswd(const QString &v);
    void saveConnectionPort(int v);
    void saveConsumePlaylist(bool v);
    void saveRandomPlaylist(bool v);
    void saveRepeatPlaylist(bool v);
    void saveShowPlaylist(bool v);
    void savePlaylistHeaderState(const QByteArray &v);
    void saveSplitterState(const QByteArray &v);
#ifndef ENABLE_KDE_SUPPORT
    void saveMainWindowSize(const QSize &v);
#endif
    void saveUseSystemTray(bool v);
    void saveShowPopups(bool v);
    void saveMpdDir(const QString &v);
    void saveCoverSize(int v);
    void saveSidebar(int v);
    void saveLyricProviders(const QStringList &p);
    void save();
#ifdef ENABLE_KDE_SUPPORT
    bool openWallet();
#endif

private:
#ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg;
    KWallet::Wallet *wallet;
    QString passwd;
#else
    QSettings cfg;
#endif
};

#endif
