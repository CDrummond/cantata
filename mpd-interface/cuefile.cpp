/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cuefile.h"
#include "mpdconnection.h"
#include "support/utils.h"
#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStringBuilder>
#include <QRegExp>
#include <QRegularExpression>
#include <QTextCodec>
#include <QTextStream>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QObject>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "CueFile"
void CueFile::enableDebug()
{
    debugEnabled=true;
}

static const QString constCueProtocol = QLatin1String("cue:///");
static const QString constFile = QLatin1String("file");
static const QString constAudioTrackType = QLatin1String("audio");
static const QString constGenre = QLatin1String("genre");
static const QString constDate = QLatin1String("date");
static const QString constOrigYear = QLatin1String("originalyear");
static const QString constOrigDate = QLatin1String("originaldate");
static const QString constDisc = QLatin1String("disc");
static const QString constDiscNumber = QLatin1String("discnumber");
static const QString constRemark = QLatin1String("rem");
static const QString constComment = QLatin1String("comment");
static const QString constTrack = QLatin1String("track");
static const QString constIndex = QLatin1String("index");
static const QString constTitle = QLatin1String("title");
static const QString constComposer = QLatin1String("composer");
static const QString constPerformer = QLatin1String("performer");

bool CueFile::isCue(const QString &str)
{
    return str.startsWith(constCueProtocol);
}

QByteArray CueFile::getLoadLine(const QString &str)
{
    QUrl u(str);
    QUrlQuery q(u);

    if (q.hasQueryItem("pos")) {
        QString pos=q.queryItemValue("pos");
        QString path=u.path();
        if (path.startsWith("/")) {
            path=path.mid(1);
        }
        return MPDConnection::encodeName(path)+" \""+pos.toLatin1()+":"+QString::number(pos.toInt()+1).toLatin1()+"\"";
    }

    return MPDConnection::encodeName(str);
}

static const QList<QTextCodec *> & codecList()
{
    static QList<QTextCodec *> codecs;
    if (codecs.isEmpty()) {
        codecs.append(QTextCodec::codecForName("UTF-8"));
        QTextCodec *codec=QTextCodec::codecForLocale();
        if (codec && !codecs.contains(codec)) {
            codecs.append(codec);
        }
        codec=QTextCodec::codecForName("System");
        if (codec && !codecs.contains(codec)) {
            codecs.append(codec);
        }
    }
    return codecs;
}

// Split a raw .cue line into logical parts, returning a list where:
//   the 1st item contains 'REM' if present (if not present it is empty),
//   the 2nd item contains the CUE command (which is empty only for the standard 'REM comment' commands)
//   the 3rd item contains the remaining part in the line
static QStringList splitCueLine(const QString &line)
{
    QRegularExpression reCueLine;
    reCueLine.setPattern("^\\s*(REM){0,1}\\s*([a-zA-Z]*){0,1}\\s*(.*)$");
    reCueLine.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    reCueLine.setPatternOptions(QRegularExpression::DotMatchesEverythingOption); // dot metacharacter (.) match everything including newlines

    QRegularExpressionMatch m = reCueLine.match(line);
    if (m.hasMatch()) {
        return { m.captured(1), m.captured(2), m.captured(3) };
    } else {
        return QStringList();
    }
}

static const double constMsecPerSec = 1000.0;

// Seconds in a MSF index (MM:SS:FF)
static double indexToMarker(const QString &index)
{
    QRegExp indexRegexp("(\\d{1,3}):(\\d{2}):(\\d{2})");
    if (!indexRegexp.exactMatch(index)) {
        return -1.0;
    }

    QStringList splitted = indexRegexp.capturedTexts().mid(1, -1);
    qlonglong frames = splitted.at(0).toLongLong() * 60 * 75 + splitted.at(1).toLongLong() * 75 + splitted.at(2).toLongLong();
    return (frames * constMsecPerSec) / 75.0;
}

