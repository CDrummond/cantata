/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QTextCodec>
#include <QTextStream>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "CueFile"
void CueFile::enableDebug()
{
    debugEnabled=true;
}

static const QString constFileLineRegExp = QLatin1String("(\\S+)\\s+(?:\"([^\"]+)\"|(\\S+))\\s*(?:\"([^\"]+)\"|(\\S+))?");
static const QString constIndexRegExp = QLatin1String("(\\d{1,3}):(\\d{2}):(\\d{2})");
static const QString constPerformer = QLatin1String("performer");
static const QString constTitle = QLatin1String("title");
static const QString constSongWriter = QLatin1String("songwriter");
static const QString constComposer = QLatin1String("composer");
static const QString constFile = QLatin1String("file");
static const QString constTrack = QLatin1String("track");
static const QString constDisc = QLatin1String("discnumber");
static const QString constIndex = QLatin1String("index");
static const QString constAudioTrackType = QLatin1String("audio");
static const QString constRem = QLatin1String("rem");
static const QString constGenre = QLatin1String("genre");
static const QString constDate = QLatin1String("date");
static const QString constOriginalDate = QLatin1String("originaldate");
static const QString constCueProtocol = QLatin1String("cue:///");

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

// A single TRACK entry in .cue file.
struct CueEntry {
    QString file;
    QString index;
    int trackNo;
    int discNo;
    QString title;
    QString artist;
    QString albumArtist;
    QString album;
    QString performer;
    QString composer;
    QString albumComposer;
    QString genre;
    QString date;
    QString originalDate;

    CueEntry(QString &file, QString &index, int trackNo, int discNo, QString &title, QString &artist, QString &albumArtist, QString &album,
            QString &performer, QString &composer, QString &albumComposer, QString &genre, QString &date, QString &originalDate) {
        this->file = file;
        this->index = index;
        this->trackNo = trackNo;
        this->discNo = discNo;
        this->title = title;
        this->artist = artist;
        this->albumArtist = albumArtist;
        this->album = album;
        this->performer = performer;
        this->composer = composer;
        this->albumComposer = albumComposer;
        this->genre = genre;
        this->date = date;
        this->originalDate = originalDate;
    }
};
static const double constMsecPerSec  = 1000.0;

static double indexToMarker(const QString &index)
{
    QRegExp indexRegexp(constIndexRegExp);
    if (!indexRegexp.exactMatch(index)) {
        return -1.0;
    }

    QStringList splitted = indexRegexp.capturedTexts().mid(1, -1);
    qlonglong frames = splitted.at(0).toLongLong() * 60 * 75 + splitted.at(1).toLongLong() * 75 + splitted.at(2).toLongLong();
    return (frames * constMsecPerSec) / 75.0;
}

// This and the constFileLineRegExp do most of the "dirty" work, namely: splitting the raw .cue
// line into logical parts and getting rid of all the unnecessary whitespaces and quoting.
static QStringList splitCueLine(const QString &line)
{
    QRegExp lineRegexp(constFileLineRegExp);
    if (!lineRegexp.exactMatch(line.trimmed())) {
        return QStringList();
    }

    // let's remove the empty entries while we're at it
    return lineRegexp.capturedTexts().filter(QRegExp(".+")).mid(1, -1);
}

// Updates the song with data from the .cue entry. This one mustn't be used for the
// last song in the .cue file.
static bool updateSong(const CueEntry &entry, const QString &nextIndex, Song &song)
{
    double beginning = indexToMarker(entry.index);
    double end = indexToMarker(nextIndex);

    // incorrect indices (we won't be able to calculate beginning or end)
    if (beginning<0 || end<0) {
        DBUG << "Failed to update" << entry.title << beginning << end;
        return false;
    }

    song.title=entry.title;
    song.artist=entry.artist;
    song.album=entry.album;
    song.albumartist=entry.albumArtist;
    song.addGenre(entry.genre);
    song.year=entry.date.toInt();
    song.origYear=entry.originalDate.toInt();
    song.disc=entry.discNo;
    song.track=entry.trackNo;
    song.time=(quint16)(((end-beginning)/constMsecPerSec)+0.5);
    if (!entry.performer.isEmpty()) {
        song.setPerformer(entry.performer);
    }
    if (!entry.composer.isEmpty()) {
        song.setComposer(entry.composer);
    } else if (!entry.albumComposer.isEmpty()) {
        song.setComposer(entry.albumComposer);
    }
    return true;
}

// Updates the song with data from the .cue entry. This one must be used only for the
// last song in the .cue file.
static bool updateLastSong(const CueEntry &entry, Song &song, double &lastTrackIndex)
{
    double beginning = indexToMarker(entry.index);

    // incorrect index (we won't be able to calculate beginning)
    if (beginning<0) {
        return false;
    }
    lastTrackIndex = beginning;

    song.title=entry.title;
    song.artist=entry.artist;
    song.album=entry.album;
    song.albumartist=entry.albumArtist;
    song.addGenre(entry.genre);
    song.year=entry.date.toInt();
    song.origYear=entry.originalDate.toInt();
    song.disc=entry.discNo;
    song.track=entry.trackNo;

    if (!entry.performer.isEmpty()) {
        song.setPerformer(entry.performer);
    }
    if (!entry.composer.isEmpty()) {
        song.setComposer(entry.composer);
    } else if (!entry.albumComposer.isEmpty()) {
        song.setComposer(entry.albumComposer);
    }
    return true;
}

