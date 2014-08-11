/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/utils.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <ios>
#include <QPair>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextCodec>
#include <QDebug>
#define TAGLIB_VERSION CANTATA_MAKE_VERSION(TAGLIB_MAJOR_VERSION, TAGLIB_MINOR_VERSION, TAGLIB_PATCH_VERSION)

#include <taglib/taglib.h>
#if TAGLIB_VERSION >= CANTATA_MAKE_VERSION(1,8,0)
#include <taglib/tpropertymap.h>
#endif
#include <taglib/fileref.h>
#include <taglib/aifffile.h>
#ifdef TAGLIB_ASF_FOUND
#include <taglib/asffile.h>
#endif
#include <taglib/flacfile.h>
#ifdef TAGLIB_MP4_FOUND
#include <taglib/mp4file.h>
#endif
#ifdef TAGLIB_OPUS_FOUND
#include <taglib/opusfile.h>
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
#include <taglib/id3v1genres.h>
#ifdef TAGLIB_MP4_FOUND
#include <taglib/mp4tag.h>
#endif
#include <taglib/xiphcomment.h>
#include <taglib/textidentificationframe.h>
#include <taglib/relativevolumeframe.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/popularimeterframe.h>
#ifdef TAGLIB_EXTRAS_FOUND
#include <taglib-extras/audiblefiletyperesolver.h>
#include <taglib-extras/realmediafiletyperesolver.h>
#endif

#ifdef ENABLE_EXTERNAL_TAGS
// Mutex is locked in client, and server (that accesses this class) is single threaded
#define LOCK_MUTEX
#else
#include <QMutex>
#include <QMutexLocker>
static QMutex mutex;
#define LOCK_MUTEX QMutexLocker locker(&mutex);
#endif

