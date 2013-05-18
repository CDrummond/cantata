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
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef ALBUM_VIEW_H
#define ALBUM_VIEW_H

#include "view.h"

class QImage;
class QNetworkReply;
class QByteArray;
class QUrl;

class AlbumView : public View
{
    Q_OBJECT
public:
    static const QLatin1String constCacheDir;
    static const QLatin1String constInfoExt;

    AlbumView(QWidget *p);

    void update(const Song &song, bool force=false);

Q_SIGNALS:
    void playSong(const QString &file);

public Q_SLOTS:
    void coverRetreived(const Song &s, const QImage &img, const QString &file);
    void infoRetreived();
    void playSong(const QUrl &u);

private:
    void getTrackListing();
    void getDetails();
    bool parseLastFmResponse(const QByteArray &data);
    void updateDetails(bool preservePos=false);
    void abort();

private:
    int detailsReceived;
    QString details;
    QString trackList;
    QNetworkReply *job;
};

#endif
