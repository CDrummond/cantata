/*
 * Cantata Web
 *
 * Copyright (c) 2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "coversapi.h"
#include "apihandler.h"
#include "db/mpdlibrarydb.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "http/staticfilecontroller.h"
#include "support/utils.h"
#include <QSettings>
#include <QFile>
#include <QDebug>

#define DBUG if (ApiHandler::debugEnabled()) qWarning() << "CoversApi" << (void *)this << __FUNCTION__

CoversApi::CoversApi(const QSettings *settings)
{
    musicFolder=Utils::fixPath(settings->value("mpd/dir", "/var/lib/mpd/music").toString());
    coverName=settings->value("mpd/cover", "cover").toString();
}

HttpRequestHandler::HandleStatus CoversApi::handle(HttpRequest *request, HttpResponse *response)
{
    if (HttpRequest::Method_Get==request->method() && request->path()=="/api/v1/covers") {
        QString artistId=request->parameter("artistId");
        QString albumId=request->parameter("albumId");
        DBUG << "params" << artistId << albumId;
        Song song=MpdLibraryDb::self()->getCoverSong(artistId, albumId);
        if (!song.isEmpty()) {
            QString path=musicFolder+Utils::getDir(song.file)+coverName+'.';
            QStringList extensions=QStringList() << "jpg" << "png";
            foreach (const QString &ext, extensions) {
                DBUG << "Check" << path+ext;
                if (QFile::exists(path+ext)) {
                    QFile f(path+ext);
                    if (f.open(QIODevice::ReadOnly)) {
                        StaticFileController::setContentType(ext, response);
                        response->write(f.readAll());
                    } else {
                        DBUG << "403 Forbidden: " << path+ext;
                        response->setStatus(403, "Forbidden");
                        response->write("403 Forbidden");
                        response->flush();
                    }
                    return Status_Handled;
                }
            }
        }
        DBUG << "404 Not found: " << artistId << albumId;
        response->setStatus(404, "Not found");
        response->write("404 Not found");
        response->flush();
        return Status_Handled;
    }
    return Status_BadRequest;
}
