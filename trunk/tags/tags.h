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

#ifndef TAGS_H
#define TAGS_H

#include "song.h"
#include "utils.h"
#include "config.h"
#include <QImage>
#include <QMetaType>

#if defined ENABLE_EXTERNAL_TAGS && !defined CANTATA_TAG_SERVER
#include "tagclient.h"
#endif

namespace Tags
{
    struct ReplayGain
    {
        ReplayGain(double tg=0.0, double ag=0.0, double tp=0.0, double ap=0.0)
            : trackGain(tg)
            , albumGain(ag)
            , trackPeak(tp)
            , albumPeak(ap) {
        }

        bool isEmpty() const
        {
            return Utils::equal(trackGain, 0.0) && Utils::equal(trackPeak, 0.0) && Utils::equal(albumGain, 0.0) && Utils::equal(albumPeak, 0.0);
        }

        double trackGain;
        double albumGain;
        double trackPeak;
        double albumPeak;
    };

    enum Update
    {
        Update_Failed,
        Update_None,
        Update_Modified
        #ifdef ENABLE_EXTERNAL_TAGS
        , Update_Timedout
        , Update_BadFile
        #endif
    };

    #if defined ENABLE_EXTERNAL_TAGS && !defined CANTATA_TAG_SERVER
    inline Song read(const QString &fileName) { return TagClient::read(fileName); }
    inline QImage readImage(const QString &fileName) { return TagClient::readImage(fileName); }
    inline QString readLyrics(const QString &fileName) { return TagClient::readLyrics(fileName); }
    inline Update updateArtistAndTitle(const QString &fileName, const Song &song) { return (Update)TagClient::updateArtistAndTitle(fileName, song); }
    inline Update update(const QString &fileName, const Song &from, const Song &to, int id3Ver=-1) { return (Update)TagClient::update(fileName, from, to, id3Ver); }
    inline ReplayGain readReplaygain(const QString &fileName) { return TagClient::readReplaygain(fileName); }
    inline Update updateReplaygain(const QString &fileName, const ReplayGain &rg) { return (Update)TagClient::updateReplaygain(fileName, rg); }
    inline Update embedImage(const QString &fileName, const QByteArray &cover) { return (Update)TagClient::embedImage(fileName, cover); }
    #else
    extern Song read(const QString &fileName);
    extern QImage readImage(const QString &fileName);
    extern QString readLyrics(const QString &fileName);
    extern Update updateArtistAndTitle(const QString &fileName, const Song &song);
    extern Update update(const QString &fileName, const Song &from, const Song &to, int id3Ver=-1);
    extern ReplayGain readReplaygain(const QString &fileName);
    extern Update updateReplaygain(const QString &fileName, const ReplayGain &rg);
    extern Update embedImage(const QString &fileName, const QByteArray &cover);
    #endif
}

#ifdef ENABLE_EXTERNAL_TAGS
Q_DECLARE_METATYPE(Tags::ReplayGain)

inline QDataStream & operator<<(QDataStream &stream, const Tags::ReplayGain &rg)
{
    stream << rg.trackGain << rg.albumGain << rg.trackPeak << rg.albumPeak;
    return stream;
}

inline QDataStream & operator>>(QDataStream &stream, Tags::ReplayGain &rg)
{
    stream >> rg.trackGain >> rg.albumGain >> rg.trackPeak >> rg.albumPeak;
    return stream;
}
#endif

#endif