namespace Tags
{

static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "Tags" << __FUNCTION__

void enableDebug()
{
    debugEnabled=true;
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

static int convertRating(double r) {
    return (int)r;
    //return qRound(r/2.0);
}

static double parseDoubleString(const TagLib::String &str) {
    if (str.isEmpty()) {
        return 0.0;
    }

    QString s=tString2QString(str);
    bool ok=false;
    double v=s.toDouble(&ok);

    return ok ? v : 0.0;
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

static TagLib::FileRef getFileRef(const QString &path)
{
    ensureFileTypeResolvers();

    #ifdef Q_OS_WIN
    return TagLib::FileRef(reinterpret_cast<const wchar_t*>(path.constData()), true, TagLib::AudioProperties::Fast);
    #else
    return TagLib::FileRef(QFile::encodeName(path).constData(), true, TagLib::AudioProperties::Fast);
    #endif
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
static bool clearTxxxTag(TagLib::ID3v2::Tag *tag, TagLib::String tagName)
{
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

static bool clearRva2Tag(TagLib::ID3v2::Tag *tag, TagLib::String tagName)
{
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

static void setTxxxTag(TagLib::ID3v2::Tag *tag, const std::string &tagName, const std::string &value)
{
    TagLib::ID3v2::UserTextIdentificationFrame *frame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, tagName);
    if (!frame) {
        frame = new TagLib::ID3v2::UserTextIdentificationFrame;
        frame->setDescription(tagName);
        tag->addFrame(frame);
    }
    frame->setText(value);
}

static void setRva2Tag(TagLib::ID3v2::Tag *tag, const std::string &tagName, double gain, double peak)
{
    TagLib::ID3v2::RelativeVolumeFrame *frame = NULL;
    TagLib::ID3v2::FrameList frameList = tag->frameList("RVA2");
    TagLib::ID3v2::FrameList::ConstIterator it = frameList.begin();
    for (; it != frameList.end(); ++it) {
        TagLib::ID3v2::RelativeVolumeFrame *fr=dynamic_cast<TagLib::ID3v2::RelativeVolumeFrame*>(*it);
        if (fr && fr->identification() == tagName) {
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

static void readID3v2Tags(TagLib::ID3v2::Tag *tag, Song *song, ReplayGain *rg, QImage *img, QString *lyrics, int *rating)
{
    if (song) {
        const TagLib::ID3v2::FrameList &albumArtist = tag->frameListMap()["TPE2"];

        if (!albumArtist.isEmpty()) {
            song->albumartist=tString2QString(albumArtist.front()->toString());
        }

        const TagLib::ID3v2::FrameList &composer = tag->frameListMap()["TCOM"];

        if (!composer.isEmpty()) {
            song->setComposer(tString2QString(composer.front()->toString()));
        }

        const TagLib::ID3v2::FrameList &disc = tag->frameListMap()["TPOS"];

        if (!disc.isEmpty()) {
            song->disc=splitDiscNumber(tString2QString(disc.front()->toString())).first;
        }

        const TagLib::ID3v2::FrameList &tcon = tag->frameListMap()["TCON"];
        if (!tcon.isEmpty()) {
            TagLib::ID3v2::FrameList::ConstIterator it = tcon.begin();
            TagLib::ID3v2::FrameList::ConstIterator end = tcon.end();
            QSet<QString> genres;

            for (; it!=end; ++it) {
                TagLib::ID3v2::TextIdentificationFrame *f = static_cast<TagLib::ID3v2::TextIdentificationFrame *>(*it);
                TagLib::StringList fields = f->fieldList();
                TagLib::StringList::ConstIterator fIt = fields.begin();
                TagLib::StringList::ConstIterator fEnd = fields.end();

                for (; fIt!=fEnd; ++fIt) {
                    if ((*fIt).isEmpty()) {
                        continue;
                    }

                    bool ok;
                    int number = (*fIt).toInt(&ok);
                    QString genre;
                    if (ok && number >= 0 && number <= 255) {
                        genre = tString2QString(TagLib::ID3v1::genre(number));
                    } else {
                        genre = tString2QString(*fIt);
                    }
                    if (!genres.contains(genre)) {
                        genres.insert(genre);
                        if (!song->genre.isEmpty()) {
                            song->genre+=QLatin1Char(';');
                        }
                        song->genre+=genre;
                    }
                }
            }
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

    if (rating) {
        // First check for FMPS rating
        const TagLib::ID3v2::FrameList &frames = tag->frameList("TXXX");
        TagLib::ID3v2::FrameList::ConstIterator it = frames.begin();
        TagLib::ID3v2::FrameList::ConstIterator end = frames.end();

        for (; it != end; ++it) {
            TagLib::ID3v2::UserTextIdentificationFrame *fr=dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(*it);
            if (fr) {
                if (fr->description().upper() == "FMPS_RATING") {
                    *rating=convertRating(parseDoubleString(fr->fieldList().back())*10);
                    return;
                }
            }
        }

        const TagLib::ID3v2::FrameList &popm = tag->frameListMap()["POPM"];
        it = popm.begin();
        end = popm.end();

        for (; it != end; ++it) {
            TagLib::ID3v2::PopularimeterFrame *p=dynamic_cast< TagLib::ID3v2::PopularimeterFrame * >(*it);
            if (p) {
                if (p->email().isEmpty()) {
                    *rating=convertRating(p->rating() / 256.0 * 10.0);
                    return;
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

    if (/*!from.isEmpty() &&*/ !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateID3v2Tag(tag, "TPE2", to.albumartist)) {
            changed=true;
        }
        if (from.composer()!=to.composer() && updateID3v2Tag(tag, "TCOM", to.composer())) {
            changed=true;
        }
        if (from.disc!=to.disc&& updateID3v2Tag(tag, "TPOS", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
        DBUG << "genres" << from.genre << to.genre;
        if (from.genre!=to.genre) {
            tag->removeFrames("TCON");
            QStringList genres=to.genres();
            DBUG << "num genres:" << genres.count();
            if (genres.count()<2) {
                DBUG << "set genre" << (genres.isEmpty() ? QString() : genres.first().trimmed());
                tag->setGenre(qString2TString(genres.isEmpty() ? QString() : genres.first().trimmed()));
            } else {
                foreach (const QString &genre, genres) {
                    TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame("TCON");
                    tag->addFrame(frame);
                    DBUG << "add genre" << genre.trimmed();
                    frame->setText(qString2TString(genre.trimmed()));
                }
            }
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

static void readAPETags(TagLib::APE::Tag *tag, Song *song, ReplayGain *rg, QImage *img, int *rating)
{
    const TagLib::APE::ItemListMap &map = tag->itemListMap();

    if (song) {
        if (map.contains("Album Artist")) {
            song->albumartist=tString2QString(map["Album Artist"].toString());
        }
        if (map.contains("Composer")) {
            song->setComposer(tString2QString(map["Composer"].toString()));
        }
        if (map.contains("Disc")) {
            song->disc=splitDiscNumber(tString2QString(map["Disc"].toString())).first;
        }
        if (map.contains("GENRE")) {
            TagLib::StringList genres=map["GENRE"].values();
            TagLib::StringList::ConstIterator it=genres.begin();
            TagLib::StringList::ConstIterator end=genres.end();
            for (; it!=end; ++it) {
                song->addGenre(tString2QString(*it));
            }
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

    if (img) {
        if (map.contains("COVER ART (FRONT)")) {
            const TagLib::ByteVector nullStringTerminator(1, 0);

            TagLib::ByteVector item = map["COVER ART (FRONT)"].value();
            int pos = item.find(nullStringTerminator);   // Skip the filename

            if (++pos > 0) {
                const TagLib::ByteVector &bytes=item.mid(pos);
                QByteArray data(bytes.data(), bytes.size());
                img->loadFromData(data);
            }
        }
    }

    if (rating) {
        *rating=convertRating(parseDoubleString(map["FMPS_RATING"].toString())*10.0);
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

    if (/*!from.isEmpty() &&*/ !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateAPETag(tag, "Album Artist", to.albumartist)) {
            changed=true;
        }
        if (from.composer()!=to.composer() && updateAPETag(tag, "Composer", to.composer())) {
            changed=true;
        }
        if (from.disc!=to.disc && updateAPETag(tag, "Disc", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
        if (from.genre!=to.genre) {
            tag->removeItem("GENRE");
            QStringList genres=to.genres();
            if (genres.count()<2) {
                tag->setGenre(qString2TString(genres.isEmpty() ? QString() : genres.first().trimmed()));
            } else {
                foreach (const QString &genre, genres) {
                    tag->addValue("GENRE", qString2TString(genre.trimmed()), false);
                }
            }
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

static void readVorbisCommentTags(TagLib::Ogg::XiphComment *tag, Song *song, ReplayGain *rg, QImage *img, QString *lyrics, int *rating)
{
    if (song) {
        TagLib::String str=readVorbisTag(tag, "ALBUMARTIST");
        if (str.isEmpty()) {
            song->albumartist=tString2QString(str);
        }
        str=readVorbisTag(tag, "COMPOSER");
        if (str.isEmpty()) {
            song->setComposer(tString2QString(str));
        }
        str=readVorbisTag(tag, "DISCNUMBER");
        if (str.isEmpty()) {
            song->disc=splitDiscNumber(tString2QString(str)).first;
        }
        if (tag->contains("GENRE")) {
            const TagLib::StringList &genres = tag->fieldListMap()["GENRE"];
            if (!genres.isEmpty()) {
                TagLib::StringList::ConstIterator it=genres.begin();
                TagLib::StringList::ConstIterator end=genres.end();
                for (; it!=end; ++it) {
                    song->addGenre(tString2QString(*it));
                }
            }
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
        #if TAGLIB_VERSION >= CANTATA_MAKE_VERSION(1,7,0)
        // METADATA_BLOCK_PICTURE is the Ogg standard way of encoding a covers.
        // https://wiki.xiph.org/index.php/VorbisComment#Cover_art
        if (map.contains("METADATA_BLOCK_PICTURE")) {
            TagLib::StringList block=map["METADATA_BLOCK_PICTURE"];
            if (!block.isEmpty()) {
                TagLib::StringList::ConstIterator i = block.begin();
                TagLib::StringList::ConstIterator end = block.end();

                for (; i != end; ++i ) {
                    QByteArray data(QByteArray::fromBase64(i->to8Bit().c_str()));
                    TagLib::ByteVector bytes(data.data(), data.size());
                    TagLib::FLAC::Picture pic;

                    if (pic.parse(bytes)) {
                        img->loadFromData(QByteArray(pic.data().data(), pic.data().size()));
                    }
                }
            }
        }
        #endif

        // COVERART is an older (now deprecated) way of storing covers...
        if (img->isNull() && map.contains("COVERART")) {
            QByteArray data=map["COVERART"].toString().toCString();
            img->loadFromData(QByteArray::fromBase64(data));
            if (img->isNull()) {
                img->loadFromData(data); // not base64??
            }
        }
    }

    if (lyrics) {
        TagLib::String str=readVorbisTag(tag, "LYRICS");
        if (!str.isEmpty()) {
            *lyrics=tString2QString(str);
        }
    }

    if (rating) {
        *rating=convertRating(parseDoubleString(readVorbisTag(tag, "FMPS_RATING"))*10.0);
    }
}

#if TAGLIB_VERSION >= CANTATA_MAKE_VERSION(1,7,0)
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

    if (/*!from.isEmpty() &&*/ !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateVorbisCommentTag(tag, "ALBUMARTIST", to.albumartist)) {
            changed=true;
        }
        if (from.composer()!=to.composer() && updateVorbisCommentTag(tag, "COMPOSER", to.composer())) {
            changed=true;
        }
        if (from.disc!=to.disc && updateVorbisCommentTag(tag, "DISCNUMBER", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
        if (from.genre!=to.genre) {
            tag->removeField("GENRE");
            QStringList genres=to.genres();
            if (genres.count()<2) {
                tag->setGenre(qString2TString(genres.isEmpty() ? QString() : genres.first().trimmed()));
            } else {
                foreach (const QString &genre, genres) {
                    tag->addField("GENRE", qString2TString(genre.trimmed()), false);
                }
            }
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
static void readMP4Tags(TagLib::MP4::Tag *tag, Song *song, ReplayGain *rg, QImage *img, int *rating)
{
    TagLib::MP4::ItemListMap &map = tag->itemListMap();

    if (song) {
        if (map.contains("aART") && !map["aART"].toStringList().isEmpty()) {
            song->albumartist=tString2QString(map["aART"].toStringList().front());
        }
        if (map.contains("\xA9wrt") && !map["\xA9wrt"].toStringList().isEmpty()) {
            song->setComposer(tString2QString(map["\xA9wrt"].toStringList().front()));
        }
        if (map.contains("disk")) {
            song->disc=map["disk"].toIntPair().first;
        }
        if (map.contains("\251gen")) {
            TagLib::StringList genres=map["\251gen"].toStringList();
            TagLib::StringList::ConstIterator it=genres.begin();
            TagLib::StringList::ConstIterator end=genres.end();
            for (; it!=end; ++it) {
                song->addGenre(tString2QString(*it));
            }
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
    if (rating) {
        if (map.contains("----:com.apple.iTunes:FMPS_Rating")) {
            *rating=convertRating(parseDoubleString(map["----:com.apple.iTunes:FMPS_Rating"].toStringList().front())*10.0);
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
        map.insert(tagName, TagLib::StringList(qString2TString(value)));
        return true;
    }
    return false;
}

static bool writeMP4Tags(TagLib::MP4::Tag *tag, const Song &from, const Song &to, const RgTags &rg, const QByteArray &img)
{
    bool changed=false;

    if (/*!from.isEmpty() &&*/ !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateMP4Tag(tag, "aART", to.albumartist)) {
            changed=true;
        }
        if (from.composer()!=to.composer() && updateMP4Tag(tag, "\xA9wrt", to.composer())) {
            changed=true;
        }
        if (from.disc!=to.disc && updateMP4Tag(tag, "disk", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
        if (from.genre!=to.genre) {
            QStringList genres=to.genres();
            if (genres.count()<2) {
                tag->setGenre(qString2TString(genres.isEmpty() ? QString() : genres.first().trimmed()));
            } else {
                TagLib::StringList tagGenres;
                foreach (const QString &genre, genres) {
                    tagGenres.append(qString2TString(genre.trimmed()));
                }
                tag->itemListMap()["\251gen"]=tagGenres;
            }
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
static void readASFTags(TagLib::ASF::Tag *tag, Song *song, int *rating)
{
    if (song) {
        const TagLib::ASF::AttributeListMap &map = tag->attributeListMap();

        if (map.contains("WM/AlbumTitle") && !map["WM/AlbumTitle"].isEmpty()) {
            song->albumartist=tString2QString(map["WM/AlbumTitle"].front().toString());
        }
        if (map.contains("WM/Composer") && !map["WM/Composer"].isEmpty()) {
            song->setComposer(tString2QString(map["WM/Composer"].front().toString()));
        }
        if (map.contains("WM/PartOfSet") && !map["WM/PartOfSet"].isEmpty()) {
            song->albumartist=map["WM/PartOfSet"].front().toUInt();
        }
        if (map.contains("WM/Genre") && !map["WM/Genre"].isEmpty()) {
            const TagLib::ASF::AttributeList &genres=map["WM/Genre"];
            TagLib::ASF::AttributeList::ConstIterator it=genres.begin();
            TagLib::ASF::AttributeList::ConstIterator end=genres.end();
            for (; it!=end; ++it) {
                song->addGenre(tString2QString((*it).toString()));
            }
        }
    }
    if (rating) {
        const TagLib::ASF::AttributeListMap &map = tag->attributeListMap();

        if (map.contains("FMPS/Rating") && !map["FMPS/Rating"].isEmpty()) {
            *rating=convertRating(parseDoubleString(map["FMPS/Rating"].front().toString())*10.0);
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

    if (/*!from.isEmpty() &&*/ !to.isEmpty()) {
        if (from.albumartist!=to.albumartist && updateASFTag(tag, "WM/AlbumTitle", to.albumartist)) {
            changed=true;
        }
        if (from.composer()!=to.composer() && updateASFTag(tag, "WM/Composer", to.composer())) {
            changed=true;
        }
        if (from.disc!=to.disc && updateASFTag(tag, "WM/PartOfSet", 0==to.disc ? QString() : QString::number(to.disc))) {
            changed=true;
        }
        if (from.genre!=to.genre) {
            tag->removeItem("WM/Genre");
            QStringList genres=to.genres();
            if (genres.count()<2) {
                tag->setGenre(qString2TString(genres.isEmpty() ? QString() : genres.first().trimmed()));
            } else {
                foreach (const QString &genre, genres) {
                    tag->addAttribute("WM/Genre", qString2TString(genre.trimmed()));
                }
            }
            changed=true;
        }
    }
    return changed;
}
#endif

static void readTags(const TagLib::FileRef fileref, Song *song, ReplayGain *rg, QImage *img, QString *lyrics, int *rating)
{
    TagLib::Tag *tag=fileref.tag();
    if (song) {
        song->title=tString2QString(tag->title());
        song->artist=tString2QString(tag->artist());
        song->album=tString2QString(tag->album());
//        song->genre=tString2QString(tag->genre());
        song->track=tag->track();
        song->year=tag->year();
    }

    if (TagLib::MPEG::File *file = dynamic_cast< TagLib::MPEG::File * >(fileref.file())) {
        if (file->ID3v2Tag() && !file->ID3v2Tag()->isEmpty()) {
            readID3v2Tags(file->ID3v2Tag(), song, rg, img, lyrics, rating);
        } else if (file->APETag()) {
            readAPETags(file->APETag(), song, rg, img, rating);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    } else if (TagLib::Ogg::Vorbis::File *file = dynamic_cast< TagLib::Ogg::Vorbis::File * >(fileref.file()))  {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song, rg, img, lyrics, rating);
        }
    } else if (TagLib::Ogg::FLAC::File *file = dynamic_cast< TagLib::Ogg::FLAC::File * >(fileref.file())) {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song, rg, img, lyrics, rating);
        }
    } else if (TagLib::Ogg::Speex::File *file = dynamic_cast< TagLib::Ogg::Speex::File * >(fileref.file())) {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song, rg, img, lyrics, rating);
        }
    #ifdef TAGLIB_OPUS_FOUND
    } else if (TagLib::Ogg::Opus::File *file = dynamic_cast< TagLib::Ogg::Opus::File * >(fileref.file())) {
        if (file->tag()) {
            readVorbisCommentTags(file->tag(), song, rg, img, lyrics, rating);
        }
    #endif
    } else if (TagLib::FLAC::File *file = dynamic_cast< TagLib::FLAC::File * >(fileref.file())) {
        if (file->xiphComment()) {
            readVorbisCommentTags(file->xiphComment(), song, rg, img, lyrics, rating);
            #if TAGLIB_VERSION >= CANTATA_MAKE_VERSION(1,7,0)
            if (img && img->isNull()) {
                readFlacPicture(file->pictureList(), img);
            }
            #endif
        } else if (file->ID3v2Tag() && !file->ID3v2Tag()->isEmpty()) {
            readID3v2Tags(file->ID3v2Tag(), song, rg, img, lyrics, rating);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    #ifdef TAGLIB_MP4_FOUND
    } else if (TagLib::MP4::File *file = dynamic_cast< TagLib::MP4::File * >(fileref.file())) {
        TagLib::MP4::Tag *tag = dynamic_cast< TagLib::MP4::Tag * >(file->tag());
        if (tag) {
            readMP4Tags(tag, song, rg, img, rating);
        }
    #endif
    } else if (TagLib::MPC::File *file = dynamic_cast< TagLib::MPC::File * >(fileref.file())) {
        if (file->APETag()) {
            readAPETags(file->APETag(), song, rg, img, rating);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    } else if (TagLib::RIFF::AIFF::File *file = dynamic_cast< TagLib::RIFF::AIFF::File * >(fileref.file())) {
        if (file->tag()) {
            readID3v2Tags(file->tag(), song, rg, img, lyrics, rating);
        }
    } else if (TagLib::RIFF::WAV::File *file = dynamic_cast< TagLib::RIFF::WAV::File * >(fileref.file())) {
        if (file->tag()) {
            readID3v2Tags(file->tag(), song, rg, img, lyrics, rating);
        }
    #ifdef TAGLIB_ASF_FOUND
    } else if (TagLib::ASF::File *file = dynamic_cast< TagLib::ASF::File * >(fileref.file())) {
        TagLib::ASF::Tag *tag = dynamic_cast< TagLib::ASF::Tag * >(file->tag());
        if (tag) {
            readASFTags(tag, song, rating);
        }
    #endif
    } else if (TagLib::TrueAudio::File *file = dynamic_cast< TagLib::TrueAudio::File * >(fileref.file())) {
        if (file->ID3v2Tag(false)) {
            readID3v2Tags(file->ID3v2Tag(false), song, rg, img, lyrics, rating);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    } else if (TagLib::WavPack::File *file = dynamic_cast< TagLib::WavPack::File * >(fileref.file())) {
        if (file->APETag()) {
            readAPETags(file->APETag(), song, rg, img, rating);
//         } else if (file->ID3v1Tag()) {
//             readID3v1Tags(fileref, song, rg);
        }
    }
    if (song && song->genre.isEmpty()) {
        song->genre=tString2QString(tag->genre());
    }
}

static bool writeTags(const TagLib::FileRef fileref, const Song &from, const Song &to, const RgTags &rg, const QByteArray &img, bool saveComment)
{
    bool changed=false;
    TagLib::Tag *tag=fileref.tag();
    if (/*!from.isEmpty() &&*/ !to.isEmpty()) {
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
//        if (from.genre!=to.genre) {
//            tag->setGenre(qString2TString(to.genre));
//            changed=true;
//        }
        if (from.track!=to.track) {
            tag->setTrack(to.track);
            changed=true;
        }
        if (from.year!=to.year) {
            tag->setYear(to.year);
            changed=true;
        }
        if (saveComment && from.comment()!=to.comment()) {
            tag->setComment(qString2TString(to.comment()));
            changed=true;
        }
    }
    if (TagLib::MPEG::File *file = dynamic_cast< TagLib::MPEG::File * >(fileref.file())) {
        if (file->ID3v2Tag() && !file->ID3v2Tag()->isEmpty()) {
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
    #ifdef TAGLIB_OPUS_FOUND
    } else if (TagLib::Ogg::Opus::File *file = dynamic_cast< TagLib::Ogg::Opus::File * >(fileref.file())) {
        if (file->tag()) {
            changed=writeVorbisCommentTags(file->tag(), from, to, rg, img) || changed;
        }
    #endif
    } else if (TagLib::FLAC::File *file = dynamic_cast< TagLib::FLAC::File * >(fileref.file())) {
        if (file->xiphComment()) {
            changed=writeVorbisCommentTags(file->xiphComment(), from, to, rg, img) || changed;
        } else if (file->ID3v2Tag() && !file->ID3v2Tag()->isEmpty()) {
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

Song read(const QString &fileName)
{
    LOCK_MUTEX
    Song song;
    TagLib::FileRef fileref = getFileRef(fileName);
    if (fileref.isNull()) {
        return song;
    }

    readTags(fileref, &song, 0, 0, 0, 0);
    song.file=fileName;
    song.time=fileref.audioProperties() ? fileref.audioProperties()->length() : 0;
    return song;
}

QImage readImage(const QString &fileName)
{
    LOCK_MUTEX
    QImage img;
    TagLib::FileRef fileref = getFileRef(fileName);
    if (fileref.isNull()) {
        return img;
    }

    readTags(fileref, 0, 0, &img, 0, 0);
    return img;
}

QString readLyrics(const QString &fileName)
{
    LOCK_MUTEX
    QString lyrics;
    TagLib::FileRef fileref = getFileRef(fileName);
    if (fileref.isNull()) {
        return lyrics;
    }

    readTags(fileref, 0, 0, 0, &lyrics, 0);
    return lyrics;
}

QString readComment(const QString &fileName)
{
    LOCK_MUTEX
    TagLib::FileRef fileref = getFileRef(fileName);
    return fileref.isNull() ? QString() : tString2QString(fileref.tag()->comment());
}

static Update update(const TagLib::FileRef fileref, const Song &from, const Song &to, const RgTags &rg, const QByteArray &img, int id3Ver=-1, bool saveComment=false)
{
    if (writeTags(fileref, from, to, rg, img, saveComment)) {
        TagLib::MPEG::File *mpeg=dynamic_cast<TagLib::MPEG::File *>(fileref.file());
        if (mpeg) {
            #ifdef TAGLIB_CAN_SAVE_ID3VER
            TagLib::ID3v2::Tag *v2=mpeg->ID3v2Tag(false);
            bool isID3v24=v2 && isId3V24(v2->header());
            int ver=id3Ver==3 ? 3 : (id3Ver==4 ? 4 : (isID3v24 ? 4 : 3));
            DBUG << "isID3v24" << isID3v24 << "reqVer:" << id3Ver << "use:" << ver;
            return mpeg->save(TagLib::MPEG::File::ID3v2, true, ver) ? Update_Modified : Update_Failed;
            #else
            Q_UNUSED(id3Ver)
            return mpeg->save(TagLib::MPEG::File::ID3v2, true) ? Update_Modified : Update_Failed;
            #endif
        }
        return fileref.file()->save() ? Update_Modified : Update_Failed;
    }
    return Update_None;
}

Update updateArtistAndTitle(const QString &fileName, const Song &song)
{
    LOCK_MUTEX
    TagLib::FileRef fileref = getFileRef(fileName);
    if (fileref.isNull()) {
        return Update_Failed;
    }

    TagLib::MPEG::File *mpeg=dynamic_cast<TagLib::MPEG::File *>(fileref.file());
    TagLib::Tag *tag=fileref.tag();
    tag->setTitle(qString2TString(song.title));
    tag->setArtist(qString2TString(song.artist));

    if (mpeg) {
        #ifdef TAGLIB_CAN_SAVE_ID3VER
        TagLib::ID3v2::Tag *v2=mpeg->ID3v2Tag(false);
        int ver=v2 && isId3V24(v2->header()) ? 4 : 3;
        DBUG << "useId3ver:" << ver;
        return mpeg->save(TagLib::MPEG::File::ID3v2, true, ver) ? Update_Modified : Update_Failed;
        #else
        return mpeg->save(TagLib::MPEG::File::ID3v2, true) ? Update_Modified : Update_Failed;
        #endif
    }
    return fileref.file()->save() ? Update_Modified : Update_Failed;
}

Update update(const QString &fileName, const Song &from, const Song &to, int id3Ver, bool saveComment)
{
    LOCK_MUTEX
    TagLib::FileRef fileref = getFileRef(fileName);
    return fileref.isNull() ? Update_Failed : update(fileref, from, to, RgTags(), QByteArray(), id3Ver, saveComment);
}

ReplayGain readReplaygain(const QString &fileName)
{
    LOCK_MUTEX
    TagLib::FileRef fileref = getFileRef(fileName);
    if (fileref.isNull()) {
        return false;
    }

    ReplayGain rg;
    readTags(fileref, 0, &rg, 0, 0, 0);
    return rg;
}

Update updateReplaygain(const QString &fileName, const ReplayGain &rg)
{
    LOCK_MUTEX
    TagLib::FileRef fileref = getFileRef(fileName);
    return fileref.isNull() ? Update_Failed : update(fileref, Song(), Song(), RgTags(rg), QByteArray());
}

Update embedImage(const QString &fileName, const QByteArray &cover)
{
    LOCK_MUTEX
    TagLib::FileRef fileref = getFileRef(fileName);
    return fileref.isNull() ? Update_Failed : update(fileref, Song(), Song(), RgTags(), cover);
}

QString oggMimeType(const QString &fileName)
{
    LOCK_MUTEX
    #ifdef Q_OS_WIN
    const wchar_t*encodedName=reinterpret_cast<const wchar_t*>(fileName.constData());
    #else
    const char *encodedName=QFile::encodeName(fileName).constData();
    #endif

    TagLib::Ogg::Vorbis::File vorbis(encodedName, false, TagLib::AudioProperties::Fast);
    if (vorbis.isValid()) {
        return QLatin1String("audio/x-vorbis+ogg");
    }

    TagLib::Ogg::FLAC::File flac(encodedName, false, TagLib::AudioProperties::Fast);
    if (flac.isValid()) {
        return QLatin1String("audio/x-flac+ogg");
    }

    TagLib::Ogg::Speex::File speex(encodedName, false, TagLib::AudioProperties::Fast);
    if (speex.isValid()) {
        return QLatin1String("audio/x-speex+ogg");
    }

    #ifdef TAGLIB_OPUS_FOUND
    TagLib::Ogg::Opus::File opus(encodedName, false, TagLib::AudioProperties::Fast);
    if (opus.isValid()) {
        return QLatin1String("audio/x-opus+ogg");
    }
    #endif
    return QLatin1String("audio/ogg");
}

int readRating(const QString &fileName)
{
    LOCK_MUTEX
    int rating=-1;
    TagLib::FileRef fileref = getFileRef(fileName);
    if (!fileref.isNull()) {
        readTags(fileref, 0, 0, 0, 0, &rating);
    }
    return rating;
}

QMap<QString, QString> readAll(const QString &fileName)
{
    QMap<QString, QString> allTags;
    TagLib::FileRef fileref = getFileRef(fileName);
    if (fileref.isNull()) {
        return allTags;
    }

    #if TAGLIB_VERSION >= CANTATA_MAKE_VERSION(1,8,0)
    TagLib::PropertyMap properties=fileref.file()->properties();
    TagLib::PropertyMap::ConstIterator it = properties.begin();
    TagLib::PropertyMap::ConstIterator end = properties.end();
    for (; it!=end; ++it) {
        allTags.insert(tString2QString(it->first.upper()), tString2QString(it->second.toString(", ")));
    }

    if (fileref.audioProperties()) {
        TagLib::AudioProperties *properties = fileref.audioProperties();
        allTags.insert(QLatin1String("X-AUDIO:BITRATE"), QString("%1 kb/s").arg(properties->bitrate()));
        allTags.insert(QLatin1String("X-AUDIO:SAMPLERATE"), QString("%1 Hz").arg(properties->sampleRate()));
        allTags.insert(QLatin1String("X-AUDIO:CHANNELS"), QString::number(properties->channels()));
    }
    #endif
    return allTags;
}

QString id3Genre(int id)
{
    // Clementine: In theory, genre 0 is "blues"; in practice it's invalid.
    return 0==id ? QString() : tString2QString(TagLib::ID3v1::genre(id));
}

}