// Get list of text codecs used to decode CUE files...
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

static inline bool isVariousArtists(const QString &str)
{
    return str==Song::variousArtists() || str.toLower() == "various" || str.toLower() == "various artists";
}

// Parse CUE file content
//
// Supported CUE tags (and corresponding MPD tags) are:
// -------------------isGenreInComposerSupport
// FILE HEADER SECTION
// -------------------
// REM GENRE "genre"                            (genre)
// REM DATE "YYYY"                              (date)
// REM ORIGINALDATE "YYYY"                      (originaldate)
// REM DISC "N"                                 (disc)
// REM COMPOSER "composer"                      (composer)
// COMPOSER "composer"  [relaxed CUE syntax]    (composer)
// PERFORMER "performer"                        (performer)
// TITLE "album"                                (album)
// FILE "filename" filetype
// ----------------
// TRACK(S) SECTION
// ----------------
// TRACK NN datatype
// INDEX NN MM:SS:FF
// TITLE "title"                                (title)
// REM COMPOSER "composer"                      (composer)
// COMPOSER "composer"  [relaxed CUE syntax]    (composer)
// PERFORMER "performer"                        (performer)
//
bool CueFile::parse(const QString &fileName, const QString &dir, QList<Song> &songList, QSet<QString> &files, double &lastTrackIndex)
{
    DBUG << fileName;
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

    // read the first line already
    QString line = textStream->readLine();
    QList<CueEntry> entries;
    QString fileDir=fileName.contains("/") ? Utils::getDir(fileName) : QString();

    // -- whole file
    while (!textStream->atEnd()) {
        QString albumArtist;
        QString album;
        QString albumComposer;
        QString file;
        QString fileType;
        QString genre;
        QString date;
        QString originalDate;
        int disc = 0;

        // -- FILE section
        do {
            QStringList splitted = splitCueLine(line);

            // uninteresting or incorrect line
            if (splitted.size() < 2) {
                continue;
            }

            QString lineName = splitted[0].toLower();
            QString lineValue = splitted[1];

            if (lineName == constPerformer) {
                albumArtist = lineValue;
            } else if (lineName == constComposer) {
                // this is to handle e relaxed CUE syntax for COMPOSER (which is: COMPOSER "composer")
                albumComposer = lineValue;
            } else if (lineName == constSongWriter && albumComposer.isEmpty()) {
                albumComposer = lineValue;
            } else if (lineName == constTitle) {
                album = lineValue;
            } else if (lineName == constFile) {
                file = lineValue;
                if (splitted.size() > 2) {
                    fileType = splitted[2];
                }
            } else if (lineName == constRem) {
                // notice that the 'REM comment' (mpd tag: 'comment') is NOT being handled (it should require some code rewriting...)
                if (splitted.size() < 3) {
                    break;
                }
                lineName = splitted[1].toLower();
                lineValue = splitted[2];
                if (lineName == constGenre) {
                    genre = lineValue;
                } else if (lineName == constDate) {
                    date = lineValue;
                } else if (lineName == constOriginalDate) {
                    originalDate = lineValue;
                } else if (lineName == constDisc) {
                    disc = lineValue.toInt();
                } else if (lineName == constComposer) {
                    // this is to handle the strict CUE syntax for COMPOSER (which is: REM COMPOSER "composer")
                    albumComposer = lineValue;
                } // here should be added additional code for 'LABEL label' (mpd tag: 'label') and other possible CUE commands...
            // end of the header -> go into the track mode
            } else if (lineName == constTrack) {
                break;
            }
            // just ignore the rest of possible field types for now...
        } while (!(line = textStream->readLine()).isNull());

        bool isGenreInComposerSupport=Song::composerGenres().contains(genre);

        DBUG << "FILE section -- genre:" << genre << "isGenreInComposerSupport?" << isGenreInComposerSupport << "albumArtist:" << albumArtist << "albumComposer:" << albumComposer;

        if (line.isNull()) {
            return false;
        }

        // if this is a data file, all of it's tracks will be ignored
        bool validFile = fileType.compare("BINARY", Qt::CaseInsensitive) && fileType.compare("MOTOROLA", Qt::CaseInsensitive);

        QString trackType;
        QString index;
        QString artist;
        QString composer;
        QString performer;
        QString title;
        int trackNo=0;

        // TRACK section
        do {
            QStringList splitted = splitCueLine(line);

            // uninteresting or incorrect line
            if (splitted.size() < 2) {
                continue;
            }

            QString lineName = splitted[0].toLower();
            QString lineValue = splitted[1];
            QString lineAdditional = splitted.size() > 2 ? splitted[2].toLower() : QString();

            if (lineName == constTrack) {
                // the beginning of another track's definition - we're saving the current one
                // for later (if it's valid of course)
                //
                // please note that the same code is repeated just after this 'do-while' loop
                // before saving, set artist/albumArtist for the track...
               
                if (artist.isEmpty()) {
                    if (!albumArtist.isEmpty() && !isVariousArtists(albumArtist)) {
                        artist = albumArtist;
                    } else if (!performer.isEmpty()) {
                        artist = performer;
                    }
                }
                if (composer.isEmpty() && !albumComposer.isEmpty() && !isVariousArtists(albumComposer)) {
                    composer = albumComposer;
                }
                DBUG << "TRACK saving - " "valid?" << validFile << "| index:" << index << "| type:" << trackType
                     << "| genre:" << genre << "| albumArtist:" << albumArtist << "| albumComposer:" << albumComposer << "| artist:" << artist
                     << "| performer:" << performer << "| composer:" << composer << "| originalDate:" << originalDate;
                if (validFile && !index.isEmpty() && (trackType.isEmpty() || trackType == constAudioTrackType)) {
                    entries.append(CueEntry(file, index, trackNo, disc, title, artist, albumArtist, album, performer, composer, albumComposer, genre, date, originalDate));
                }

                // clear the state
                trackType = index = artist = composer = performer = title = QString();

                if (!lineAdditional.isEmpty()) {
                    trackType = lineAdditional;
                }
                trackNo = lineValue.toInt();
            } else if (lineName == constIndex) {
                // we need the index's position field
                // note: PREGAP and POSTGAP are NOT handled...
                if (!lineAdditional.isEmpty()) {
                    // if there's none "01" index, we'll just take the first one
                    // also, we'll take the "01" index even if it's the last one
                    if (QLatin1String("01")==lineValue || index.isEmpty()) {
                        index = lineAdditional;
                    }
                }
            } else if (lineName == constTitle) {
                title = lineValue;
            } else if (lineName == constPerformer) {
                if (!lineValue.isEmpty()) {
                    performer = lineValue;
                }
            } else if (lineName == constComposer) {
                // relaxed CUE syntax (COMPOSER "composer")
                if (!lineValue.isEmpty()) {
                    composer = lineValue;
                }
            } else if (lineName == constSongWriter && composer.isEmpty()) {
                if (!lineValue.isEmpty()) {
                    composer = lineValue;
                }
            } else if (lineName == constRem) {
                // strict CUE syntax (REM COMPOSER "composer")
                lineName = splitted[1].toLower();
                lineValue = splitted[2];
                if (lineName == constComposer) {
                    if (!lineValue.isEmpty()) {
                        composer = lineValue;
                    }
                }
            }
            // end of track's for the current file -> parse next one
            if (lineName == constFile) {
                break;
            }
            // just ignore the rest of possible field types for now...
        } while (!(line = textStream->readLine()).isNull());

        // we didn't add the last song yet... (repeating code)
        if (artist.isEmpty()) {
            if (!albumArtist.isEmpty() && !isVariousArtists(albumArtist)) {
                artist = albumArtist;
            } else if (!performer.isEmpty()) {
                artist = performer;
            }
        }
        if (composer.isEmpty() && !albumComposer.isEmpty() && !isVariousArtists(albumComposer)) {
            composer = albumComposer;
        }
        DBUG << "TRACK saving - " "valid?" << validFile << "| index:" << index << "| type:" << trackType;
        DBUG << "... genre:" << genre << "| albumArtist:" << albumArtist << "| albumComposer:" << albumComposer << "| artist:" << artist << "| performer:" << performer << "| composer:" << composer;
        if (validFile && !index.isEmpty() && (trackType.isEmpty() || trackType == constAudioTrackType)) {
            entries.append(CueEntry(file, index, trackNo, disc, title, artist, albumArtist, album, performer, composer, albumComposer, genre, date, originalDate));
        }
    }

    DBUG << "num entries:" << entries.length();
    // finalize parsing songs
    for(int i = 0; i < entries.length(); i++) {
        CueEntry entry = entries.at(i);

        Song song;
        song.file=constCueProtocol+fileName+"?pos="+QString::number(i);
        song.track=entry.trackNo;
        song.disc=entry.discNo;
        QString songFile=fileDir+entry.file;
        song.setName(songFile); // HACK!!!
        if (!files.contains(songFile)) {
            files.insert(songFile);
        }
        // the last TRACK for every FILE gets it's 'end' marker from the media file's
        // length
        if (i+1 < entries.size() && entries.at(i).file == entries.at(i+1).file) {
            // incorrect indices?
            if (!updateSong(entry, entries.at(i+1).index, song)) {
                continue;
            }
        } else {
            // incorrect index?
            if (!updateLastSong(entry, song, lastTrackIndex)) {
                continue;
            }
        }

        songList.append(song);
    }

    return true;
}
