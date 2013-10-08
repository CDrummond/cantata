/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "utils.h"
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
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

static const QString constFileLineRegExp = QLatin1String("(\\S+)\\s+(?:\"([^\"]+)\"|(\\S+))\\s*(?:\"([^\"]+)\"|(\\S+))?");
static const QString constIndexRegExp = QLatin1String("(\\d{2,3}):(\\d{2}):(\\d{2})");
static const QString constPerformer = QLatin1String("performer");
static const QString constTitle = QLatin1String("title");
//static const QString constSongWriter = QLatin1String("songwriter");
static const QString constFile = QLatin1String("file");
static const QString constTrack = QLatin1String("track");
static const QString constIndex = QLatin1String("index");
static const QString constAudioTrackType = QLatin1String("audio");
static const QString constRem = QLatin1String("rem");
static const QString constGenre = QLatin1String("genre");
static const QString constDate = QLatin1String("date");
static const QString constCueProtocol = QLatin1String("cue:///");

bool CueFile::isCue(const QString &str)
{
    return str.startsWith(constCueProtocol);
}

QByteArray CueFile::getLoadLine(const QString &str)
{
    QUrl u(str);
    #if QT_VERSION < 0x050000
    const QUrl &q=u;
    #else
    QUrlQuery q(u);
    #endif

    if (q.hasQueryItem("pos")) {
        QString pos=q.queryItemValue("pos");
        QString path=u.path();
        if (path.startsWith("/")) {
            path=path.mid(1);
        }
        return MPDConnection::encodeName(path)+" "+pos.toLatin1()+":"+QString::number(pos.toInt()+1).toLatin1();
    }
    return MPDConnection::encodeName(str);
}

// A single TRACK entry in .cue file.
struct CueEntry {
    QString file;
    QString index;
    int trackNo;
    QString title;
    QString artist;
    QString albumArtist;
    QString album;
//    QString composer;
//    QString albumComposer;
    QString genre;
    QString date;

    CueEntry(QString &file, QString &index, int trackNo, QString &title, QString &artist, QString &albumArtist,
             QString &album, /*QString &composer, QString &albumComposer,*/ QString &genre, QString &date) {
        this->file = file;
        this->index = index;
        this->trackNo = trackNo;
        this->title = title;
        this->artist = artist;
        this->albumArtist = albumArtist;
        this->album = album;
//        this->composer = composer;
//        this->albumComposer = albumComposer;
        this->genre = genre;
        this->date = date;
    }
};
static const qint64 constMsecPerSec  = 1000;

static qint64 indexToMarker(const QString& index)
{
    QRegExp indexRegexp(constIndexRegExp);
    if (!indexRegexp.exactMatch(index)) {
        return -1;
    }

    QStringList splitted = indexRegexp.capturedTexts().mid(1, -1);
    qlonglong frames = splitted.at(0).toLongLong() * 60 * 75 + splitted.at(1).toLongLong() * 75 + splitted.at(2).toLongLong();
    return (frames * constMsecPerSec) / 75;
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
    qint64 beginning = indexToMarker(entry.index);
    qint64 end = indexToMarker(nextIndex);

    // incorrect indices (we won't be able to calculate beginning or end)
    if (-1==beginning || -1==end) {
        return false;
    }

    song.title=entry.title;
    song.artist=entry.artist;
    song.album=entry.album;
    song.albumartist=entry.albumArtist;
    song.genre=entry.genre;
    song.year=entry.date.toInt();
    song.time=(end-beginning)/constMsecPerSec;
    return true;
}

// Updates the song with data from the .cue entry. This one must be used only for the
// last song in the .cue file.
static bool updateLastSong(const CueEntry &entry, Song &song)
{
    qint64 beginning = indexToMarker(entry.index);

    // incorrect index (we won't be able to calculate beginning)
    if (-1==beginning ) {
        return false;
    }

    song.title=entry.title;
    song.artist=entry.artist;
    song.album=entry.album;
    song.albumartist=entry.albumArtist;
    song.genre=entry.genre;
    song.year=entry.date.toInt();
    return true;
}

