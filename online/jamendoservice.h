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

#ifndef JAMENDO_SERVICE_H
#define JAMENDO_SERVICE_H

#include "onlineservice.h"
#include <QtCore/QLatin1String>

class MusicLibraryItemArtist;
class MusicLibraryItemAlbum;

class JamendoMusicLoader : public OnlineMusicLoader
{
public:
    JamendoMusicLoader(const QUrl &src);
    bool parse(QXmlStreamReader &xml);

private:
    void parseArtist(QXmlStreamReader &xml);
    void parseAlbum(MusicLibraryItemArtist *artist, QXmlStreamReader &xml);
    void parseSong(MusicLibraryItemArtist *artist, MusicLibraryItemAlbum *album, QXmlStreamReader &xml);
};

class JamendoService : public OnlineService
{
    Q_OBJECT

public:
    enum Format {
        FMT_MP3,
        FMT_Ogg
    };

    static const QLatin1String constName;
    JamendoService(OnlineServicesModel *m)
        : OnlineService(m, constName)
        , format(FMT_MP3) {
    }

    virtual const Icon & serviceIcon() const { return Icons::jamendoIcon; }
    Song fixPath(const Song &orig) const;
    void createLoader();
    void loadConfig();
    void saveConfig();
    void configure(QWidget *p);

private:
    Format format;
};

#endif
