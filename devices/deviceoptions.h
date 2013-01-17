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

#ifndef DEVICE_OPTIONS_H
#define DEVICE_OPTIONS_H

#include <QtCore/QString>
#include "config.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "encoders.h"
#endif

class Song;

struct DeviceOptions {
    static const QLatin1String constAlbumArtist;
    static const QLatin1String constAlbumTitle;
    static const QLatin1String constTrackArtist;
    static const QLatin1String constTrackTitle;
    static const QLatin1String constTrackNumber;
    static const QLatin1String constCdNumber;
    static const QLatin1String constGenre;
    static const QLatin1String constYear;

    static bool isConfigured(const QString &group, bool isMpd=false);

    #ifdef ENABLE_DEVICES_SUPPORT
    DeviceOptions(const QString &cvrName=QString());
    #else
    DeviceOptions();
    #endif

    void load(const QString &group, bool isMpd=false);
    void save(const QString &group, bool isMpd=false);

    bool operator==(const DeviceOptions &o) const {
        return vfatSafe==o.vfatSafe && asciiOnly==o.asciiOnly && ignoreThe==o.ignoreThe &&
                replaceSpaces==o.replaceSpaces && scheme==o.scheme
                #ifdef ENABLE_DEVICES_SUPPORT
                && coverMaxSize==o.coverMaxSize && coverName==o.coverName
                && fixVariousArtists==o.fixVariousArtists && useCache==o.useCache &&
                transcoderCodec==o.transcoderCodec && autoScan==o.autoScan &&
                (transcoderCodec.isEmpty() || (transcoderValue==o.transcoderValue && transcoderWhenDifferent==o.transcoderWhenDifferent))
                #endif
                ;
    }
    bool operator!=(const DeviceOptions &o) const {
        return !(*this==o);
    }
    QString clean(const QString &str) const;
    Song clean(const Song &s) const;
    QString createFilename(const Song &s) const;
    #ifdef ENABLE_DEVICES_SUPPORT
    void checkCoverSize() {
        if (0==coverMaxSize || coverMaxSize>400) {
            coverMaxSize=0;
        } else {
            coverMaxSize=((unsigned int)(coverMaxSize/100))*100;
        }
    }
    #endif
    QString scheme;
    bool vfatSafe;
    bool asciiOnly;
    bool ignoreThe;
    bool replaceSpaces;
    #ifdef ENABLE_DEVICES_SUPPORT
    bool fixVariousArtists;
    QString transcoderCodec;
    int transcoderValue;
    bool transcoderWhenDifferent;
    bool useCache;
    bool autoScan;
    QString coverName;
    unsigned int coverMaxSize;
    #endif
};

#endif
