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

#include "tags.h"
#include "config.h"
#include "filetyperesolver.h"
#include "utils.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <ios>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QTextCodec>
#include <taglib/fileref.h>
#include <taglib/aifffile.h>
#ifdef TAGLIB_ASF_FOUND
#include <taglib/asffile.h>
#endif
#include <taglib/flacfile.h>
#ifdef TAGLIB_MP4_FOUND
#include <taglib/mp4file.h>
#endif
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
#ifdef TAGLIB_ASF_FOUND
#include <taglib/asftag.h>
#endif
#include <taglib/apetag.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v1tag.h>
#ifdef TAGLIB_MP4_FOUND
#include <taglib/mp4tag.h>
#endif
#include <taglib/xiphcomment.h>
#include <taglib/textidentificationframe.h>
#include <taglib/relativevolumeframe.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#ifdef TAGLIB_EXTRAS_FOUND
#include <taglib-extras/audiblefiletyperesolver.h>
#include <taglib-extras/realmediafiletyperesolver.h>
#endif

namespace Tags
{

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

static double parseRgString(const TagLib::String &str) {
    if (str.isEmpty()) {
        return 0.0;
    }

    QString s=tString2QString(str);
    s.remove(QLatin1String(" dB"), Qt::CaseInsensitive);

    if (str.isEmpty()) {
        return 0.0;
    }

    bool ok=false;
    double v=s.toDouble(&ok);

    return ok ? v : 0.0;
}

bool ReplayGain::isEmpty() const
{
    return Utils::equal(trackGain, 0.0) && Utils::equal(trackPeak, 0.0) && Utils::equal(albumGain, 0.0) && Utils::equal(albumPeak, 0.0);
}

struct RgTags : public ReplayGain
{
    RgTags(const ReplayGain &r)
        : ReplayGain(r)
        , albumMode(true)
        , null(false) {
    }
    RgTags()
        : albumMode(false)
        , null(true) {
    }
    bool albumMode;
    bool null;
};

struct RgTagsStrings
{
    RgTagsStrings(const RgTags &rg) {
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

        #ifdef TAGLIB_EXTRAS_FOUND
        TagLib::FileRef::addFileTypeResolver(new AudibleFileTypeResolver);
        TagLib::FileRef::addFileTypeResolver(new RealMediaFileTypeResolver);
        #endif
        TagLib::FileRef::addFileTypeResolver(new Meta::Tag::FileTypeResolver());
    }
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
    frame->setVolumeAdjustment(float(gain), TagLib::ID3v2::RelativeVolumeFrame::MasterVolume);

    TagLib::ID3v2::RelativeVolumeFrame::PeakVolume peakVolume;
    peakVolume.bitsRepresentingPeak = 16;
    double ampPeak = peak * 32768.0 > 65535.0 ? 65535.0 : peak * 32768.0;
    unsigned int ampPeakInt = static_cast<unsigned int>(std::ceil(ampPeak));
    TagLib::ByteVector bv_uint = TagLib::ByteVector::fromUInt(ampPeakInt);
    peakVolume.peakVolume = TagLib::ByteVector(&(bv_uint.data()[2]), 2);
    frame->setPeakVolume(peakVolume);
}
// -- taken from rgtag.cpp from libebur128 -- END

static void readID3v2Tags(TagLib::ID3v2::Tag *tag, Song *song, ReplayGain *rg, QImage *img, QString *lyrics)
{
    if (song) {
        const TagLib::ID3v2::FrameList &albumArtist = tag->frameListMap()["TPE2"];

        if (!albumArtist.isEmpty()) {
            song->albumartist=tString2QString(albumArtist.front()->toString());
        }

        const TagLib::ID3v2::FrameList &disc = tag->frameListMap()["TPOS"];

        if (!disc.isEmpty()) {
            song->disc=splitDiscNumber(tString2QString(disc.front()->toString())).first;
        }
    }

    if (rg) {
        const TagLib::ID3v2::FrameList &frames = tag->frameList("TXXX");
        TagLib::ID3v2::FrameList::ConstIterator it = frames.begin();
        TagLib::ID3v2::FrameList::ConstIterator end = frames.end();
        int found=0;

        for (; it != end && found!=0x0F; ++it) {
            TagLib::ID3v2::UserTextIdentificationFrame *fr=dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);
            if (fr) {
                if (fr->description().upper() == "REPLAYGAIN_TRACK_GAIN") {
                    found|=1;
                    rg->trackGain=parseRgString(fr->fieldList().back());
                } else if (fr->description().upper() == "REPLAYGAIN_TRACK_PEAK") {
                    rg->trackPeak=parseRgString(fr->fieldList().back());
                    found|=2;
                } else if (fr->description().upper() == "REPLAYGAIN_ALBUM_GAIN") {
                    rg->albumGain=parseRgString(fr->fieldList().back());
                    found|=4;
                } else if (fr->description().upper() == "REPLAYGAIN_ALBUM_PEAK") {
                    rg->albumPeak=parseRgString(fr->fieldList().back());
                    found|=8;
                }
            }
        }
    }

