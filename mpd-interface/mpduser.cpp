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
 * General Public License for more det.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "mpduser.h"
#include "config.h"
#include "support/utils.h"
#include "gui/settings.h"
#include "support/globalstatic.h"
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QCoreApplication>
#include <signal.h>

const QString MPDUser::constName=QLatin1String("-");

static const QString constDir=QLatin1String("mpd");
static const QString constConfigFile=QLatin1String("mpd.conf");
static const QString constMusicFolderKey=QLatin1String("music_directory");
static const QString constSocketKey=QLatin1String("bind_to_address");
static const QString constPlaylistsKey=QLatin1String("playlist_directory");
static const QString constPidKey=QLatin1String("pid_file");

QString MPDUser::translatedName()
{
    return QObject::tr("Personal");
}

GLOBAL_STATIC(MPDUser, instance)

#if !defined Q_OS_WIN && !defined Q_OS_MAC
static void moveConfig()
{
    QString oldName=QDir::homePath()+"/.config/cantata/"+constDir+"/"+constConfigFile;

    if (QFile::exists(oldName)) {
        QString newName=Utils::dataDir(constDir, true)+constConfigFile;
        if (QFile::exists(newName)) {
            QFile::remove(oldName);
        } else {
            QFile::rename(oldName, newName);
        }
    }
}
#endif

MPDUser::MPDUser()
{
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    moveConfig();
    #endif
    // For now, per-user MPD support is disabled for windows builds
    // - as I'm unsure how/if MPD works in windows!!!
    // - If enable, also need to fix isRunning!!
    #ifndef Q_OS_WIN
    mpdExe=Utils::findExe("mpd");
    #endif
    det.name=constName;
    det.allowLocalStreaming=true;
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

static QString readValue(const QString &line)
{
    int start=line.indexOf("\"");
    int end=-1==start ? -1 : line.indexOf("\"", start+1);
    return -1==end ? QString() : line.mid(start+1, (end-start)-1);
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
    init(true);

    QFile cfgFile(Utils::dataDir(constDir, true)+constConfigFile);
    QStringList lines;
    if (cfgFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        while (!cfgFile.atEnd()) {
            QString line = QString::fromUtf8(cfgFile.readLine());
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
            for (const QString &line: lines) {
                out << line;
            }
        }
    }
    det.dir=folder;
    det.setDirReadable();
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
    QString cfgFileName(Utils::dataDir(constDir, false)+constConfigFile);
    QFile cfgFile(cfgFileName);
    QSet<QString> files;
    QSet<QString> dirs;
    QString playlistDir;

    if (cfgFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QStringList fileKeys=QStringList() << constPidKey << constSocketKey << QLatin1String("db_file")
                                           << QLatin1String("state_file") << QLatin1String("sticker_file");
        QStringList dirKeys=QStringList() << constPlaylistsKey;
        while (!cfgFile.atEnd()) {
            QString line = QString::fromUtf8(cfgFile.readLine());
            for (const QString &key: fileKeys) {
                if (line.startsWith(key)) {
                    QString file=readValue(line);
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
                playlistDir=readValue(line);
            }
        }
        files.insert(cfgFileName);
        dirs.insert(Utils::getDir(cfgFileName));
    }

    if (!dirs.isEmpty() && !files.isEmpty()) {
        for (const QString &f: files) {
            QFile::remove(f);
        }

        if (!playlistDir.isEmpty()) {
            QFileInfoList files=QDir(playlistDir).entryInfoList(QStringList() << "*.m3u", QDir::Files|QDir::NoDotAndDotDot);
            for (const QFileInfo &file: files) {
                QFile::remove(file.absoluteFilePath());
            }
            removeDir(playlistDir);
        }

        for (const QString &d: dirs) {
            removeDir(d);
        }
        removeDir(Utils::dataDir(constDir, false));
        removeDir(Utils::cacheDir(constDir, false));
    }
}

void MPDUser::init(bool create)
{
    if (create || det.dir.isEmpty() || det.hostname.isEmpty() || pidFileName.isEmpty()) {
        // Read coverFileName from Cantata settings...
        det.dirReadable=false;

        // Read music folder and socket from MPD conf file...
        QString cfgDir=Utils::dataDir(constDir, create);
        QString cfgName(cfgDir+constConfigFile);
        QString playlists;
        if (create && !QFile::exists(cfgName)) {
            // Conf file does not exist, so we need to create one...
            QFile cfgTemplate(":"+constConfigFile+".template");

            if (cfgTemplate.open(QIODevice::ReadOnly|QIODevice::Text)) {
                QFile cfgFile(cfgName);
                if (cfgFile.open(QIODevice::WriteOnly|QIODevice::Text)) {
                    QString homeDir=QDir::homePath();
                    QString cacheDir=Utils::cacheDir(constDir, create);
                    QString dataDir=Utils::dataDir(constDir, create);
                    QTextStream out(&cfgFile);
                    while (!cfgTemplate.atEnd()) {
                        QString line = cfgTemplate.readLine();
                        line=line.replace(QLatin1String("${HOME}"), homeDir);
                        line=line.replace(QLatin1String("${CONFIG_DIR}"), cfgDir);
                        line=line.replace(QLatin1String("${DATA_DIR}"), dataDir);
                        line=line.replace(QLatin1String("${CACHE_DIR}"), cacheDir);
                        line=line.replace("//", "/");
                        out << line;

                        if (det.dir.isEmpty() && line.startsWith(constMusicFolderKey)) {
                            det.dir=Utils::fixPath(readValue(line));
                        }
                        if (det.hostname.isEmpty() && line.startsWith(constSocketKey)) {
                            det.hostname=readValue(line);
                        }
                        if (pidFileName.isEmpty() && line.startsWith(constPidKey)) {
                            pidFileName=readValue(line);
                        }
                        // Create playlists dir...
                        if (playlists.isEmpty() && line.startsWith(constPlaylistsKey)) {
                            playlists=readValue(line);
                            if (!playlists.isEmpty()) {
                                Utils::createWorldReadableDir(playlists, QString());
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
                    QString line = QString::fromUtf8(cfgFile.readLine());
                    if (det.dir.isEmpty() && line.startsWith(constMusicFolderKey)) {
                        det.dir=Utils::fixPath(readValue(line));
                    }
                    if (det.hostname.isEmpty() && line.startsWith(constSocketKey)) {
                        det.hostname=readValue(line);
                    }
                    if (pidFileName.isEmpty() && line.startsWith(constPidKey)) {
                        pidFileName=readValue(line);
                    }
                }
            }
            det.setDirReadable();
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
    QString confFile=Utils::dataDir(constDir, true)+constConfigFile;
    if (!QFile::exists(confFile)) {
        return false;
    }
    QStringList args=QStringList() << confFile;
    if (stop) {
        args+="--kill";
    } else {
        // Ensure cache dir exists before starting MPD
        Utils::cacheDir(constDir, true);
        if (!pidFileName.isEmpty() && QFile::exists(pidFileName)) {
            QFile::remove(pidFileName);
        }
    }
    bool started=QProcess::startDetached(mpdExe, args);
    if (started && !stop) {
        for (int i=0; i<8; ++i) {
            Utils::msleep(250);
            if (0!=getPid()) {
                return true;
            }
        }
    }
    return started;
}
