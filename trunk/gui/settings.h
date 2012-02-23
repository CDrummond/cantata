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
#include "config.h"

#define CANTATA_MAKE_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))

class QTimer;

class Settings : public QObject
{
    Q_OBJECT

public:
    enum Constants
    {
        MinFade     = 400,
        MaxFade     = 4000,
        DefaultFade = 2000
    };

    static Settings *self();

    Settings();
    ~Settings();

    QString connectionHost();
    QString connectionPasswd();
    int connectionPort();
    bool showPlaylist();
    QByteArray playQueueHeaderState();
    QByteArray splitterState();
    #ifndef ENABLE_KDE_SUPPORT
    QSize mainWindowSize();
    #endif
    bool useSystemTray();
    bool showPopups();
    bool stopOnExit();
    bool smallPlaybackButtons();
    bool smallControlButtons();
    const QString & mpdDir();
    int libraryView();
    int albumsView();
    int folderView();
    int playlistsView();
    int streamsView();
    int libraryCoverSize();
    int albumsCoverSize();
    bool albumFirst();
    int sidebar();
    bool libraryYear();
    bool groupSingle();
    QStringList lyricProviders();
    QString page();
    QStringList hiddenPages();
    bool mpris();
    bool dockManager();
    #ifdef ENABLE_DEVICES_SUPPORT
    bool overwriteSongs();
    bool showDeleteAction();
    int devicesView();
    #endif
    int version();
    int stopFadeDuration();
    int httpPort();
    bool enableHttp();
    bool alwaysUseHttp();

    void saveConnectionHost(const QString &v);
    void saveConnectionPasswd(const QString &v);
    void saveConnectionPort(int v);
    void saveShowPlaylist(bool v);
    void saveStopOnExit(bool v);
    void saveSmallPlaybackButtons(bool v);
    void saveSmallControlButtons(bool v);
    void savePlayQueueHeaderState(const QByteArray &v);
    void saveSplitterState(const QByteArray &v);
    #ifndef ENABLE_KDE_SUPPORT
    void saveMainWindowSize(const QSize &v);
    #endif
    void saveUseSystemTray(bool v);
    void saveShowPopups(bool v);
    void saveMpdDir(const QString &v);
    void saveLibraryView(int v);
    void saveAlbumsView(int v);
    void saveFolderView(int v);
    void savePlaylistsView(int v);
    void saveStreamsView(int v);
    void saveLibraryCoverSize(int v);
    void saveAlbumsCoverSize(int v);
    void saveAlbumFirst(bool v);
    void saveSidebar(int v);
    void saveLibraryYear(bool v);
    void saveGroupSingle(bool v);
    void saveLyricProviders(const QStringList &p);
    void savePage(const QString &v);
    void saveHiddenPages(const QStringList &p);
    void saveMpris(bool v);
    void saveDockManager(bool v);
    #ifdef ENABLE_DEVICES_SUPPORT
    void saveOverwriteSongs(bool v);
    void saveShowDeleteAction(bool v);
    void saveDevicesView(int v);
    #endif
    void saveStopFadeDuration(int v);
    void saveHttpPort(int v);
    void saveEnableHttp(bool v);
    void saveAlwaysUseHttp(bool v);
    void save(bool force=false);
    #ifdef ENABLE_KDE_SUPPORT
    bool openWallet();
    #endif

private Q_SLOTS:
    void actualSave();

private:
    QString mpdDirSetting;
    QTimer *timer;
    int ver;
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg;
    KWallet::Wallet *wallet;
    QString passwd;
    #else
    QSettings cfg;
    #endif
};

#endif
