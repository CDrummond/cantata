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

#include "tags.h"
#include "filetyperesolver.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <ios>
#include <QtCore/QFile>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QString>
#include <QtCore/QStringList>
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
#include <taglib/id3v1tag.h>
#include <taglib/mp4tag.h>
#include <taglib/xiphcomment.h>
#include <taglib/textidentificationframe.h>
#include <taglib/relativevolumeframe.h>
#ifdef TAGLIB_EXTRAS_FOUND
#include <taglib-extras/audiblefiletyperesolver.h>
#include <taglib-extras/realmediafiletyperesolver.h>
#endif

namespace Tags
{

struct ReplayGain
{
    ReplayGain(double tg, double ag, double tp, double ap)
        : trackGain(tg)
        , albumGain(ag)
        , trackPeak(tp)
        , albumPeak(ap)
        , albumMode(true)
        , null(false) {
    }
    ReplayGain()
        : null(true) {
    }
    double trackGain;
    double albumGain;
    double trackPeak;
    double albumPeak;
    bool albumMode;
    bool null;
};

struct ReplayGainStrings
{
    ReplayGainStrings(const ReplayGain &rg) {
        std::stringstream ss;
        ss.precision(2);
        ss << std::fixed;
        ss << rg.albumGain << " dB"; albumGain = ss.str(); ss.str(std::string()); ss.clear();
        ss << rg.trackGain << " dB"; trackGain = ss.str(); ss.str(std::string()); ss.clear();
        ss.precision(6);
        ss << rg.albumPeak; ss >> albumPeak; ss.str(std::string()); ss.clear();
        ss << rg.trackPeak; ss >> trackPeak; ss.str(std::string()); ss.clear();
    }
    std::string trackGain, trackPeak, albumGain, albumPeak;
};

static TagLib::FileRef getFileRef(const QString &path)
{
    #ifdef Q_OS_WIN32
    const wchar_t *encodedName = reinterpret_cast< const wchar_t * >(path.utf16());
    #elif defined COMPLEX_TAGLIB_FILENAME
    const wchar_t *encodedName = reinterpret_cast< const wchar_t * >(path.utf16());
    #else
    QByteArray fileName = QFile::encodeName(path);
    const char *encodedName = fileName.constData(); // valid as long as fileName exists
    #endif
    return TagLib::FileRef(encodedName, true, TagLib::AudioProperties::Fast);
}

static void ensureFileTypeResolvers()
{
    static bool alreadyAdded = false;
    if (!alreadyAdded) {
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
    static QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    return codec->toUnicode(str.toCString(true)).trimmed();
}

TagLib::String qString2TString(const QString &str)
{
    QString val = str.trimmed();
    return val.isEmpty() ? TagLib::String::null : TagLib::String(val.toUtf8().data(), TagLib::String::UTF8);
}

static QPair<int, int> splitDiscNumber(const QString &value)
{
    int disc;
    int count = 0;
    if (-1!=value.indexOf('/')) {
        QStringList list = value.split('/', QString::SkipEmptyParts);
        disc = list.value(0).toInt();
        count = list.value(1).toInt();
    } else if (-1!=value.indexOf(':')) {
        QStringList list = value.split(':', QString::SkipEmptyParts);
        disc = list.value(0).toInt();
        count = list.value(1).toInt();
    } else {
        disc = value.toInt();
    }

    return qMakePair(disc, count);
}

// -- taken from rgtag.cpp from libebur128 -- START
static bool clearTxxxTag(TagLib::ID3v2::Tag* tag, TagLib::String tagName) {
    TagLib::ID3v2::FrameList l = tag->frameList("TXXX");
    for (TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
        TagLib::ID3v2::UserTextIdentificationFrame *fr=dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);
        if (fr && fr->description().upper() == tagName) {
            tag->removeFrame(fr);
            return true;
        }
    }
    return false;
}

static bool clearRva2Tag(TagLib::ID3v2::Tag* tag, TagLib::String tagName) {
    TagLib::ID3v2::FrameList l = tag->frameList("RVA2");
    for (TagLib::ID3v2::FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
        TagLib::ID3v2::RelativeVolumeFrame *fr=dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*it);
        if (fr && fr->identification().upper() == tagName) {
            tag->removeFrame(fr);
            return true;
        }
    }
  return false;
}

