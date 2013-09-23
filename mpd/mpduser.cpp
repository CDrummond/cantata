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
 * General Public License for more det.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "mpduser.h"
#include "config.h"
#include "utils.h"
#include "localize.h"
#include "settings.h"
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QCoreApplication>
#include <signal.h>
#if defined Q_OS_WIN
#include <QDesktopServices>
#endif
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(MPDUser, instance)
#endif

const QString MPDUser::constName=QLatin1String("-");

static const QString constDir=QLatin1String("mpd");
static const QString constConfigFile=QLatin1String("mpd.conf");
static const QString constMusicFolderKey=QLatin1String("music_directory");
static const QString constSocketKey=QLatin1String("bind_to_address");
static const QString constPlaylistsKey=QLatin1String("playlist_directory");
static const QString constPidKey=QLatin1String("pid_file");

QString MPDUser::translatedName()
{
    return i18n("Personal");
}

MPDUser * MPDUser::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static MPDUser *instance=0;
    if(!instance) {
        instance=new MPDUser;
    }
    return instance;
    #endif
}

MPDUser::MPDUser()
{
    // For now, per-user MPD support is disabled for windows builds
    // - as I'm unsure how/if MPD works in windows!!!
    // - If enable, also need to fix isRunning!!
    #ifndef Q_OS_WIN
    mpdExe=Utils::findExe("mpd");
    #endif
    det.name=constName;
}

bool MPDUser::isSupported()
{
    return !mpdExe.isEmpty();
}

bool MPDUser::isRunning()
{
    #ifdef Q_OS_WIN
    return false;
    #else
    int pid=getPid();
    return pid ? 0==::kill(pid, 0) : false;
    #endif
}

static QString readValue(const QString &line, const QString &key)
{
    if (line.startsWith(key)) {
        int start=line.indexOf("\"");
        int end=-1==start ? -1 : line.indexOf("\"", start+1);
        if (-1!=end) {
            return line.mid(start+1, (end-start)-1);
        }
    }
    return QString();
}

void MPDUser::start()
{
    if (isRunning() || mpdExe.isEmpty()) {
        return;
    }

    init(true);

    if (!det.dir.isEmpty() && !det.hostname.isEmpty()) {
        controlMpd(false);
    }
}

void MPDUser::stop()
{
    if (!isRunning() || mpdExe.isEmpty()) {
        return;
    }

    init(false);
    controlMpd(true);
}

void MPDUser::setMusicFolder(const QString &folder)
{
    if (folder==det.dir) {
        return;
    }

    QFile cfgFile(Utils::configDir(constDir, true)+constConfigFile);
    QStringList lines;
    if (cfgFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        while (!cfgFile.atEnd()) {
            QString line = cfgFile.readLine();
            if (line.startsWith(constMusicFolderKey)) {
                lines.append(constMusicFolderKey+" \""+folder+"\"\n");
            } else {
                lines.append(line);
            }
        }
    }

    if (!lines.isEmpty()) {
        cfgFile.close();
        if (cfgFile.open(QIODevice::WriteOnly|QIODevice::Text)) {
            QTextStream out(&cfgFile);
            foreach (const QString &line, lines) {
                out << line;
            }
        }
    }
    det.dir=folder;
    det.dirReadable=QDir(det.dir).isReadable();
    if (0!=getPid()) {
        controlMpd(true); // Stop
        controlMpd(false); // Start
    }    
}

void MPDUser::setDetails(const MPDConnectionDetails &d)
{
    setMusicFolder(d.dir);
    bool dirReadable=det.dirReadable;
    det=d;
    det.dirReadable=dirReadable;
}

static void removeDir(const QString &d)
{
    if (d.isEmpty()) {
        return;
    }
    QDir dir(d);
    if (dir.exists()) {
        QString dirName=dir.dirName();
        if (!dirName.isEmpty()) {
            dir.cdUp();
            dir.rmdir(dirName);
        }
    }
}

void MPDUser::cleanup()
{
    QString cfgFileName(Utils::configDir(constDir, false)+constConfigFile);
    QFile cfgFile(cfgFileName);
    QSet<QString> files;
    QSet<QString> dirs;
    QString playlistDir;

    if (cfgFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QStringList fileKeys=QStringList() << constPidKey << constSocketKey << QLatin1String("db_file")
                                           << QLatin1String("state_file") << QLatin1String("sticker_file");
        QStringList dirKeys=QStringList() << constPlaylistsKey;
        while (!cfgFile.atEnd()) {
            QString line = cfgFile.readLine();
            foreach (const QString &key, fileKeys) {
                if (line.startsWith(key)) {
                    QString file=readValue(line, key);
                    if (!file.isEmpty()) {
                        QString dir=Utils::getDir(file);
                        if (!dir.isEmpty()) {
                            dirs.insert(dir);
                        }
                        files.insert(file);
                        fileKeys.removeAll(key);
                    }
                }
            }
            if (playlistDir.isEmpty() && line.startsWith(constPlaylistsKey)) {
                playlistDir=readValue(line, constPlaylistsKey);
            }
        }
        files.insert(cfgFileName);
        dirs.insert(Utils::getDir(cfgFileName));
    }

    if (!dirs.isEmpty() && !files.isEmpty()) {
        foreach (const QString &f, files) {
            QFile::remove(f);
        }

        if (!playlistDir.isEmpty()) {
            QFileInfoList files=QDir(playlistDir).entryInfoList(QStringList() << "*.m3u", QDir::Files|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &file, files) {
                QFile::remove(file.absoluteFilePath());
            }
            removeDir(playlistDir);
        }

        foreach (const QString &d, dirs) {
            removeDir(d);
        }
        removeDir(Utils::configDir(constDir, false));
        removeDir(Utils::cacheDir(constDir, false));
    }
}

