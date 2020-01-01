/*
 * Cantata
 *
 * Copyright (c) 2017-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef JAMENDO_SERVICE_H
#define JAMENDO_SERVICE_H

#include "models/sqllibrarymodel.h"
#include "onlinedbservice.h"

class JamendoXmlParser : public OnlineXmlParser
{
public:
    int parse(QXmlStreamReader &xml) override;
private:
    void parseArtist(QList<Song> *songList, QXmlStreamReader &xml);
    void parseAlbum(Song &song, QList<Song> *songList, QXmlStreamReader &xml);
    void parseSong(Song &song, const QString &albumGenre, QXmlStreamReader &xml);
};

class JamendoService : public OnlineDbService
{
    Q_OBJECT
public:
    enum Format {
        FMT_MP3,
        FMT_Ogg
    };

    JamendoService(QObject *p);
    QVariant data(const QModelIndex &index, int role) const override;
    QString name() const override;
    QString title() const override;
    QString descr() const override;
    OnlineXmlParser * createParser() override;
    QUrl listingUrl() const override;
    void configure(QWidget *p) override;
    int averageSize() const override { return 100; }
private:
    Song & fixPath(Song &s) const override;

private:
    Format format;
};

#endif