bool CueFile::parse(const QString &fileName, const QString &dir, QList<Song> &songList, QSet<QString> &files)
{
    QFile f(dir+fileName);
    if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) {
        return false;
    }

    QTextStream textStream(&f);
    textStream.setCodec(QTextCodec::codecForUtfText(f.peek(1024), QTextCodec::codecForName("UTF-8")));

    // read the first line already
    QString line = textStream.readLine();
    QList<CueEntry> entries;
    QString fileDir=fileName.contains("/") ? Utils::getDir(fileName) : QString();

    // -- whole file
    while (!textStream.atEnd()) {
        QString albumArtist;
        QString album;
//        QString albumComposer;
        QString file;
        QString fileType;
        QString genre;
        QString date;

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
            } else if (lineName == constTitle) {
                album = lineValue;
            } /*else if (lineName == constSongWriter) {
                albumComposer = lineValue;
            }*/ else if (lineName == constFile) {
                file = lineValue;
                if (splitted.size() > 2) {
                    fileType = splitted[2];
                }
            } else if (lineName == constRem) {
                if (splitted.size() < 3) {
                    break;
                }

                if (lineValue.toLower() == constGenre) {
                    genre = splitted[2];
                } else if (lineValue.toLower() == constDate) {
                    date = splitted[2];
                }
                // end of the header -> go into the track mode
            } else if (lineName == constTrack) {
                break;
            }

            // just ignore the rest of possible field types for now...
        } while (!(line = textStream.readLine()).isNull());

        if (line.isNull()) {
            return false;
        }

        // if this is a data file, all of it's tracks will be ignored
        bool validFile = fileType.compare("BINARY", Qt::CaseInsensitive) && fileType.compare("MOTOROLA", Qt::CaseInsensitive);
        QString trackType;
        QString index;
        QString artist;
//        QString composer;
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
                // please note that the same code is repeated just after this 'do-while' loop
                if (validFile && !index.isEmpty() && (trackType.isEmpty() || trackType == constAudioTrackType)) {
                    entries.append(CueEntry(file, index, trackNo, title, artist, albumArtist, album, /*composer, albumComposer,*/ genre, date));
                }

                // clear the state
                trackType = index = artist = title = QString();

                if (!lineAdditional.isEmpty()) {
                    trackType = lineAdditional;
                }
                trackNo = lineValue.toInt();
            } else if (lineName == constIndex) {
                // we need the index's position field
                if (!lineAdditional.isEmpty()) {

                    // if there's none "01" index, we'll just take the first one
                    // also, we'll take the "01" index even if it's the last one
                    if (QLatin1String("01")==lineValue || index.isEmpty()) {
                        index = lineAdditional;
                    }
                }
            } else if (lineName == constPerformer) {
                artist = lineValue;
            } else if (lineName == constTitle) {
                title = lineValue;
            } /*else if (lineName == constSongWriter) {
                composer = lineValue;
                // end of track's for the current file -> parse next one
            }*/ else if (lineName == constFile) {
                break;
            }
            // just ignore the rest of possible field types for now...
        } while (!(line = textStream.readLine()).isNull());

        // we didn't add the last song yet...
        if (validFile && !index.isEmpty() && (trackType.isEmpty() || trackType == constAudioTrackType)) {
            entries.append(CueEntry(file, index, trackNo, title, artist, albumArtist, album, /*composer, albumComposer,*/ genre, date));
        }
    }

    // finalize parsing songs
    for(int i = 0; i < entries.length(); i++) {
        CueEntry entry = entries.at(i);

        Song song;
        song.file=constCueProtocol+fileName+"?pos="+QString::number(i);
        song.track=entry.trackNo;
        song.name=fileDir+entry.file; // HACK!!!
        if (!files.contains(song.name)) {
            files.insert(song.name);
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
            if (!updateLastSong(entry, song)) {
                continue;
            }
        }

        songList.append(song);
    }

    return true;
}
