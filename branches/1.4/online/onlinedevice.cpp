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

#include "onlinedevice.h"
#include "models/musiclibrarymodel.h"
#include "models/dirviewmodel.h"
#include "support/utils.h"
#include "network/networkaccessmanager.h"
#include "mpd/mpdconnection.h"
#include <QDir>

void OnlineDevice::copySongTo(const Song &s, const QString &musicPath, bool overwrite, bool copyCover)
{
    Q_UNUSED(copyCover)

    jobAbortRequested=false;
    QString baseDir=MPDConnection::self()->getDetails().dir;
    QString dest(baseDir+musicPath);
    if (!overwrite && (MusicLibraryModel::self()->songExists(s) || QFile::exists(dest))) {
        emit actionStatus(SongExists);
        return;
    }

    overWrite=overwrite;
    lastProg=-1;
    currentDestFile=baseDir+musicPath;
    currentSong=s;

    QDir dir(Utils::getDir(dest));
    if (!dir.exists() && !Utils::createWorldReadableDir(dir.absolutePath(), baseDir)) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    job=NetworkAccessManager::self()->get(QUrl(s.file));
    connect(job, SIGNAL(finished()), SLOT(downloadFinished()));
    connect(job, SIGNAL(downloadProgress(qint64,qint64)), SLOT(downloadProgress(qint64,qint64)));
}

void OnlineDevice::downloadFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (reply!=job) {
        return;
    }

    if (reply->ok()) {
        if (overWrite && QFile::exists(currentDestFile)) {
            QFile::remove(currentDestFile);
        }

        QFile f(currentDestFile);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(reply->readAll());

            currentSong.file=currentDestFile.mid(MPDConnection::self()->getDetails().dir.length());
            QString origPath;
            if (MPDConnection::self()->isMopdidy()) {
                origPath=currentSong.file;
                currentSong.file=Song::encodePath(currentSong.file);
            }
            Utils::setFilePerms(currentDestFile);
            MusicLibraryModel::self()->addSongToList(currentSong);
            DirViewModel::self()->addFileToList(origPath.isEmpty() ? currentSong.file : origPath,
                                                origPath.isEmpty() ? QString() : currentSong.file);
            emit actionStatus(Ok);
        } else {
            emit actionStatus(WriteFailed);
        }
    } else {
        emit actionStatus(DownloadFailed);
    }
}

void OnlineDevice::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (jobAbortRequested || bytesTotal<=1) {
        return;
    }
    int prog=(bytesReceived*100)/bytesTotal;
    if (prog!=lastProg) {
        lastProg=prog;
        emit progress(prog);
    }
}