static void setTxxxTag(TagLib::ID3v2::Tag* tag, const std::string &tagName, const std::string &value) {
    TagLib::ID3v2::UserTextIdentificationFrame *frame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, tagName);
    if (!frame) {
        frame = new TagLib::ID3v2::UserTextIdentificationFrame;
        frame->setDescription(tagName);
        tag->addFrame(frame);
    }
    frame->setText(value);
}

static void setRva2Tag(TagLib::ID3v2::Tag* tag, const std::string &tagName, double gain, double peak) {
    TagLib::ID3v2::RelativeVolumeFrame *frame = NULL;
    TagLib::ID3v2::FrameList frameList = tag->frameList("RVA2");
    TagLib::ID3v2::FrameList::ConstIterator it = frameList.begin();
    for (; it != frameList.end(); ++it) {
        TagLib::ID3v2::RelativeVolumeFrame *fr=dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*it);
        if (fr->identification() == tagName) {
            frame = fr;
            break;
        }
    }
    if (!frame) {
        frame = new TagLib::ID3v2::RelativeVolumeFrame;
        frame->setIdentification(tagName);
        tag->addFrame(frame);
    }
    frame->setChannelType(TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);
    frame->setVolumeAdjustment(float(gain));

    TagLib::ID3v2::RelativeVolumeFrame::PeakVolume peakVolume;
    peakVolume.bitsRepresentingPeak = 16;
    double ampPeak = peak * 32768.0 > 65535.0 ? 65535.0 : peak * 32768.0;
    unsigned int ampPeakInt = static_cast<unsigned int>(std::ceil(ampPeak));
    TagLib::ByteVector bv_uint = TagLib::ByteVector::fromUInt(ampPeakInt);
    peakVolume.peakVolume = TagLib::ByteVector(&(bv_uint.data()[2]), 2);
    frame->setPeakVolume(peakVolume);
}
// -- taken from rgtag.cpp from libebur128 -- END

static void readID3v2Tags(TagLib::ID3v2::Tag *tag, Song &song)
{
    const TagLib::ID3v2::FrameList &albumArtist = tag->frameListMap()["TPE2"];

    if (!albumArtist.isEmpty()) {
        song.albumartist=tString2QString(albumArtist.front()->toString());
    }

    const TagLib::ID3v2::FrameList &disc = tag->frameListMap()["TPOS"];

    if (!disc.isEmpty()) {
        song.disc=splitDiscNumber(tString2QString(disc.front()->toString())).first;
    }
}

static bool updateID3v2Tag(TagLib::ID3v2::Tag *tag, const char *tagName, const QString &value)
{
    const TagLib::ID3v2::FrameList &frameList = tag->frameListMap()[tagName];

    if (value.isEmpty()) {
        if (!frameList.isEmpty()) {
            tag->removeFrames(tagName);
            return true;
        }
    } else {
        TagLib::ID3v2::TextIdentificationFrame *frame=frameList.isEmpty()
                                ? 0 : dynamic_cast<TagLib::ID3v2::TextIdentificationFrame *>(frameList.front());

        if (!frame) {
            frame = new TagLib::ID3v2::TextIdentificationFrame(tagName);
            tag->addFrame(frame);
        }
        frame->setText(qString2TString(value));
        return true;
    }
    return false;
}