void MPDUser::init(bool create)
{
    if (det.dir.isEmpty() || det.hostname.isEmpty() || pidFileName.isEmpty()) {
        // Read coverFileName from Cantata settings...
        det.coverName=Settings::self()->connectionDetails(constName).coverName;
        det.dirReadable=false;

        // Read music folder and socket from MPD conf file...
        QString cfgDir=Utils::configDir(constDir, create);
        QString cfgName(cfgDir+constConfigFile);
        QString playlists;

        if (create && !QFile::exists(cfgName)) {
            // Conf file does not exist, so we need to create one...
            #ifdef Q_OS_WIN
            QFile cfgTemplate(QCoreApplication::applicationDirPath()+"/mpd/"+constConfigFile+".template");
            #else
            QFile cfgTemplate(INSTALL_PREFIX"/share/cantata/mpd/"+constConfigFile+".template");
            #endif
            if (cfgTemplate.open(QIODevice::ReadOnly|QIODevice::Text)) {
                QFile cfgFile(cfgName);
                if (cfgFile.open(QIODevice::WriteOnly|QIODevice::Text)) {
                    QString homeDir=QDir::homePath();
                    QString cacheDir=Utils::cacheDir(constDir, create);
                    QTextStream out(&cfgFile);
                    while (!cfgTemplate.atEnd()) {
                        QString line = cfgTemplate.readLine();
                        line=line.replace(QLatin1String("${HOME}"), homeDir);
                        line=line.replace(QLatin1String("${CONFIG_DIR}"), cfgDir);
                        line=line.replace(QLatin1String("${CACHE_DIR}"), cacheDir);
                        line=line.replace("//", "/");
                        out << line;

                        if (det.dir.isEmpty()) {
                            det.dir=readValue(line, constMusicFolderKey);
                        }
                        if (det.hostname.isEmpty()) {
                            det.hostname=readValue(line, constSocketKey);
                        }
                        if (pidFileName.isEmpty()) {
                            pidFileName=readValue(line, constPidKey);
                        }
                        // Create playlists dir...
                        if (playlists.isEmpty()) {
                            playlists=readValue(line, constPlaylistsKey);
                            if (!playlists.isEmpty()) {
                                Utils::createDir(playlists, QString());
                            }
                        }
                    }
                }
            }
        }

        if (det.dir.isEmpty() || det.hostname.isEmpty() || pidFileName.isEmpty()) {
            QFile cfgFile(cfgName);
            if (cfgFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
                while (!cfgFile.atEnd() && (det.dir.isEmpty() || det.hostname.isEmpty() || pidFileName.isEmpty())) {
                    QString line = cfgFile.readLine();
                    if (det.dir.isEmpty()) {
                        det.dir=readValue(line, constMusicFolderKey);
                    }
                    if (det.hostname.isEmpty()) {
                        det.hostname=readValue(line, constSocketKey);
                    }
                    if (pidFileName.isEmpty()) {
                        pidFileName=readValue(line, constPidKey);
                    }
                }
            }
            det.dirReadable=QDir(det.dir).isReadable();
        }
        det.name=constName;
    }
}

int MPDUser::getPid()
{
    int pid=0;

    init(false);
    if (!pidFileName.isEmpty()) {
        QFile pidFile(pidFileName);

        if (pidFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
            QTextStream str(&pidFile);
            str >> pid;
        }
    }
    return pid;
}

bool MPDUser::controlMpd(bool stop)
{
    QStringList args=QStringList() << Utils::configDir(constDir, true)+constConfigFile;
    if (stop) {
        args+="--kill";
    }

    qint64 appPid=0;
    bool started=QProcess::startDetached(mpdExe, args, QString(), &appPid);
    if (started && !stop && 0!=appPid) {
        for (int i=0; i<8; ++i) {
            if (getPid()!=appPid) {
                Utils::msleep(250);
            } else {
                Utils::msleep(250);
                return true;
            }
        }
    }
    return started;
}
