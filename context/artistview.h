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

#ifndef ARTIST_VIEW_H
#define ARTIST_VIEW_H

#include "view.h"
#include <QMap>

class ComboBox;
class QLabel;
class QNetworkReply;
class QIODevice;
class QImage;
class QUrl;

class ArtistView : public View
{
    Q_OBJECT

public:
    static const QLatin1String constCacheDir;
    static const QLatin1String constInfoExt;
    static const QLatin1String constSimilarInfoExt;

    ArtistView(QWidget *parent);
    virtual ~ArtistView() { abort(); }

    void update(const Song &s, bool force=false);

Q_SIGNALS:
    void findArtist(const QString &artist);
    void haveBio(const QString &artist, const QString &bio);

public Q_SLOTS:
    void artistImage(const Song &song, const QImage &i, const QString &f);

private Q_SLOTS:
    void setBio();
    void handleSimilarReply();
    void showArtist(const QUrl &url);

private:
    void loadBio();
    void searchResponse(const QString &resp);
    void loadSimilar();
    void requestSimilar();
    QStringList parseSimilarResponse(const QByteArray &resp);
    void buildSimilar(const QStringList &artists);
    void abort();

private:
    bool triedWithFilter;
    QString biography;
    QString similarArtists;
    QNetworkReply *currentSimilarJob;
    QString provider;
    #ifndef Q_OS_WIN
    QString webLinks;
    #endif
};

#endif