static bool writeID3v2Tags(TagLib::ID3v2::Tag *tag, const Song &from, const Song &to, const ReplayGain &rg)
{
    bool changed=false;

    if (!from.isEmpty() && !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateID3v2Tag(tag, "TPE2", to.albumartist)) {
            changed=true;
        }
        if (from.disc!=to.disc&& updateID3v2Tag(tag, "TPOS", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
    }

    if (!rg.null) {
        ReplayGainStrings rgs(rg);
        while (clearTxxxTag(tag, TagLib::String("replaygain_albumGain").upper()));
        while (clearTxxxTag(tag, TagLib::String("replaygain_albumPeak").upper()));
        while (clearRva2Tag(tag, TagLib::String("album").upper()));
        while (clearTxxxTag(tag, TagLib::String("replaygain_trackGain").upper()));
        while (clearTxxxTag(tag, TagLib::String("replaygain_trackPeak").upper()));
        while (clearRva2Tag(tag, TagLib::String("track").upper()));
        setTxxxTag(tag, "replaygain_trackGain", rgs.trackGain);
        setTxxxTag(tag, "replaygain_trackPeak", rgs.trackPeak);
        setRva2Tag(tag, "track", rg.trackGain, rg.trackPeak);
        if (rg.albumMode) {
            setTxxxTag(tag, "replaygain_albumGain", rgs.albumGain);
            setTxxxTag(tag, "replaygain_albumPeak", rgs.albumPeak);
            setRva2Tag(tag, "album", rg.albumGain, rg.albumPeak);
        }
        changed=true;
    }
    return changed;
}

static void readAPETags(TagLib::APE::Tag *tag, Song &song)
{
    const TagLib::APE::ItemListMap &map = tag->itemListMap();

    if (map.contains("Album Artist")) {
        song.albumartist=tString2QString(map["Album Artist"].toString());
    }

    if (map.contains("Disc")) {
        song.disc=splitDiscNumber(tString2QString(map["Disc"].toString())).first;
    }
}

static bool updateAPETag(TagLib::APE::Tag *tag, const char *tagName, const QString &value)
{
    const TagLib::APE::ItemListMap &map = tag->itemListMap();

    if (value.isEmpty()) {
        if (map.contains(tagName)) {
            tag->removeItem(tagName);
            return true;
        }
    } else {
        tag->addValue(tagName, qString2TString(value), true);
        return true;
    }
    return false;
}

static bool writeAPETags(TagLib::APE::Tag *tag, const Song &from, const Song &to, const ReplayGain &rg)
{
    bool changed=false;

    if (!from.isEmpty() && !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateAPETag(tag, "Album Artist", to.albumartist)) {
            changed=true;
        }
        if (from.disc!=to.disc && updateAPETag(tag, "Disc", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
    }

    if (!rg.null) {
        ReplayGainStrings rgs(rg);
        tag->addValue("replaygain_track_gain", rgs.trackGain);
        tag->addValue("replaygain_track_peak", rgs.trackPeak);
        if (rg.albumMode) {
            tag->addValue("replaygain_album_gain", rgs.albumGain);
            tag->addValue("replaygain_album_peak", rgs.albumPeak);
        } else {
            tag->removeItem("replaygain_album_gain");
            tag->removeItem("replaygain_album_peak");
        }
        changed=true;
    }
    return changed;
}

static void readVorbisCommentTags(TagLib::Ogg::XiphComment *tag, Song &song)
{
    if (tag->contains("ALBUMARTIST")) {
        const TagLib::StringList &list = tag->fieldListMap()["ALBUMARTIST"];

        if (!list.isEmpty()) {
            song.albumartist=tString2QString(list.front());
        }
    }

    if (tag->contains("DISCNUMBER")) {
        const TagLib::StringList &list = tag->fieldListMap()["DISCNUMBER"];

        if (!list.isEmpty()) {
            song.disc=splitDiscNumber(tString2QString(list.front())).first;
        }
    }
}

static bool updateVorbisCommentTag(TagLib::Ogg::XiphComment *tag, const char *tagName, const QString &value)
{
    if (value.isEmpty()) {
        const TagLib::StringList &list = tag->fieldListMap()[tagName];
        if (!list.isEmpty()) {
            tag->removeField(tagName);
            return true;
        }
    } else {
        tag->addField(tagName, qString2TString(value), true);
        return true;
    }
    return false;
}

static bool writeVorbisCommentTags(TagLib::Ogg::XiphComment *tag, const Song &from, const Song &to, const ReplayGain &rg)
{
    bool changed=false;

    if (!from.isEmpty() && !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateVorbisCommentTag(tag, "ALBUMARTIST", to.albumartist)) {
            changed=true;
        }
        if (from.disc!=to.disc && updateVorbisCommentTag(tag, "DISCNUMBER", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
    }

    if (!rg.null) {
        ReplayGainStrings rgs(rg);
        tag->addField("REPLAYGAIN_TRACK_GAIN", rgs.trackGain);
        tag->addField("REPLAYGAIN_TRACK_PEAK", rgs.trackPeak);
        if (rg.albumMode) {
            tag->addField("REPLAYGAIN_ALBUM_GAIN", rgs.albumGain);
            tag->addField("REPLAYGAIN_ALBUM_PEAK", rgs.albumPeak);
        } else {
            tag->removeField("REPLAYGAIN_ALBUM_GAIN");
            tag->removeField("REPLAYGAIN_ALBUM_PEAK");
        }
        changed=true;
    }
    return changed;
}

static void readMP4Tags(TagLib::MP4::Tag *tag, Song &song)
{
    TagLib::MP4::ItemListMap &map = tag->itemListMap();

    if (map.contains("aART") && !map["aART"].toStringList().isEmpty()) {
        song.albumartist=tString2QString(map["aART"].toStringList().front());
    }
    if (map.contains("disk")) {
        song.disc=map["disk"].toIntPair().first;
    }
}

static bool updateMP4Tag(TagLib::MP4::Tag *tag, const char *tagName, const QString &value)
{
    TagLib::MP4::ItemListMap &map = tag->itemListMap();

    if (value.isEmpty()) {
        if (map.contains(tagName)) {
            map.erase(tagName);
            return true;
        }
    } else {
        map.insert("aART", TagLib::StringList(qString2TString(value)));
        return true;
    }
    return false;
}

static bool writeMP4Tags(TagLib::MP4::Tag *tag, const Song &from, const Song &to, const ReplayGain &rg)
{
    bool changed=false;

    if (!from.isEmpty() && !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateMP4Tag(tag, "aART", to.albumartist)) {
            changed=true;
        }
        if (from.disc!=to.disc && updateMP4Tag(tag, "disk", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
    }

    if (!rg.null) {
        ReplayGainStrings rgs(rg);
        TagLib::MP4::ItemListMap &map = tag->itemListMap();
        map["----:com.apple.iTunes:replaygain_track_gain"] = TagLib::MP4::Item(TagLib::StringList(rgs.trackGain));
        map["----:com.apple.iTunes:replaygain_track_peak"] = TagLib::MP4::Item(TagLib::StringList(rgs.trackPeak));
        if (rg.albumMode) {
            map["----:com.apple.iTunes:replaygain_album_gain"] = TagLib::MP4::Item(TagLib::StringList(rgs.albumGain));
            map["----:com.apple.iTunes:replaygain_album_peak"] = TagLib::MP4::Item(TagLib::StringList(rgs.albumPeak));
        } else {
            map.erase("----:com.apple.iTunes:replaygain_album_gain");
            map.erase("----:com.apple.iTunes:replaygain_album_peak");
        }
        changed=true;
    }
    return changed;
}

static void readASFTags(TagLib::ASF::Tag *tag, Song &song)
{
    const TagLib::ASF::AttributeListMap &map = tag->attributeListMap();

    if (map.contains("WM/AlbumTitle") && !map["WM/AlbumTitle"].isEmpty()) {
        song.albumartist=tString2QString(map["WM/AlbumTitle"].front().toString());
    }

    if (map.contains("WM/PartOfSet")) {
        song.albumartist=map["WM/PartOfSet"].front().toUInt();
    }
}

static bool updateASFTag(TagLib::ASF::Tag *tag, const char *tagName, const QString &value)
{
    const TagLib::ASF::AttributeListMap &map = tag->attributeListMap();

    if (value.isEmpty()) {
        if (map.contains(tagName)) {
            tag->removeItem(tagName);
            return true;
        }
    } else {
        tag->setAttribute(tagName, qString2TString(value));
        return true;
    }
    return false;
}

static bool writeASFTags(TagLib::ASF::Tag *tag, const Song &from, const Song &to, const ReplayGain &rg)
{
    Q_UNUSED(rg)
    bool changed=false;

    if (!from.isEmpty() && !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateASFTag(tag, "WM/AlbumTitle", to.albumartist)) {
            changed=true;
        }
        if (from.disc!=to.disc && updateASFTag(tag, "WM/PartOfSet", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
    }
    return changed;
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

    if (TagLib::MPEG::File *file = dynamic_cast< TagLib::MPEG::File * >(fileref.file())) {
        if (file->ID3v2Tag()) {
            readID3v2Tags(file->ID3v2Tag(), song);
        } else if (file->APETag()) {
            readAPETags(file->APETag(), song);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    } else if (TagLib::Ogg::Vorbis::File *file = dynamic_cast< TagLib::Ogg::Vorbis::File * >(fileref.file()))  {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song);
        }
    } else if (TagLib::Ogg::FLAC::File *file = dynamic_cast< TagLib::Ogg::FLAC::File * >(fileref.file())) {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song);
        }
    } else if (TagLib::Ogg::Speex::File *file = dynamic_cast< TagLib::Ogg::Speex::File * >(fileref.file())) {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song);
        }
    } else if (TagLib::FLAC::File *file = dynamic_cast< TagLib::FLAC::File * >(fileref.file())) {
        if (file->xiphComment()) {
            readVorbisCommentTags(file->xiphComment(), song);
        } else if (file->ID3v2Tag()) {
            readID3v2Tags(file->ID3v2Tag(), song);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    } else if (TagLib::MP4::File *file = dynamic_cast< TagLib::MP4::File * >(fileref.file())) {
        TagLib::MP4::Tag *tag = dynamic_cast< TagLib::MP4::Tag * >(file->tag());
        if (tag) {
            readMP4Tags(tag, song);
        }
    } else if (TagLib::MPC::File *file = dynamic_cast< TagLib::MPC::File * >(fileref.file())) {
        if (file->APETag()) {
            readAPETags(file->APETag(), song);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    } else if (TagLib::RIFF::AIFF::File *file = dynamic_cast< TagLib::RIFF::AIFF::File * >(fileref.file())) {
        if (file->tag()) {
            readID3v2Tags(file->tag(), song);
        }
    } else if (TagLib::RIFF::WAV::File *file = dynamic_cast< TagLib::RIFF::WAV::File * >(fileref.file())) {
        if (file->tag()) {
            readID3v2Tags(file->tag(), song);
        }
    } else if (TagLib::ASF::File *file = dynamic_cast< TagLib::ASF::File * >(fileref.file())) {
        TagLib::ASF::Tag *tag = dynamic_cast< TagLib::ASF::Tag * >(file->tag());
        if (tag) {
            readASFTags(tag, song);
        }
    } else if (TagLib::TrueAudio::File *file = dynamic_cast< TagLib::TrueAudio::File * >(fileref.file())) {
        if (file->ID3v2Tag(false)) {
            readID3v2Tags(file->ID3v2Tag(false), song);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    } else if (TagLib::WavPack::File *file = dynamic_cast< TagLib::WavPack::File * >(fileref.file())) {
        if (file->APETag()) {
            readAPETags(file->APETag(), song);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song);
        }
    }

    return song;
}

static bool writeTags(const TagLib::FileRef fileref, const Song &from, const Song &to, const ReplayGain &rg)
{
    bool changed=false;
    TagLib::Tag *tag=fileref.tag();
    if (!from.isEmpty() && !to.isEmpty()) {
        if (from.title!=to.title) {
            tag->setTitle(qString2TString(to.title));
            changed=true;
        }
        if (from.artist!=to.artist) {
            tag->setArtist(qString2TString(to.artist));
            changed=true;
        }
        if (from.album!=to.album) {
            tag->setAlbum(qString2TString(to.album));
            changed=true;
        }
        if (from.genre!=to.genre) {
            tag->setGenre(qString2TString(to.genre));
            changed=true;
        }
        if (from.track!=to.track) {
            tag->setTrack(to.track);
            changed=true;
        }
        if (from.year!=to.year) {
            tag->setYear(to.year);
            changed=true;
        }
    }
    if (TagLib::MPEG::File *file = dynamic_cast< TagLib::MPEG::File * >(fileref.file())) {
        if (file->ID3v2Tag()) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg) || changed;
        } else if (file->APETag()) {
            changed=writeAPETags(file->APETag(), from, to, rg) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg) || changed;
        } else if (file->ID3v2Tag(true)) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg) || changed;
        }
    } else if (TagLib::Ogg::Vorbis::File *file = dynamic_cast< TagLib::Ogg::Vorbis::File * >(fileref.file()))  {
        if (file->tag()) {
            changed=writeVorbisCommentTags(file->tag(), from, to, rg) || changed;
        }
    } else if (TagLib::Ogg::FLAC::File *file = dynamic_cast< TagLib::Ogg::FLAC::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeVorbisCommentTags(file->tag(), from, to, rg) || changed;
        }
    } else if (TagLib::Ogg::Speex::File *file = dynamic_cast< TagLib::Ogg::Speex::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeVorbisCommentTags(file->tag(), from, to, rg) || changed;
        }
    } else if (TagLib::FLAC::File *file = dynamic_cast< TagLib::FLAC::File * >(fileref.file())) {
        if (file->xiphComment()) {
            changed=writeVorbisCommentTags(file->xiphComment(), from, to, rg) || changed;
        } else if (file->ID3v2Tag()) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg) || changed;
        } else if (file->ID3v2Tag(true)) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg) || changed;
        }
    } else if (TagLib::MP4::File *file = dynamic_cast< TagLib::MP4::File * >(fileref.file())) {
        TagLib::MP4::Tag *tag = dynamic_cast<TagLib::MP4::Tag * >(file->tag());
        if (tag) {
            changed=writeMP4Tags(tag, from, to, rg) || changed;
        }
    } else if (TagLib::MPC::File *file = dynamic_cast< TagLib::MPC::File * >(fileref.file())) {
        if (file->APETag(true)) {
            changed=writeAPETags(file->APETag(), from, to, rg) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg) || changed;
        }
    } else if (TagLib::RIFF::AIFF::File *file = dynamic_cast< TagLib::RIFF::AIFF::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeID3v2Tags(file->tag(), from, to, rg) || changed;
        }
    } else if (TagLib::RIFF::WAV::File *file = dynamic_cast< TagLib::RIFF::WAV::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeID3v2Tags(file->tag(), from, to, rg) || changed;
        }
    } else if (TagLib::ASF::File *file = dynamic_cast< TagLib::ASF::File * >(fileref.file())) {
        TagLib::ASF::Tag *tag = dynamic_cast< TagLib::ASF::Tag * >(file->tag());
        if (tag) {
            changed=writeASFTags(tag, from, to, rg) || changed;
        }
    } else if (TagLib::TrueAudio::File *file = dynamic_cast< TagLib::TrueAudio::File * >(fileref.file())) {
        if (file->ID3v2Tag(true)) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg) || changed;
        }
    } else if (TagLib::WavPack::File *file = dynamic_cast< TagLib::WavPack::File * >(fileref.file())) {
        if (file->APETag(true)) {
            changed=writeAPETags(file->APETag(), from, to, rg) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg) || changed;
        }
    }

    return changed;
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
    song.time=fileref.audioProperties() ? fileref.audioProperties()->length() : 0;
    if (!song.albumartist.isEmpty() && song.albumartist != song.artist) {
        song.modifiedtitle = song.artist + QLatin1String(" - ") + song.title;
    }

    return song;
}

