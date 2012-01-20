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
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
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
#include <taglib/asftag.h>
#include <taglib/apetag.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4tag.h>
#include <taglib/xiphcomment.h>
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

TagLib::String qString2TString(const QString &str)
{
    QString val = str.trimmed();
    return val.isEmpty() ? TagLib::String::null : TagLib::String(val.toUtf8().data(), TagLib::String::UTF8);
}

// static void readID3v1Tags(const TagLib::FileRef fileref, Song &song)
// {
// }

static void readID3v2Tags(TagLib::ID3v2::Tag *tag, Song &song)
{
    TagLib::ID3v2::FrameList list = tag->frameListMap()["TPE2"];

    if (!list.isEmpty()) {
        song.albumartist=tString2QString(list.front()->toString());
    }
}

static void readAPETags(TagLib::APE::Tag *tag, Song &song)
{
    TagLib::APE::ItemListMap map = tag->itemListMap();

    if (map.contains("Album Artist")) {
        song.albumartist=tString2QString(map["Album Artist"].toString());
    }
}

static void readVorbisCommentTags(TagLib::Ogg::XiphComment *tag, Song &song)
{
    if (!tag->contains("ALBUMARTIST")) {
        return;
    }

    TagLib::StringList list = tag->fieldListMap()["ALBUMARTIST"];

    if (!list.isEmpty()) {
        song.albumartist=tString2QString(list.front());
    }
}

static void readMP4Tags(TagLib::MP4::Tag *tag, Song &song)
{
    TagLib::MP4::ItemListMap map = tag->itemListMap();

    if (map.contains("aART") && !map["aART"].toStringList().isEmpty()) {
        song.albumartist=tString2QString(map["aART"].toStringList().front());
    }
}

static void readASFTags(TagLib::ASF::Tag *tag, Song &song)
{
    TagLib::ASF::AttributeListMap map = tag->attributeListMap();

    if (map.contains("WM/AlbumTitle") && !map["WM/AlbumTitle"].isEmpty()) {
        song.albumartist=tString2QString(map["WM/AlbumTitle"].front().toString());
    }
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

    if(TagLib::MPEG::File *file = dynamic_cast< TagLib::MPEG::File * >(fileref.file())) {
        if(file->ID3v2Tag()) {
            readID3v2Tags(file->ID3v2Tag(), song);
        } else if(file->APETag()) {
            readAPETags(file->APETag(), song);
//         } else if(file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    } else if(TagLib::Ogg::Vorbis::File *file = dynamic_cast< TagLib::Ogg::Vorbis::File * >(fileref.file()))  {
        if(file->tag()) {
            readVorbisCommentTags(file->tag(), song);
        }
    } else if(TagLib::Ogg::FLAC::File *file = dynamic_cast< TagLib::Ogg::FLAC::File * >(fileref.file())) {
        if(file->tag()) {
            readVorbisCommentTags(file->tag(), song);
        }
    } else if(TagLib::Ogg::Speex::File *file = dynamic_cast< TagLib::Ogg::Speex::File * >(fileref.file())) {
        if(file->tag()) {
            readVorbisCommentTags(file->tag(), song);
        }
    } else if(TagLib::FLAC::File *file = dynamic_cast< TagLib::FLAC::File * >(fileref.file())) {
        if(file->xiphComment()) {
            readVorbisCommentTags(file->xiphComment(), song);
        } else if(file->ID3v2Tag()) {
            readID3v2Tags(file->ID3v2Tag(), song);
//         } else if(file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    } else if(TagLib::MP4::File *file = dynamic_cast< TagLib::MP4::File * >(fileref.file())) {
        TagLib::MP4::Tag *tag = dynamic_cast< TagLib::MP4::Tag * >(file->tag());
        if(tag) {
            readMP4Tags(tag, song);
        }
    } else if(TagLib::MPC::File *file = dynamic_cast< TagLib::MPC::File * >(fileref.file())) {
        if(file->APETag()) {
            readAPETags(file->APETag(), song);
//         } else if(file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    } else if(TagLib::RIFF::AIFF::File *file = dynamic_cast< TagLib::RIFF::AIFF::File * >(fileref.file())) {
        if(file->tag()) {
            readID3v2Tags(file->tag(), song);
        }
    } else if(TagLib::RIFF::WAV::File *file = dynamic_cast< TagLib::RIFF::WAV::File * >(fileref.file())) {
        if(file->tag()) {
            readID3v2Tags(file->tag(), song);
        }
    } else if(TagLib::ASF::File *file = dynamic_cast< TagLib::ASF::File * >(fileref.file())) {
        TagLib::ASF::Tag *tag = dynamic_cast< TagLib::ASF::Tag * >(file->tag());
        if(tag) {
            readASFTags(tag, song);
        }
    } else if(TagLib::TrueAudio::File *file = dynamic_cast< TagLib::TrueAudio::File * >(fileref.file())) {
        if(file->ID3v2Tag(false)) {
            readID3v2Tags(file->ID3v2Tag(false), song);
//         } else if(file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    } else if(TagLib::WavPack::File *file = dynamic_cast< TagLib::WavPack::File * >(fileref.file())) {
        if(file->APETag()) {
            readAPETags(file->APETag(), song);
//         } else if(file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    }

    return song;
}

static QMutex mutex;

Song read(const QString &fileName)
{
    QMutexLocker locker(&mutex);

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

bool updateArtistAndTitleTags(const QString &fileName, const Song &song)
{
    QMutexLocker locker(&mutex);

    ensureFileTypeResolvers();

    TagLib::FileRef fileref = getFileRef(fileName);

    if (fileref.isNull()) {
        return false;
    }

    TagLib::Tag *tag=fileref.tag();
    tag->setTitle(qString2TString(song.title));
    tag->setArtist(qString2TString(song.artist));
    TagLib::MPEG::File *mpeg=dynamic_cast<TagLib::MPEG::File *>(fileref.file());

    return mpeg ? mpeg->save(TagLib::MPEG::File::ID3v2) : fileref.file()->save();
}

}
