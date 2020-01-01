/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/****************************************************************************************
 * Copyright (c) 2005 Martin Aumueller <aumueller@reserv.at>                            *
 * Copyright (c) 2011 Ralf Engels <ralf-engels@gmx.de>                                  *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "filetyperesolver.h"

#include <QFile>
#include <QFileInfo>
#ifdef TAGLIB_EXTRAS_FOUND
#include <taglib-extras/audiblefile.h>
#include <taglib-extras/realmediafile.h>
#endif
#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/asffile.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/oggfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/speexfile.h>
#include <taglib/trueaudiofile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>
#ifdef TAGLIB_OPUS_FOUND
#include <taglib/opusfile.h>
#endif

TagLib::File *Meta::Tag::FileTypeResolver::createFile(TagLib::FileName fileName,
        bool readProperties,
        TagLib::AudioProperties::ReadStyle propertiesStyle) const
{
    TagLib::File* result = nullptr;

    QString fn = QFile::decodeName(fileName);
    QString suffix = QFileInfo(fn).suffix();

    if (suffix == QLatin1String("mp3")) {
        result = new TagLib::MPEG::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("flac")) {
        result = new TagLib::FLAC::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("ogg")) {
        result = new TagLib::Ogg::Vorbis::File(fileName, readProperties, propertiesStyle);
        if (!result->isValid()) {
            delete result;
            result = new TagLib::Ogg::FLAC::File(fileName, readProperties, propertiesStyle);
        }
        if (!result->isValid()) {
            delete result;
            result = new TagLib::TrueAudio::File(fileName, readProperties, propertiesStyle);
        }
        #ifdef TAGLIB_OPUS_FOUND
        if (!result->isValid()) {
            delete result;
            result = new TagLib::Ogg::Opus::File(fileName, readProperties, propertiesStyle);
        }
        #endif
    } else if (suffix == QLatin1String("m4a") || suffix == QLatin1String("m4b")
        || suffix == QLatin1String("m4p") || suffix == QLatin1String("mp4")
        /*|| suffix == QLatin1String("m4v") || suffix == QLatin1String("mp4v") */) {
        result = new TagLib::MP4::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("wav")) {
        result = new TagLib::RIFF::WAV::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("wma") /*|| suffix == QLatin1String("asf")*/) {
        result = new TagLib::ASF::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("wvp") || suffix == QLatin1String("wv")) {
        result = new TagLib::WavPack::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("ape")) {
        result = new TagLib::APE::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("spx")) {
        result = new TagLib::TrueAudio::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("tta")) {
        result = new TagLib::TrueAudio::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("aiff") || suffix == QLatin1String("aif") || suffix == QLatin1String("aifc")) {
        result = new TagLib::RIFF::AIFF::File(fileName, readProperties, propertiesStyle);
    } else if (suffix == QLatin1String("mpc") || suffix == QLatin1String("mpp") || suffix == QLatin1String("mp+")) {
        result = new TagLib::MPC::File(fileName, readProperties, propertiesStyle);
    }
    #ifdef TAGLIB_OPUS_FOUND
    else if (suffix == QLatin1String("opus")) {
        result = new TagLib::Ogg::Opus::File(fileName, readProperties, propertiesStyle);
    }
    #endif

    if (result && !result->isValid()) {
        delete result;
        result = nullptr;
    }

    return result;
}