    if (img) {
        const TagLib::ID3v2::FrameList &frames = tag->frameList("APIC");

        if (!frames.isEmpty()) {
            TagLib::ID3v2::FrameList::ConstIterator it = frames.begin();
            TagLib::ID3v2::FrameList::ConstIterator end = frames.end();
            bool found = false;

            for (; it != end && !found; ++it) {
                TagLib::ID3v2::AttachedPictureFrame *pic=dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(*it);
                if (pic && TagLib::ID3v2::AttachedPictureFrame::FrontCover==pic->type()) {
                    img->loadFromData((const uchar *) pic->picture().data(), pic->picture().size());
                    if (!img->isNull()) {
                        found=true;
                    }
                }
            }

            if (!found) {
                // Just use first image!
                TagLib::ID3v2::AttachedPictureFrame *pic=static_cast<TagLib::ID3v2::AttachedPictureFrame *>(frames.front());
                img->loadFromData((const uchar *) pic->picture().data(), pic->picture().size());
            }
        }
    }

    if (lyrics) {
        const TagLib::ID3v2::FrameList &frames = tag->frameList("USLT");

        if (!frames.isEmpty()) {
            TagLib::ID3v2::FrameList::ConstIterator it = frames.begin();
            TagLib::ID3v2::FrameList::ConstIterator end = frames.end();
            bool found = false;
            for (; it != end && !found; ++it) {
                TagLib::ID3v2::UnsynchronizedLyricsFrame *l=dynamic_cast<TagLib::ID3v2::UnsynchronizedLyricsFrame*>(*it);
                if (l /*&& !l->language().isEmpty() && 0==strcmp(l->language().data(), lyricsLang)*/) {
                    *lyrics=tString2QString(l->toString());
                    found=true;
                }
            }
        }
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

static bool isId3V24(TagLib::ID3v2::Header *header)
{
    return header && header->majorVersion()>3;
}

static bool writeID3v2Tags(TagLib::ID3v2::Tag *tag, const Song &from, const Song &to, const RgTags &rg, const QByteArray &img)
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
        RgTagsStrings rgs(rg);
        while (clearTxxxTag(tag, TagLib::String("replaygain_album_gain").upper()));
        while (clearTxxxTag(tag, TagLib::String("replaygain_album_peak").upper()));
        while (clearRva2Tag(tag, TagLib::String("album").upper()));
        while (clearTxxxTag(tag, TagLib::String("replaygain_track_gain").upper()));
        while (clearTxxxTag(tag, TagLib::String("replaygain_track_peak").upper()));
        while (clearRva2Tag(tag, TagLib::String("track").upper()));
        setTxxxTag(tag, "replaygain_track_gain", rgs.trackGain);
        setTxxxTag(tag, "replaygain_track_peak", rgs.trackPeak);
        if (isId3V24(tag->header())) {
            setRva2Tag(tag, "track", rg.trackGain, rg.trackPeak);
        }
        if (rg.albumMode) {
            setTxxxTag(tag, "replaygain_album_gain", rgs.albumGain);
            setTxxxTag(tag, "replaygain_album_peak", rgs.albumPeak);
            if (isId3V24(tag->header())) {
                setRva2Tag(tag, "album", rg.albumGain, rg.albumPeak);
            }
        }
        changed=true;
    }