// Updates the time (in seconds) using the indexes of the tracks (songs).
// This one mustn't be used for the last track (song).
static bool indexTime(const QString &index, const QString &nextIndex, int &time)
{
    double beginning = indexToMarker(index);
    double end = indexToMarker(nextIndex);

    // incorrect indices (we won't be able to calculate beginning or end)
    if (beginning<0 || end<0) {
        DBUG << "Failed to calculate time - index:" << index << "nextIndex:" << nextIndex << "beginning:" << beginning << "end:" << end;
        return false;
    }
    // calculate time duration in seconds
    time=static_cast<int>(((end-beginning)/constMsecPerSec)+0.5);
    return true;
}

// Updates the lastTrackIndex time (in seconds) using the index of the last track (song).
// This one must be used ONLY for the last track (song).
//      note: the last song time will be calculate dinamically later
//            (see: MPDParseUtils::parseDirItems in mpd-interface/mpdparseutils.cpp )
static bool indexLastTime(const QString &index, double &lastTrackIndex)
{
    double beginning = indexToMarker(index);

    // incorrect index (we won't be able to calculate beginning)
    if (beginning<0) {
        DBUG << "Failed to calculate *last* time - index:" << index << "beginning:" << beginning;
        return false;
    }
    // note: the last song duration will be calculate dinamically
    //       (see: MPDParseUtils::parseDirItems in mpd-interface/mpdparseutils.cpp )
    lastTrackIndex = beginning;
    return true;
}

// Is various artists?
static inline bool isVariousArtists(const QString &str)
{
    if (0==str.compare(Song::variousArtists(), Qt::CaseInsensitive)) {
        return true;
    }

    QString lower = str.toLower();
    return QLatin1String("various") == lower || QLatin1String("various artists") == lower;
}

