/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "tagreader.h"
#include "filetyperesolver.h"
#include <QtCore/QFile>
#include <QtCore/QTextCodec>
#include <taglib/fileref.h>
#include <taglib/aifffile.h>
#include <taglib/asffile.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/oggfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/rifffile.h>
#include <taglib/speexfile.h>
#include <taglib/trueaudiofile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>
#ifdef TAGLIB_EXTRAS_FOUND
#include <taglib-extras/audiblefiletyperesolver.h>
#include <taglib-extras/realmediafiletyperesolver.h>
#endif

namespace TagReader
{

static TagLib::FileRef getFileRef(const QString &path)
{
#ifdef Q_OS_WIN32
    const wchar_t *encodedName = reinterpret_cast< const wchar_t * >(path.utf16());
#else
#ifdef COMPLEX_TAGLIB_FILENAME
    const wchar_t *encodedName = reinterpret_cast< const wchar_t * >(path.utf16());
#else
    QByteArray fileName = QFile::encodeName(path);
    const char *encodedName = fileName.constData(); // valid as long as fileName exists
#endif
#endif
    return TagLib::FileRef(encodedName, true, TagLib::AudioProperties::Fast);
}

static void ensureFileTypeResolvers()
{
    static bool alreadyAdded = false;
    if(!alreadyAdded) {
        alreadyAdded = true;

#ifdef TAGLIB_FOUND
#ifdef TAGLIB_EXTRAS_FOUND
        TagLib::FileRef::addFileTypeResolver(new AudibleFileTypeResolver);
        TagLib::FileRef::addFileTypeResolver(new RealMediaFileTypeResolver);
#endif
        TagLib::FileRef::addFileTypeResolver(new FileTypeResolver());
#endif
    }
}

static QString tString2QString(const TagLib::String &str)
{
    static QTextCodec *codec = QTextCodec::codecForName( "UTF-8" );

    return codec->toUnicode(str.toCString(true)).trimmed();
}

// static void readID3v1Tags(const TagLib::FileRef fileref, Song &song)
// {
// }

static void readID3v2Tags(const TagLib::FileRef fileref, Song &song)
{
    // TPE2
}

static void readAPETags(const TagLib::FileRef fileref, Song &song)
{
}

static void readVorbisCommentTags(const TagLib::FileRef fileref, Song &song)
{
}

static void readMP4Tags(const TagLib::FileRef fileref, Song &song)
{
}

static void readASFTags(const TagLib::FileRef fileref, Song &song)
{
}

static Song readTags(const TagLib::FileRef fileref)
{
    Song song;
    TagLib::Tag *tag=fileref.tag();
    song.title=tString2QString(tag->title());
    song.artist=tString2QString(tag->artist());
    song.album=tString2QString(tag->album());
    song.genre=tString2QString(tag->genre());
    song.track=tag->track();
    song.year=tag->year();

    if(TagLib::MPEG::File *file = dynamic_cast< TagLib::MPEG::File * >(fileref.file()))
    {
        if(file->ID3v2Tag(false))
            readID3v2Tags(fileref, song);
        else if(file->APETag())
            readAPETags(fileref, song);
//         else if(file->ID3v1Tag())
//             readID3v1Tags(fileref, song);
    }
    else if(TagLib::Ogg::Vorbis::File *file = dynamic_cast< TagLib::Ogg::Vorbis::File * >(fileref.file()))
    {
        if(file->tag())
            readVorbisCommentTags(fileref, song);
    }
    else if(TagLib::Ogg::FLAC::File *file = dynamic_cast< TagLib::Ogg::FLAC::File * >(fileref.file()))
    {
        if(file->tag())
            readVorbisCommentTags(fileref, song);
    }
    else if(TagLib::Ogg::Speex::File *file = dynamic_cast< TagLib::Ogg::Speex::File * >(fileref.file()))
    {
        if(file->tag())
            readVorbisCommentTags(fileref, song);
    }
    else if(TagLib::FLAC::File *file = dynamic_cast< TagLib::FLAC::File * >(fileref.file()))
    {
        if(file->xiphComment())
            readVorbisCommentTags(fileref, song);
        else if(file->ID3v2Tag())
            readID3v2Tags(fileref, song);
//         else if(file->ID3v1Tag())
//             readID3v1Tags(fileref, song);
    }
    else if(TagLib::MP4::File *file = dynamic_cast< TagLib::MP4::File * >(fileref.file()))
    {
        TagLib::MP4::Tag *tag = dynamic_cast< TagLib::MP4::Tag * >(file->tag());
        if(tag)
            readMP4Tags(fileref, song);
    }
    else if(TagLib::MPC::File *file = dynamic_cast< TagLib::MPC::File * >(fileref.file()))
    {
        if(file->APETag(false))
            readAPETags(fileref, song);
//         else if(file->ID3v1Tag())
//             readID3v1Tags(fileref, song);
    }
    else if(TagLib::RIFF::AIFF::File *file = dynamic_cast< TagLib::RIFF::AIFF::File * >(fileref.file()))
    {
        if(file->tag())
            readID3v2Tags(fileref, song);
    }
    else if(TagLib::RIFF::WAV::File *file = dynamic_cast< TagLib::RIFF::WAV::File * >(fileref.file()))
    {
        if(file->tag())
            readID3v2Tags(fileref, song);
    }
    else if(TagLib::ASF::File *file = dynamic_cast< TagLib::ASF::File * >(fileref.file()))
    {
        TagLib::ASF::Tag *tag = dynamic_cast< TagLib::ASF::Tag * >(file->tag());
        if(tag)
            readASFTags(fileref, song);
    }
    else if(TagLib::TrueAudio::File *file = dynamic_cast< TagLib::TrueAudio::File * >(fileref.file()))
    {
        if(file->ID3v2Tag(false))
            readID3v2Tags(fileref, song);
//         else if(file->ID3v1Tag())
//             readID3v1Tags(fileref, song);
    }
    else if(TagLib::WavPack::File *file = dynamic_cast< TagLib::WavPack::File * >(fileref.file()))
    {
        if(file->APETag(false))
            readAPETags(fileref, song);
//         else if(file->ID3v1Tag())
//             readID3v1Tags(fileref, song);
    }

    return song;
}

Song read(const QString &fileName)
{
    ensureFileTypeResolvers();

    Song song;
    TagLib::FileRef fileref = getFileRef(fileName);

    if (fileref.isNull()) {
        return song;
    }

    song=readTags(fileref);
    song.file=fileName;
    song.time=fileref.audioProperties() ? (fileref.audioProperties()->length() * 1000) : 0;
    if (!song.albumartist.isEmpty() && song.albumartist != song.artist) {
        song.modifiedtitle = song.artist + QLatin1String(" - ") + song.title;
    }

    return song;
}

}