    if (!img.isEmpty()) {
        TagLib::ByteVector imgData(img.constData(), img.length());
        TagLib::ID3v2::AttachedPictureFrame *pic=new TagLib::ID3v2::AttachedPictureFrame;
        pic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
        pic->setMimeType("image/jpeg");
        pic->setPicture(imgData);
        tag->addFrame(pic);
        changed=true;
    }

    return changed;
}

static void readAPETags(TagLib::APE::Tag *tag, Song *song, ReplayGain *rg)
{
    const TagLib::APE::ItemListMap &map = tag->itemListMap();

    if (song) {
        if (map.contains("Album Artist")) {
            song->albumartist=tString2QString(map["Album Artist"].toString());
        }

        if (map.contains("Disc")) {
            song->disc=splitDiscNumber(tString2QString(map["Disc"].toString())).first;
        }
    }

    if (rg) {
        if (map.contains("replaygain_track_gain")) {
            rg->trackGain=parseRgString(map["replaygain_track_gain"].toString());
        }
        if (map.contains("replaygain_track_peak")) {
            rg->trackPeak=parseRgString(map["replaygain_track_peak"].toString());
        }
        if (map.contains("replaygain_album_gain")) {
            rg->albumGain=parseRgString(map["replaygain_album_gain"].toString());
        }
        if (map.contains("replaygain_album_peak")) {
            rg->albumPeak=parseRgString(map["replaygain_album_peak"].toString());
        }
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

static bool writeAPETags(TagLib::APE::Tag *tag, const Song &from, const Song &to, const RgTags &rg)
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
        RgTagsStrings rgs(rg);
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

static TagLib::String readVorbisTag(TagLib::Ogg::XiphComment *tag, const char *field)
{
    if (tag->contains(field)) {
        const TagLib::StringList &list = tag->fieldListMap()[field];

        if (!list.isEmpty()) {
            return list.front();
        }
    }

    return TagLib::String();
}

static void readVorbisCommentTags(TagLib::Ogg::XiphComment *tag, Song *song, ReplayGain *rg, QImage *img)
{
    if (song) {
        TagLib::String str=readVorbisTag(tag, "ALBUMARTIST");
        if (str.isEmpty()) {
            song->albumartist=tString2QString(str);
        }

        str=readVorbisTag(tag, "DISCNUMBER");
        if (str.isEmpty()) {
            song->disc=splitDiscNumber(tString2QString(str)).first;
        }
    }

    if (rg) {
        rg->trackGain=parseRgString(readVorbisTag(tag, "REPLAYGAIN_TRACK_GAIN"));
        rg->trackPeak=parseRgString(readVorbisTag(tag, "REPLAYGAIN_TRACK_PEAK"));
        rg->albumGain=parseRgString(readVorbisTag(tag, "REPLAYGAIN_ALBUM_GAIN"));
        rg->albumPeak=parseRgString(readVorbisTag(tag, "REPLAYGAIN_ALBUM_PEAK"));
    }

    if (img) {
        TagLib::Ogg::FieldListMap map = tag->fieldListMap();
        // Ogg lacks a definitive standard for embedding cover art, but it seems
        // b64 encoding a field called COVERART is the general convention
        if (map.contains("COVERART")) {
            QByteArray data=map["COVERART"].toString().toCString();
            img->loadFromData(QByteArray::fromBase64(data));
            if (img->isNull()) {
                img->loadFromData(data); // not base64??
            }
        }
    }
}

#if (TAGLIB_MAJOR_VERSION > 1) || (TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION >= 7)
static void readFlacPicture(const TagLib::List<TagLib::FLAC::Picture*> &pics, QImage *img)
{
    if (!pics.isEmpty() && 1==pics.size()) {
        TagLib::FLAC::Picture *picture = *(pics.begin());
        QByteArray data(picture->data().data(), picture->data().size());
        img->loadFromData(data);
    }
}
#endif

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

static bool writeVorbisCommentTags(TagLib::Ogg::XiphComment *tag, const Song &from, const Song &to, const RgTags &rg, const QByteArray &img)
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
        RgTagsStrings rgs(rg);
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

    if (!img.isEmpty()) {
        #if QT_VERSION < 0x050000
        tag->addField("COVERART", qString2TString(QString::fromAscii(img.toBase64())));
        #else
        tag->addField("COVERART", qString2TString(QString::fromLatin1(img.toBase64())));
        #endif
        changed=true;
    }
    return changed;
}

#ifdef TAGLIB_MP4_FOUND
static void readMP4Tags(TagLib::MP4::Tag *tag, Song *song, ReplayGain *rg, QImage *img)
{
    TagLib::MP4::ItemListMap &map = tag->itemListMap();

    if (song) {
        if (map.contains("aART") && !map["aART"].toStringList().isEmpty()) {
            song->albumartist=tString2QString(map["aART"].toStringList().front());
        }
        if (map.contains("disk")) {
            song->disc=map["disk"].toIntPair().first;
        }
    }
    if (rg) {
        if (map.contains("----:com.apple.iTunes:replaygain_track_gain")) {
            rg->trackGain=parseRgString(map["----:com.apple.iTunes:replaygain_track_gain"].toStringList().front());
        }
        if (map.contains("----:com.apple.iTunes:replaygain_track_peak")) {
            rg->trackPeak=parseRgString(map["----:com.apple.iTunes:replaygain_track_peak"].toStringList().front());
        }
        if (map.contains("----:com.apple.iTunes:replaygain_album_gain")) {
            rg->albumGain=parseRgString(map["----:com.apple.iTunes:replaygain_album_gain"].toStringList().front());
        }
        if (map.contains("----:com.apple.iTunes:replaygain_album_peak")) {
            rg->albumPeak=parseRgString(map["----:com.apple.iTunes:replaygain_album_peak"].toStringList().front());
        }
    }
    if (img) {
        if (map.contains("covr")) {
            TagLib::MP4::Item coverItem = map["covr"];
            TagLib::MP4::CoverArtList coverArtList = coverItem.toCoverArtList();
            if (!coverArtList.isEmpty()) {
                TagLib::MP4::CoverArt coverArt = coverArtList.front();
                img->loadFromData((const uchar *) coverArt.data().data(), coverArt.data().size());
            }
        }
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

static bool writeMP4Tags(TagLib::MP4::Tag *tag, const Song &from, const Song &to, const RgTags &rg, const QByteArray &img)
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
        RgTagsStrings rgs(rg);
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

    if (!img.isEmpty()) {
        TagLib::ByteVector imgData(img.constData(), img.length());
        TagLib::MP4::CoverArt coverArt(TagLib::MP4::CoverArt::JPEG, imgData);
        TagLib::MP4::CoverArtList coverArtList;
        coverArtList.append(coverArt);
        TagLib::MP4::Item coverItem(coverArtList);
        TagLib::MP4::ItemListMap &map = tag->itemListMap();
        map.insert("covr", coverItem);
    }

    return changed;
}
#endif

#ifdef TAGLIB_ASF_FOUND
static void readASFTags(TagLib::ASF::Tag *tag, Song *song, ReplayGain *rg)
{
    Q_UNUSED(rg)

    if (song) {
        const TagLib::ASF::AttributeListMap &map = tag->attributeListMap();

        if (map.contains("WM/AlbumTitle") && !map["WM/AlbumTitle"].isEmpty()) {
            song->albumartist=tString2QString(map["WM/AlbumTitle"].front().toString());
        }

        if (map.contains("WM/PartOfSet")) {
            song->albumartist=map["WM/PartOfSet"].front().toUInt();
        }
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

static bool writeASFTags(TagLib::ASF::Tag *tag, const Song &from, const Song &to, const RgTags &rg)
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
#endif

static void readTags(const TagLib::FileRef fileref, Song *song, ReplayGain *rg, QImage *img, QString *lyrics)
{
    TagLib::Tag *tag=fileref.tag();
    if (song) {
        song->title=tString2QString(tag->title());
        song->artist=tString2QString(tag->artist());
        song->album=tString2QString(tag->album());
        song->genre=tString2QString(tag->genre());
        song->track=tag->track();
        song->year=tag->year();
    }

    if (TagLib::MPEG::File *file = dynamic_cast< TagLib::MPEG::File * >(fileref.file())) {
        if (file->ID3v2Tag()) {
            readID3v2Tags(file->ID3v2Tag(), song, rg, img, lyrics);
        } else if (file->APETag()) {
            readAPETags(file->APETag(), song, rg);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    } else if (TagLib::Ogg::Vorbis::File *file = dynamic_cast< TagLib::Ogg::Vorbis::File * >(fileref.file()))  {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song, rg, img);
        }
    } else if (TagLib::Ogg::FLAC::File *file = dynamic_cast< TagLib::Ogg::FLAC::File * >(fileref.file())) {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song, rg, img);
        }
    } else if (TagLib::Ogg::Speex::File *file = dynamic_cast< TagLib::Ogg::Speex::File * >(fileref.file())) {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song, rg, img);
        }
    } else if (TagLib::FLAC::File *file = dynamic_cast< TagLib::FLAC::File * >(fileref.file())) {
        if (file->xiphComment()) {
            readVorbisCommentTags(file->xiphComment(), song, rg, img);
            #if (TAGLIB_MAJOR_VERSION > 1) || (TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION >= 7)
            if (img && img->isNull()) {
                readFlacPicture(file->pictureList(), img);
            }
            #endif
        } else if (file->ID3v2Tag()) {
            readID3v2Tags(file->ID3v2Tag(), song, rg, img, lyrics);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    #ifdef TAGLIB_MP4_FOUND
    } else if (TagLib::MP4::File *file = dynamic_cast< TagLib::MP4::File * >(fileref.file())) {
        TagLib::MP4::Tag *tag = dynamic_cast< TagLib::MP4::Tag * >(file->tag());
        if (tag) {
            readMP4Tags(tag, song, rg, img);
        }
    #endif
    } else if (TagLib::MPC::File *file = dynamic_cast< TagLib::MPC::File * >(fileref.file())) {
        if (file->APETag()) {
            readAPETags(file->APETag(), song, rg);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    } else if (TagLib::RIFF::AIFF::File *file = dynamic_cast< TagLib::RIFF::AIFF::File * >(fileref.file())) {
        if (file->tag()) {
            readID3v2Tags(file->tag(), song, rg, img, lyrics);
        }
    } else if (TagLib::RIFF::WAV::File *file = dynamic_cast< TagLib::RIFF::WAV::File * >(fileref.file())) {
        if (file->tag()) {
            readID3v2Tags(file->tag(), song, rg, img, lyrics);
        }
    #ifdef TAGLIB_ASF_FOUND
    } else if (TagLib::ASF::File *file = dynamic_cast< TagLib::ASF::File * >(fileref.file())) {
        TagLib::ASF::Tag *tag = dynamic_cast< TagLib::ASF::Tag * >(file->tag());
        if (tag) {
            readASFTags(tag, song, rg);
        }
    #endif
    } else if (TagLib::TrueAudio::File *file = dynamic_cast< TagLib::TrueAudio::File * >(fileref.file())) {
        if (file->ID3v2Tag(false)) {
            readID3v2Tags(file->ID3v2Tag(false), song, rg, img, lyrics);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    } else if (TagLib::WavPack::File *file = dynamic_cast< TagLib::WavPack::File * >(fileref.file())) {
        if (file->APETag()) {
            readAPETags(file->APETag(), song, rg);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    }
}

static bool writeTags(const TagLib::FileRef fileref, const Song &from, const Song &to, const RgTags &rg, const QByteArray &img)
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
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg, img) || changed;
        } else if (file->APETag()) {
            changed=writeAPETags(file->APETag(), from, to, rg) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg, img) || changed;
        } else if (file->ID3v2Tag(true)) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg, img) || changed;
        }
    } else if (TagLib::Ogg::Vorbis::File *file = dynamic_cast< TagLib::Ogg::Vorbis::File * >(fileref.file()))  {
        if (file->tag()) {
            changed=writeVorbisCommentTags(file->tag(), from, to, rg, img) || changed;
        }
    } else if (TagLib::Ogg::FLAC::File *file = dynamic_cast< TagLib::Ogg::FLAC::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeVorbisCommentTags(file->tag(), from, to, rg, img) || changed;
        }
    } else if (TagLib::Ogg::Speex::File *file = dynamic_cast< TagLib::Ogg::Speex::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeVorbisCommentTags(file->tag(), from, to, rg, img) || changed;
        }
    } else if (TagLib::FLAC::File *file = dynamic_cast< TagLib::FLAC::File * >(fileref.file())) {
        if (file->xiphComment()) {
            changed=writeVorbisCommentTags(file->xiphComment(), from, to, rg, img) || changed;
        } else if (file->ID3v2Tag()) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg, img) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg, img) || changed;
        } else if (file->ID3v2Tag(true)) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg, img) || changed;
        }
    #ifdef TAGLIB_MP4_FOUND
    } else if (TagLib::MP4::File *file = dynamic_cast< TagLib::MP4::File * >(fileref.file())) {
        TagLib::MP4::Tag *tag = dynamic_cast<TagLib::MP4::Tag * >(file->tag());
        if (tag) {
            changed=writeMP4Tags(tag, from, to, rg, img) || changed;
        }
    #endif
    } else if (TagLib::MPC::File *file = dynamic_cast< TagLib::MPC::File * >(fileref.file())) {
        if (file->APETag(true)) {
            changed=writeAPETags(file->APETag(), from, to, rg) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg, img) || changed;
        }
    } else if (TagLib::RIFF::AIFF::File *file = dynamic_cast< TagLib::RIFF::AIFF::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeID3v2Tags(file->tag(), from, to, rg, img) || changed;
        }
    } else if (TagLib::RIFF::WAV::File *file = dynamic_cast< TagLib::RIFF::WAV::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeID3v2Tags(file->tag(), from, to, rg, img) || changed;
        }
    #ifdef TAGLIB_ASF_FOUND
    } else if (TagLib::ASF::File *file = dynamic_cast< TagLib::ASF::File * >(fileref.file())) {
        TagLib::ASF::Tag *tag = dynamic_cast< TagLib::ASF::Tag * >(file->tag());
        if (tag) {
            changed=writeASFTags(tag, from, to, rg) || changed;
        }
    #endif
    } else if (TagLib::TrueAudio::File *file = dynamic_cast< TagLib::TrueAudio::File * >(fileref.file())) {
        if (file->ID3v2Tag(true)) {
            changed=writeID3v2Tags(file->ID3v2Tag(), from, to, rg, img) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg, img) || changed;
        }
    } else if (TagLib::WavPack::File *file = dynamic_cast< TagLib::WavPack::File * >(fileref.file())) {
        if (file->APETag(true)) {
            changed=writeAPETags(file->APETag(), from, to, rg) || changed;
//         } else if (file->ID3v1Tag()) {
//             changed=writeID3v1Tags(fileref, from, to, rg, img) || changed;
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

    readTags(fileref, &song, 0, 0, 0);
    song.file=fileName;
    song.time=fileref.audioProperties() ? fileref.audioProperties()->length() : 0;
    return song;
}

QImage readImage(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    QImage img;
    TagLib::FileRef fileref = getFileRef(fileName);

    if (fileref.isNull()) {
        return img;
    }

    readTags(fileref, 0, 0, &img, 0);
    return img;
}

QString readLyrics(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    QString lyrics;
    TagLib::FileRef fileref = getFileRef(fileName);

    if (fileref.isNull()) {
        return lyrics;
    }

    readTags(fileref, 0, 0, 0, &lyrics);
    return lyrics;
}

static Update update(const TagLib::FileRef fileref, const Song &from, const Song &to, const RgTags &rg, const QByteArray &img)
{
    if (writeTags(fileref, from, to, rg, img)) {
        TagLib::MPEG::File *mpeg=dynamic_cast<TagLib::MPEG::File *>(fileref.file());
        if (mpeg) {
            TagLib::ID3v1::Tag *v1=mpeg->ID3v1Tag(false);
            TagLib::ID3v2::Tag *v2=mpeg->ID3v2Tag(false);
            bool haveV1=v1 && (!v1->title().isEmpty() || !v1->artist().isEmpty() || !v1->album().isEmpty());
            bool isID3v24=v2 && isId3V24(v2->header());
            return mpeg->save((haveV1 ? TagLib::MPEG::File::ID3v1 : 0)|TagLib::MPEG::File::ID3v2, true, isID3v24 ? 4 : 3) ? Update_Modified : Update_Failed;
        }
        return fileref.file()->save() ? Update_Modified : Update_Failed;
    }
    return Update_None;
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
    TagLib::Tag *tag=fileref.tag();
    tag->setTitle(qString2TString(song.title));
    tag->setArtist(qString2TString(song.artist));

    if (mpeg) {
        TagLib::ID3v1::Tag *v1=mpeg->ID3v1Tag(false);
        TagLib::ID3v2::Tag *v2=mpeg->ID3v2Tag(false);
        bool haveV1=v1 && (!v1->title().isEmpty() || !v1->artist().isEmpty() || !v1->album().isEmpty());
        bool isID3v24=v2 && isId3V24(v2->header());
        return mpeg->save((haveV1 ? TagLib::MPEG::File::ID3v1 : 0)|TagLib::MPEG::File::ID3v2, true, isID3v24 ? 4 : 3) ? Update_Modified : Update_Failed;
    }
    return fileref.file()->save() ? Update_Modified : Update_Failed;
}

Update update(const QString &fileName, const Song &from, const Song &to)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    TagLib::FileRef fileref = getFileRef(fileName);
    return fileref.isNull() ? Update_Failed : update(fileref, from, to, RgTags(), QByteArray());
}

ReplayGain readReplaygain(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    TagLib::FileRef fileref = getFileRef(fileName);

    if (fileref.isNull()) {
        return false;
    }

    ReplayGain rg;
    readTags(fileref, 0, &rg, 0, 0);
    return rg;
}

Update updateReplaygain(const QString &fileName, const ReplayGain &rg)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    TagLib::FileRef fileref = getFileRef(fileName);
    return fileref.isNull() ? Update_Failed : update(fileref, Song(), Song(), RgTags(rg), QByteArray());
}

Update embedImage(const QString &fileName, const QByteArray &cover)
{
    QMutexLocker locker(&mutex);
    ensureFileTypeResolvers();

    TagLib::FileRef fileref = getFileRef(fileName);
    return fileref.isNull() ? Update_Failed : update(fileref, Song(), Song(), RgTags(), cover);
}

}