// Parse CUE file content
//
// Supported CUE tags                       | corresponding MPD tags
// -------------------
// FILE HEADER SECTION
// -------------------
// REM GENRE "genre"                        | genre
// REM DATE "YYYY"                          | date
// REM ORIGINALYEAR "YYYY"                  | originadate
// REM ORIGINALDATE "YYYY"                  |
// REM DISC "[N]N"                          | disc
// REM DISCNUMBER "[N]N"                    |
// REM "comment"                            | comment
// REM COMMENT "comment" [relaxed syntax]   |
// REM COMPOSER "composer"                  | composer (see NOTES)
// COMPOSER "composer" [relaxed syntax]     |
// PERFORMER "performer"                    | artist (see NOTES)
// TITLE "album"                            | album
// FILE "filename" filetype
// ----------------
// TRACK(S SECTION
// ----------------
// TRACK NN datatype
// INDEX NN MM:SS:FF
// TITLE "title"                            | title
// REM COMPOSER "composer"                  | composer (see NOTES)
// COMPOSER "composer" [relaxed syntax]     |
// PERFORMER "performer"                    | performer (see NOTES)
//
// NOTES:
// -----
//
// - The official CUESHEET specification does not offer a specific command to indicate the album artist;
//   and the MPD documentation says: "albumartist: On multi-artist albums, this is the artist name which
//   shall be used for the whole album (the exact meaning of this tag is not well-defined)."
//
// - The MPD documentation says: "artist: The artist name (its meaning is not well-defined; see composer
//   and performer for more specific tags)."
//   The official CUESHEET specification by CDRWIN state the rule: "If the PERFORMER command appears
//   before any TRACK commands, then the string will be encoded as the performer of the entire disc. If the
//   command appears after a TRACK command, then the string will be encoded as the performer of the current track."
//   The PERFORMER command, when present in the header section of a CUE file, is generally and conventionally
//   used with reference to the entire album to indicate the artist (band or singer), or the composer (and not
//   who is performing the song) in the case of genres such is classical music.
//   Therefore:
//   * when in the header section of a CUE file, PERFORMER should mean the artist to whom the album refers to;
//   * when in the tracks section of a CUE file, PERFORMER should mean the artist performing the song.
//
// - MPD has a composer tag, but the official CUESHEET specification does not offer a specific command
//   to indicate the composer; nevertheless, especially in the CUE files associated with classical music CDs,
//   the REM COMPOSER (which is a standard-compliant use trick of the REM command) or the COMPOSER (relaxed
//   non-compliant syntax) is sometimes used, mainly in the tracks section.
//   Similar to PERFORMER, a compliant rule for COMPOSER should be: "If the COMPOSER command appears before
//   any TRACK commands, then the string will be encoded as the composer of the entire disc. If the command
//   appears after a TRACK command, then the string will be encoded as the composer of the current track."
//   Therefore:
//   * when in the header section of a CUE file, COMPOSER should mean the composer to whom the album refers to;
//   * when in the tracks section of a CUE file, COMPOSER should mean the composer of the song.
//
bool CueFile::parse(const QString &fileName, const QString &dir, QList<Song> &songList, QSet<QString> &files, double &lastTrackIndex)
{
    DBUG << fileName;

    // CUE file stream
    QScopedPointer<QTextStream> textStream;
    QString decoded;
    QFile f(dir+fileName);
    if (f.open(QIODevice::ReadOnly)) {
        // First attempt to use QTextDecoder to decode cue file contents into a QString
        QByteArray contents=f.readAll();
        for (QTextCodec *codec: codecList()) {
            QTextDecoder decoder(codec);
            decoded=decoder.toUnicode(contents);
            if (!decoder.hasFailure()) {
                textStream.reset(new QTextStream(&decoded, QIODevice::ReadOnly));
                break;
            }
        }
        f.close();

        if (!textStream) {
            decoded.clear();
            // Failed to use text decoders, fall back to old method...
            f.open(QIODevice::ReadOnly|QIODevice::Text);
            textStream.reset(new QTextStream(&f));
            textStream->setCodec(QTextCodec::codecForUtfText(f.peek(1024), QTextCodec::codecForName("UTF-8")));
        }
    }

    if (!textStream) {
        return false;
    }

    // file dir
    QString fileDir=fileName.contains("/") ? Utils::getDir(fileName) : QString();

    // vars to store the current values from the lines in the FILE (header) section of the CUE file
    QStringList genre;
    QString album;
    QString albumArtist;
    QString date;
    QString origYear;
    QString discNo;
    //// QString remark;
    // auxiliary vars for other data coming from the FILE section of the CUE file
    QString albumComposer;
    // variables to store the current values from the lines in the TRACKs section of the CUE file
    QString file;
    QString trackNo;
    QString trackType;
    QString index;
    QString title;
    QString artist; // is PERFORMER in the CUE file
    QString composer;
    // auxiliary data for handling the TRACKs section of the CUE file
    QString fileType;
    bool isValidFile = false;

    // hash to save a CUE track ("cuecommandtag", "cuecommandvalue")
    typedef QHash<QString, QString> CueTrack;
    // initialize the hash
    //      note: using reserve() here should NOT be necessary since QHash'automatically shrinks or grows to provide
    //            good performance without wasting memory (see: https://doc.qt.io/qt-5/qhash.html#reserve)
    CueTrack cueTrack;

    // vector where to save CUE tracks hashes (Red Book standard: CDDA can contain up to 99 tracks...)
    QVector<CueTrack> tracks;
    // note: reserve() prevent reallocations and memory fragmentation (see: https://doc.qt.io/qt-5/qvector.html#reserve)
    tracks.reserve(99);

    // initialize vars for handling parsing flow
    QString line;
    bool isCueHeader=true;

    // -- parse whole file
    while (!textStream->atEnd()) {
        // read current line removing whitespace characters from the start and the end
        line = textStream->readLine().trimmed();
        DBUG << line;

        // if current line is empty then skip the line
        if (line.isEmpty()) {
            continue;
        }

        QStringList splitted = splitCueLine(line);
        // incorrect line
        if (splitted.size() < 3 ) {
            continue;
        }
        // logical parts in the CUE line
        QString cmdRem = splitted[0].toLower();
        QString cmdCmd = splitted[1].toLower();
        QString cmdVal = splitted[2].trimmed();
        if (cmdVal.startsWith("\"") && cmdVal.endsWith("\"")) {
            cmdVal = cmdVal.mid(1, cmdVal.size()-2).trimmed();
        }
        DBUG << "cmdRem:" << cmdRem << ", cmdCmd:" << cmdCmd << ", cmdVal:" << cmdVal;

        // -- FILE section
        if (cmdCmd == constFile) {
            // get the file type
            if (!cmdVal.isEmpty()) {
                cmdVal.remove('"').remove("'");
                QStringList cmdValSplitted = cmdVal.split(QRegExp("\\s+"));
                if (cmdValSplitted.size() == 2) {
                    file = cmdValSplitted[0].remove("\"");  // file audio: name
                    fileType = cmdValSplitted[1];           // file audio: type
                }
            }
            // check if is a valid audio file (else if this is a data file, all of it's tracks will be ignored...)
            isValidFile = fileType.compare("BINARY", Qt::CaseInsensitive) && fileType.compare("MOTOROLA", Qt::CaseInsensitive);

            DBUG << "FILE: file:" << file << ", fileType:" << fileType << ", fileValid?" << isValidFile;
            DBUG << "HEADER: genre:" << genre << ", album:" << album << ", discNo:" << discNo << ", year:" << date << ", originalYear:" << origYear
                 << ", albumArtist:" << albumArtist << ", albumComposer:" << albumComposer;

            // now we are going to the TRACKs section...
            isCueHeader = false;
            // jump to next line which will be the first in the TRACKs section...
            continue;
        }
        if (isCueHeader) {
            // this is a standard 'REM comment' OR a tricky 'REM COMMENT comment'
            if ((cmdRem == constRemark && cmdCmd.isEmpty()) || cmdCmd == constComment) {
                // skip comments since are not handled by the Cantata library db...
                continue;
            // continue parsing the FILE section (header of the CUE file)...
            } else if (cmdCmd == constGenre) {
                // if GENRE is a list (separated by one of: , ; | \t), then split
                for (const auto &g: cmdVal.split(QRegExp("(,|;|\\t|\\|)"))) {
                    genre.append(g.trimmed());
                }
            } else if (cmdCmd == constTitle) {
                album = cmdVal;
            } else if (cmdCmd == constDate) {
                date = cmdVal.length()>4 ? cmdVal.left(4) : cmdVal;
            } else if (cmdCmd == constOrigDate || cmdCmd == constOrigYear) {
                origYear = cmdVal.length()>4 ? cmdVal.left(4) : cmdVal;
            } else if (cmdCmd == constDisc || cmdCmd == constDiscNumber) {
                discNo = cmdVal;
            } else if (cmdCmd == constPerformer) {
                albumArtist = cmdVal;
                // if a VA album, use the standard VA string
                if (isVariousArtists(albumArtist)) {
                    albumArtist = Song::variousArtists();
                }
            } else if (cmdCmd == constComposer) {
                albumComposer = cmdVal;
                // if a VA album, use the standard VA string
                if (isVariousArtists(albumComposer)) {
                    albumComposer = Song::variousArtists();
                }
            // } else if (...) {
            // ... just ignore the rest of possible field types for now...
            }
        // -- TRACKs section
        } else {
            // the beginning of a track's definition
            if (cmdCmd == constTrack) {
                // if there is a *previous* track and is a *valid* track, then finalize it...
                //      note: a track is valid when the file which it belongs is a valid audio file (not BINARY or MOTOROLA)
                //            AND has an index AND is an audio (AUDIO) track
                // the code section starting below is repeated after, in order to save the last track also
                //      note: Cantata has code (Song::albumArtistOrComposer) which checks if use artist (performer) or
                //            composer based upon the 'Composer support' tweak setting...

                // -- start of repeated code --
                if (isValidFile && !index.isEmpty() && (trackType == constAudioTrackType || trackType.isEmpty())) {
                    DBUG << "CUE tracks:" << tracks.size();
                    // finalize albumArtist, artist, and composer...
                    if (artist.isEmpty() && !albumArtist.isEmpty() && !isVariousArtists(albumArtist)) {
                        artist = albumArtist;
                    }
                    if (composer.isEmpty() && !albumComposer.isEmpty() && !isVariousArtists(albumComposer)) {
                        composer = albumComposer;
                    }
                    // set fallbacks...
                    if (albumArtist.isEmpty()) {
                        albumArtist = Song::unknown();
                    }
                    if (artist.isEmpty()) {
                        artist = Song::unknown();
                    }
                    if (composer.isEmpty()) {
                        composer = Song::unknown();
                    }
                    if (title.isEmpty()) {
                        title = Song::unknown();
                    }
                    DBUG << "file:" << file  << ", trackNo:" << trackNo << ", title:" << title << ", type:" << trackType << ", index:" << index
                         << ", artist:" << artist << ", composer:" << composer;
                    // update track hash values
                    //      note: using insert() means that if there is already an item with the key, its value is replaced with the
                    //            new value avoiding allocation of unnecessary items (see: https://doc.qt.io/qt-5/qhash.html#insert)
                    cueTrack.insert("file",file);
                    cueTrack.insert("trackNo",trackNo);
                    cueTrack.insert("index",index);
                    cueTrack.insert("title",title);
                    cueTrack.insert("artist",artist);
                    cueTrack.insert("composer",composer);
                    // save track
                    //     note: using append() means growing without reallocating the entire vector each time (see: https://doc.qt.io/qt-5/qvector.html#append)
                    tracks.append(cueTrack);
                    // clear the state for the next track
                    trackType = trackNo = index = title = artist = composer = QString();
                }
                // -- end of repeated code --

                // here is a new track... get the actual track number and the track type
                if (!cmdVal.isEmpty()) {
                    cmdVal.remove('"').remove("'");
                    QStringList cmdValSplitted = cmdVal.split(QRegExp("\\s+"));
                    if (cmdValSplitted.size() == 2) {
                        trackNo = cmdValSplitted[0];
                        trackType = cmdValSplitted[1].toLower();
                    }
                }

            } else if (cmdCmd == constIndex) {
                // we need the index's position field
                //      note: only the "01" index is considered
                //      note: PREGAP and POSTGAP are NOT handled...
                if (!cmdVal.isEmpty()) {
                    cmdVal.remove('"').remove("'");
                    QStringList cmdValSplitted = cmdVal.split(QRegExp("\\s+"));
                    // if there's none "01" index, we'll just take the first one
                    // also, we'll take the "01" index even if it's the last one
                    if (cmdValSplitted.size() == 2 && (cmdValSplitted[0]==QLatin1String("01") || cmdValSplitted[0]==QLatin1String("1") || index.isEmpty())) {
                        index = cmdValSplitted[1];
                    }
                }
            } else if (cmdCmd == constTitle && !cmdVal.isEmpty()) {
                title = cmdVal;
            } else if (cmdCmd == constPerformer && !cmdVal.isEmpty()) {
                artist = cmdVal;  // is PERFORMER in the CUE file
            } else if (cmdCmd == constComposer && !cmdVal.isEmpty()) {
                composer = cmdVal;
            // } else if (...) {
            // ... just ignore the rest of possible field types for now...
            }
        }
    // the next line in the CUE file...
    }

    // we didn't add the last track yet... (repeating code)

    // -- start of repeated code --
    if (isValidFile && !index.isEmpty() && (trackType == constAudioTrackType || trackType.isEmpty())) {
        DBUG << "CUE tracks:" << tracks.size();
        // finalize albumArtist, artist, and composer...
        if (artist.isEmpty() && !albumArtist.isEmpty() && !isVariousArtists(albumArtist)) {
            artist = albumArtist;
        }
        if (composer.isEmpty() && !albumComposer.isEmpty() && !isVariousArtists(albumComposer)) {
            composer = albumComposer;
        }
        // set fallbacks...
        if (albumArtist.isEmpty()) {
            albumArtist = Song::unknown();
        }
        if (artist.isEmpty()) {
            artist = Song::unknown();
        }
        if (composer.isEmpty()) {
            composer = Song::unknown();
        }
        if (title.isEmpty()) {
            title = Song::unknown();
        }
        DBUG << "file:" << file << ", trackNo:" << trackNo << ", title:" << title << ", type:" << trackType << ", index:" << index
             << ", artist:" << artist << ", composer:" << composer;
        // update track hash values
        //      note: using insert() means that if there is already an item with the key, its value is replaced with the
        //            new value avoiding allocation of unnecessary items (see: https://doc.qt.io/qt-5/qhash.html#insert)
        cueTrack.insert("file",file);
        cueTrack.insert("trackNo",trackNo);
        cueTrack.insert("index",index);
        cueTrack.insert("title",title);
        cueTrack.insert("artist",artist);
        cueTrack.insert("composer",composer);
        // save track
        //     note: using append() means growing without reallocating the entire vector each time (see: https://doc.qt.io/qt-5/qvector.html#append)
        tracks.append(cueTrack);
        // clear the state for the next track
        trackType = trackNo = index = title = artist = composer = QString();
    }
    // -- end of repeated code --

    DBUG << "CUE tracks:" << tracks.size();

    // check if the CUE file has valid tracks...
    if (tracks.size() == 0) {
        return false;
    }

    // finalize songs
    for (int i = 0; i < tracks.size(); i++) {
        // note: using at() to lookup values from the track vector is slightly faster than
        //       using operator[] or value() (see: https://doc.qt.io/qt-5/qvector.html#value)
        // note: using value() to lookup values from the track hash is the recommended way
        //       (see: https://doc.qt.io/qt-5/qhash.html#details)
        Song song;
        song.file=constCueProtocol+fileName+"?pos="+QString::number(i);
        song.disc=static_cast<quint8>(discNo.toUInt());
        song.track=static_cast<quint8>(tracks.at(i).value("trackNo").toUInt());
        QString songFile=fileDir+tracks.at(i).value("file");
        song.setName(songFile); // HACK!!!
        if (!files.contains(songFile)) {
            files.insert(songFile);
        }
        // set time...
        //      note: the last TRACK for every FILE gets it's 'end' marker from the media file's length
        //            (this will be calculated elsewhere when calling MPDParseUtils::parseDirItems)
        if (i+1 < tracks.size() && tracks.at(i).value("file") == tracks[i+1].value("file")) {
            int time=0;
            // incorrect indices?
            if (!indexTime(tracks.at(i).value("index"), tracks[i+1].value("index"), time)) {
                continue;  // if incorrect, then skip the track (jump to the next one)
            }
            song.time = static_cast<quint16>(time);
        } else {
            // incorrect index?
            if (!indexLastTime(tracks.at(i).value("index"), lastTrackIndex)) {
                continue;  // if incorrect, then skip the track (jump to the next one)
            }
        }
        // now finalize remaining tags...
        if (genre.isEmpty()) {
            song.addGenre(Song::unknown());
        } else {
            for (const auto &g: genre) {
                song.addGenre(g);
            }
        }
        song.album=album;
        song.albumartist=albumArtist;
        song.year=static_cast<quint16>(date.toUInt());
        song.origYear=static_cast<quint16>(origYear.toUInt());
        song.title=tracks.at(i).value("title");
        song.artist=tracks.at(i).value("artist");
        if (!tracks.at(i).value("composer").isEmpty()) {
            song.setComposer(tracks.at(i).value("composer"));
        }
//        if (!remark.isEmpty()) {
//            song.setComment(remark.trimmed());
//        }
        DBUG << "SONG file:" << song.file << ", songFile:" << songFile << ", genre:" << song.displayGenre() << ", album:" << song.album
             << ", year:" << song.year << ", originalYear:" << song.origYear << ", discNo:" << song.disc << ", trackNo:" << song.track
             << ", title:" << song.title << ", albumArtist:" << song.albumartist << ", artist:" << song.artist << ", composer:" << song.composer();

        // finished updating this song: append!!!
        songList.append(song);
    }

    return true;
}