Update updateArtistAndTitle(const QString &fileName, const Song &song)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    TagLib::FileRef fileref = getFileRef(fileName);

    if (fileref.isNull()) {
        return Update_Failed;
    }

    TagLib::MPEG::File *mpeg=dynamic_cast<TagLib::MPEG::File *>(fileref.file());
    TagLib::ID3v1::Tag *v1=mpeg ? mpeg->ID3v1Tag(false) : 0;
    bool haveV1=v1 && (!v1->title().isEmpty() || !v1->artist().isEmpty() || !v1->album().isEmpty());
    TagLib::Tag *tag=fileref.tag();
    tag->setTitle(qString2TString(song.title));
    tag->setArtist(qString2TString(song.artist));

    if (mpeg && !haveV1) {
        return mpeg->save(TagLib::MPEG::File::ID3v2) ? Update_Modified : Update_Failed;
    }
    return fileref.file()->save() ? Update_Modified : Update_Failed;
}

Update update(const QString &fileName, const Song &from, const Song &to)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    TagLib::FileRef fileref = getFileRef(fileName);

    if (fileref.isNull()) {
        return Update_Failed;
    }

    TagLib::MPEG::File *mpeg=dynamic_cast<TagLib::MPEG::File *>(fileref.file());
    TagLib::ID3v1::Tag *v1=mpeg ? mpeg->ID3v1Tag(false) : 0;
    bool haveV1=v1 && (!v1->title().isEmpty() || !v1->artist().isEmpty() || !v1->album().isEmpty());

    if (writeTags(fileref, from, to, ReplayGain())) {
        if (mpeg && !haveV1) {
            return mpeg->save(TagLib::MPEG::File::ID3v2) ? Update_Modified : Update_Failed;
        }
        return fileref.file()->save() ? Update_Modified : Update_Failed;
    }
    return Update_None;
}

Update updateReplaygain(const QString &fileName, double trackGain, double trackPeak, double albumGain, double albumPeak)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    TagLib::FileRef fileref = getFileRef(fileName);

    if (fileref.isNull()) {
        return Update_Failed;
    }

    TagLib::MPEG::File *mpeg=dynamic_cast<TagLib::MPEG::File *>(fileref.file());
    TagLib::ID3v1::Tag *v1=mpeg ? mpeg->ID3v1Tag(false) : 0;
    bool haveV1=v1 && (!v1->title().isEmpty() || !v1->artist().isEmpty() || !v1->album().isEmpty());

    if (writeTags(fileref, Song(), Song(), ReplayGain(trackGain, trackPeak, albumGain, albumPeak))) {
        if (mpeg && !haveV1) {
            return mpeg->save(TagLib::MPEG::File::ID3v2) ? Update_Modified : Update_Failed;
        }
        return fileref.file()->save() ? Update_Modified : Update_Failed;
    }
    return Update_None;
}

}
