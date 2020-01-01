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
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef TAGS_H
#define TAGS_H

#include "mpd-interface/song.h"
#include "support/utils.h"
#include "config.h"
#include <QMap>
#include <QImage>
#include <QMetaType>

#ifndef CANTATA_TAG_SERVER
#include "taghelperiface.h"
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
        Update_Modified,
        Update_BadFile
    };

    void enableDebug();
    #ifndef CANTATA_TAG_SERVER
    inline void init() { TagHelperIface::self(); }
    inline void stop() { TagHelperIface::self()->stop(); }
    inline Song read(const QString &fileName) { return TagHelperIface::self()->read(fileName); }
    inline QImage readImage(const QString &fileName) { return TagHelperIface::self()->readImage(fileName); }
    inline QString readLyrics(const QString &fileName) { return TagHelperIface::self()->readLyrics(fileName); }
    inline QString readComment(const QString &fileName) { return TagHelperIface::self()->readComment(fileName); }
    inline Update updateArtistAndTitle(const QString &fileName, const Song &song) { return (Update)TagHelperIface::self()->updateArtistAndTitle(fileName, song); }
    inline Update update(const QString &fileName, const Song &from, const Song &to, int id3Ver=-1, bool saveComment=false) { return (Update)TagHelperIface::self()->update(fileName, from, to, id3Ver, saveComment); }
    inline ReplayGain readReplaygain(const QString &fileName) { return TagHelperIface::self()->readReplaygain(fileName); }
    inline Update updateReplaygain(const QString &fileName, const ReplayGain &rg) { return (Update)TagHelperIface::self()->updateReplaygain(fileName, rg); }
    inline Update embedImage(const QString &fileName, const QByteArray &cover) { return (Update)TagHelperIface::self()->embedImage(fileName, cover); }
    inline QString oggMimeType(const QString &fileName) { return TagHelperIface::self()->oggMimeType(fileName); }
    inline int readRating(const QString &fileName) { return TagHelperIface::self()->readRating(fileName); }
    inline Update updateRating(const QString &fileName, int rating) { return (Update)TagHelperIface::self()->updateRating(fileName, rating); }
    inline QMap<QString, QString> readAll(const QString &fileName) { return TagHelperIface::self()->readAll(fileName); }
    #else
    inline void init() { }
    inline void stop() { }
    extern Song read(const QString &fileName);
    extern QImage readImage(const QString &fileName);
    extern QString readLyrics(const QString &fileName);
    extern QString readComment(const QString &fileName);
    extern Update updateArtistAndTitle(const QString &fileName, const Song &song);
    extern Update update(const QString &fileName, const Song &from, const Song &to, int id3Ver=-1, bool saveComment=false);
    extern ReplayGain readReplaygain(const QString &fileName);
    extern Update updateReplaygain(const QString &fileName, const ReplayGain &rg);
    extern Update embedImage(const QString &fileName, const QByteArray &cover);
    extern QString oggMimeType(const QString &fileName);
    extern int readRating(const QString &fileName);
    extern Update updateRating(const QString &fileName, int rating);
    extern QMap<QString, QString> readAll(const QString &fileName);
    #endif
    extern QString id3Genre(int id);
}

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
