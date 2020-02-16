/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "playqueuemodel.h"
#include "mpd-interface/mpdparseutils.h"
#include "mpd-interface/mpdstats.h"
#include "mpd-interface/cuefile.h"
#include "streams/streamfetcher.h"
#include "streamsmodel.h"
#include "http/httpserver.h"
#include "gui/settings.h"
#include "support/icon.h"
#include "support/monoicon.h"
#include "support/utils.h"
#include "config.h"
#include "support/action.h"
#include "support/actioncollection.h"
#include "support/globalstatic.h"
#include "gui/covers.h"
#include "widgets/groupedview.h"
#include "widgets/icons.h"
#include "roles.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devicesmodel.h"
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "devices/audiocddevice.h"
#endif
#endif
#include <QPalette>
#include <QFont>
#include <QFile>
#include <QDir>
#include <QModelIndex>
#include <QMimeData>
#include <QTextStream>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QApplication>
#include <QMenu>
#include <QXmlStreamReader>
#include <algorithm>

GLOBAL_STATIC(PlayQueueModel, instance)

const QLatin1String PlayQueueModel::constMoveMimeType("cantata/move");
const QLatin1String PlayQueueModel::constFileNameMimeType("cantata/filename");
const QLatin1String PlayQueueModel::constUriMimeType("text/uri-list");

static const char * constSortByKey="sort-by";
static const QLatin1String constSortByArtistKey("artist");
static const QLatin1String constSortByAlbumArtistKey("albumartist");
static const QLatin1String constSortByAlbumKey("album");
static const QLatin1String constSortByGenreKey("genre");
static const QLatin1String constSortByYearKey("year");
static const QLatin1String constSortByComposerKey("composer");
static const QLatin1String constSortByPerformerKey("performer");
static const QLatin1String constSortByTitleKey("title");
static const QLatin1String constSortByNumberKey("track");

static QSet<QString> constM3uPlaylists = QSet<QString>() << QLatin1String("m3u") << QLatin1String("m3u8");
static const QString constPlsPlaylist = QLatin1String("pls");
static const QString constXspfPlaylist = QLatin1String("xspf");
QSet<QString> PlayQueueModel::constFileExtensions = QSet<QString>()
                                                  << QLatin1String("mp3") << QLatin1String("ogg") << QLatin1String("flac") << QLatin1String("wma") << QLatin1String("m4a")
                                                  << QLatin1String("m4b") << QLatin1String("mp4") << QLatin1String("m4p") << QLatin1String("wav") << QLatin1String("wv")
                                                  << QLatin1String("wvp") << QLatin1String("aiff") << QLatin1String("aif") << QLatin1String("aifc") << QLatin1String("ape")
                                                  << QLatin1String("spx") << QLatin1String("tta") << QLatin1String("mpc") << QLatin1String("mpp") << QLatin1String("mp+")
                                                  << QLatin1String("dff") << QLatin1String("dsf") << QLatin1String("opus")
                                                  // And playlists...
                                                  << QLatin1String("m3u") << QLatin1String("m3u8") << constPlsPlaylist << constXspfPlaylist;

static inline QString getExtension(const QString &file)
{
    int pos=file.lastIndexOf('.');
    return pos>0 ? file.mid(pos+1).toLower() : QString();
}

static inline bool checkExtension(const QString &file, const QSet<QString> &exts = PlayQueueModel::constFileExtensions)
{
    return exts.contains(getExtension(file));
}

static QString fileUrl(const QString &file, bool useServer, bool useLocal)
{
    if (useServer) {
        QByteArray path = HttpServer::self()->encodeUrl(file);
        if (!path.isEmpty()) {
            return path;
        }
    } else if (useLocal && !file.startsWith(QLatin1String("/media/"))) {
        return QLatin1String("file://")+file;
    }
    return QString();
}

static QString checkUrl(const QString &url, const QDir &dir, bool useServer, bool useLocal, const QSet<QString> &handlers)
{
    int pos = url.indexOf(QLatin1String("://"));
    QString handler = pos>0 ? url.left(pos+3).toLower() : QString();
    if (!handler.isEmpty() && (QLatin1String("http://")==handler || QLatin1String("https://")==handler)) {
        // Radio stream?
        return StreamsModel::constPrefix+url;
    } else if (handlers.contains(handler)) {
        return url;
    } else if (checkExtension(url)) {
        QString path = dir.filePath(url);
        if (QFile::exists(path)) { // Relative
            return fileUrl(path, useServer, useLocal);
        } else if (QFile::exists(url)) { // Absolute
            return fileUrl(url, useServer, useLocal);
        }
    }
    return QString();
}

static QStringList expandM3uPlaylist(const QString &playlist, bool useServer, bool useLocal, const QSet<QString> &handlers)
{
    QStringList files;
    QFile f(playlist);
    QDir dir(Utils::getDir(playlist));

    if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (!line.startsWith(QLatin1Char('#'))) {
                QString url = checkUrl(line, dir, useServer, useLocal, handlers);
                if (!url.isEmpty()) {
                    files.append(url);
                }
            }
        }
        f.close();
    }
    return files;
}

static QStringList expandPlsPlaylist(const QString &playlist, bool useServer, bool useLocal, const QSet<QString> &handlers)
{
    QStringList files;
    QFile f(playlist);
    QDir dir(Utils::getDir(playlist));
    QMap<unsigned int, QString> titles;
    QMap<unsigned int, QString> urls;

    if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(QLatin1String("File"))) {
                QStringList parts=line.split("=");
                if (2==parts.length()) {
                    QString path=parts[1].trimmed();
                    unsigned int num=parts[0].left(4).toUInt();
                    QString url=checkUrl(path, dir, useServer, useLocal, handlers);
                    if (!url.isEmpty()) {
                        urls.insert(num, url);
                    }
                }
            } else if (line.startsWith(QLatin1String("Title"))) {
                QStringList parts=line.split("=");
                if (2==parts.length()) {
                    titles.insert(parts[0].left(5).toUInt(), parts[1].trimmed());
                }
            }
        }
        f.close();
    }

    auto it = urls.constBegin();
    auto end = urls.constEnd();
    for (; it!=end; ++it) {
        if (titles.contains(it.key()) && (it.value().startsWith(QLatin1String("http://")) || it.value().startsWith("https://"))) {
            files.append(it.value()+"#"+titles[it.key()]);
        } else {
            files.append(it.value());
        }
    }
    return files;
}

static QStringList expandXspfPlaylist(const QString &playlist, bool useServer, bool useLocal, const QSet<QString> &handlers)
{
    QStringList files;
    QFile f(playlist);

    if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QXmlStreamReader reader(&f);
        QDir dir(Utils::getDir(playlist));

        while (!reader.atEnd()) {
            reader.readNext();
            if (QXmlStreamReader::StartElement==reader.tokenType() && QLatin1String("location")==reader.name()) {
                QString url=checkUrl(reader.readElementText().trimmed(), dir, useServer, useLocal, handlers);
                if (!url.isEmpty()) {
                    files.append(url);
                }
            }
        }
        f.close();
    }

    return files;
}

static QStringList listFiles(const QDir &d, bool useServer, bool useLocal, const QSet<QString> &handlers, int level=5)
{
    QStringList files;
    if (level) {
        for (const auto &f: d.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks, QDir::LocaleAware|QDir::IgnoreCase)) {
            if (f.isDir()) {
                files += listFiles(QDir(f.absoluteFilePath()), useServer, useLocal, handlers, level-1);
            } else if (checkExtension(f.fileName(), constM3uPlaylists)) {
                files += expandM3uPlaylist(f.absoluteFilePath(), useServer, useLocal, handlers);
            } else if (constPlsPlaylist == getExtension(f.fileName())) {
                files += expandPlsPlaylist(f.absoluteFilePath(), useServer, useLocal, handlers);
            } else if (constXspfPlaylist == getExtension(f.fileName())) {
                files += expandXspfPlaylist(f.absoluteFilePath(), useServer, useLocal, handlers);
            } else if (checkExtension(f.fileName())) {
                QString fUrl=fileUrl(f.absoluteFilePath(), useServer, useLocal);
                if (!fUrl.isEmpty()) {
                    files.append(fUrl);
                }
            }
        }
    }
    return files;
}

QStringList PlayQueueModel::parseUrls(const QStringList &urls)
{
    QStringList useable;
    bool useServer = HttpServer::self()->isAlive();
    bool useLocal = MPDConnection::self()->localFilePlaybackSupported();
    QSet<QString> handlers = MPDConnection::self()->urlHandlers();
    bool haveLocalFiles = false;

    for (const auto &path: urls) {
        QUrl u=path.indexOf("://")>2 ? QUrl(path) : QUrl::fromLocalFile(path);
        #if defined ENABLE_DEVICES_SUPPORT && (defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND)
        QString cdDevice=AudioCdDevice::getDevice(u);
        if (!cdDevice.isEmpty()) {
            DevicesModel::self()->playCd(cdDevice);
        } else
        #endif
        if (QLatin1String("http")==u.scheme()) {
            useable.append(u.toString());
        } else if (u.scheme().isEmpty() || QLatin1String("file")==u.scheme()) {
            haveLocalFiles = true;
            QDir d(u.path());

            if (d.exists()) {
                useable = listFiles(d, useServer, useLocal, handlers);
            } else if (checkExtension(u.path(), constM3uPlaylists)) {
                useable += expandM3uPlaylist(u.path(), useServer, useLocal, handlers);
            } else if (constPlsPlaylist == getExtension(u.path())) {
                useable += expandPlsPlaylist(u.path(), useServer, useLocal, handlers);
            } else if (constXspfPlaylist == getExtension(u.path())) {
                useable += expandXspfPlaylist(u.path(), useServer, useLocal, handlers);
            } else if (checkExtension(u.path())) {
                QString fUrl = fileUrl(u.path(), useServer, useLocal);
                if (!fUrl.isEmpty()) {
                    useable.append(fUrl);
                }
            }
        }
    }

    QStringList use;

    if (haveLocalFiles && !useServer && !useLocal) {
        #ifdef ENABLE_HTTP_SERVER
        emit error(tr("Cannot add local files. Please enable in-built HTTP server, or configure MPD for local file playback."));
        #else
        emit error(tr("Cannot add local files. Please configure MPD for local file playback."));
        #endif
        return use;
    }

    // Ensure we only have unqiue URLs...
    QSet<QString> unique;
    for (const auto &u: useable) {
        if (!unique.contains(u)) {
            unique.insert(u);
            use.append(u);
        }
    }

    if (use.isEmpty()) {
        emit error(tr("Unable to add local files. No suitable files found."));
    }
    return use;
}

void PlayQueueModel::encode(QMimeData &mimeData, const QString &mime, const QStringList &values)
{
    QByteArray encodedData;
    QTextStream stream(&encodedData, QIODevice::WriteOnly);

    for (const QString &v: values) {
        stream << v << endl;
    }

    mimeData.setData(mime, encodedData);
}

void PlayQueueModel::encode(QMimeData &mimeData, const QString &mime, const QList<quint32> &values)
{
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    for (quint32 v: values) {
        stream << v;
    }
    mimeData.setData(mime, encodedData);
}

QStringList PlayQueueModel::decode(const QMimeData &mimeData, const QString &mime)
{
    QByteArray encodedData=mimeData.data(mime);
    QTextStream stream(&encodedData, QIODevice::ReadOnly);
    QStringList rv;

    while (!stream.atEnd()) {
        rv.append(stream.readLine().remove('\n'));
    }
    return rv;
}

QList<quint32> PlayQueueModel::decodeInts(const QMimeData &mimeData, const QString &mime)
{
    QByteArray encodedData=mimeData.data(mime);
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    QList<quint32> rv;

    while (!stream.atEnd()) {
        quint32 v;
        stream >> v;
        rv.append(v);
    }
    return rv;
}

QString PlayQueueModel::headerText(int col)
{
    switch (col) {
    case COL_TITLE:     return tr("Title");
    case COL_ARTIST:    return tr("Artist");
    case COL_ALBUM:     return tr("Album");
    case COL_TRACK:     return tr("#", "Track number");
    case COL_LENGTH:    return tr("Length");
    case COL_DISC:      return tr("Disc");
    case COL_YEAR:      return tr("Year");
    case COL_ORIGYEAR:  return tr("Original Year");
    case COL_GENRE:     return tr("Genre");
    case COL_PRIO:      return tr("Priority");
    case COL_COMPOSER:  return tr("Composer");
    case COL_PERFORMER: return tr("Performer");
    case COL_RATING:    return tr("Rating");
    case COL_FILENAME:  return tr("Filename");
    case COL_PATH:      return tr("Path");
    default:            return QString();
    }
}

PlayQueueModel::PlayQueueModel(QObject *parent)
    : QAbstractItemModel(parent)
    , currentSongId(-1)
    , currentSongRowNum(-1)
    , time(0)
    , mpdState(MPDState_Inactive)
    , stopAfterCurrent(false)
    , stopAfterTrackId(-1)
    , undoLimit(10)
    , undoEnabled(undoLimit>0)
    , lastCommand(Cmd_Other)
    , dropAdjust(0)
{
    fetcher=new StreamFetcher(this);
    connect(this, SIGNAL(modelReset()), this, SLOT(stats()));
    connect(fetcher, SIGNAL(result(const QStringList &, int, int, quint8, bool)), SLOT(addFiles(const QStringList &, int, int, quint8, bool)));
    connect(fetcher, SIGNAL(result(const QStringList &, int, int, quint8, bool)), SIGNAL(streamsFetched()));
    connect(fetcher, SIGNAL(status(QString)), SIGNAL(streamFetchStatus(QString)));
    connect(this, SIGNAL(filesAdded(const QStringList, quint32, quint32, int, quint8, bool)),
            MPDConnection::self(), SLOT(add(const QStringList, quint32, quint32, int, quint8, bool)));
    connect(this, SIGNAL(populate(QStringList, QList<quint8>)), MPDConnection::self(), SLOT(populate(QStringList, QList<quint8>)));
    connect(this, SIGNAL(move(const QList<quint32> &, quint32, quint32)),
            MPDConnection::self(), SLOT(move(const QList<quint32> &, quint32, quint32)));
    connect(this, SIGNAL(setOrder(const QList<quint32> &)), MPDConnection::self(), SLOT(setOrder(const QList<quint32> &)));
    connect(MPDConnection::self(), SIGNAL(prioritySet(const QMap<qint32, quint8> &)), SLOT(prioritySet(const QMap<qint32, quint8> &)));
    connect(MPDConnection::self(), SIGNAL(stopAfterCurrentChanged(bool)), SLOT(stopAfterCurrentChanged(bool)));
    connect(this, SIGNAL(stop(bool)), MPDConnection::self(), SLOT(stopPlaying(bool)));
    connect(this, SIGNAL(clearStopAfter()), MPDConnection::self(), SLOT(clearStopAfter()));
    connect(this, SIGNAL(removeSongs(QList<qint32>)), MPDConnection::self(), SLOT(removeSongs(QList<qint32>)));
    connect(this, SIGNAL(clearEntries()), MPDConnection::self(), SLOT(clear()));
    connect(this, SIGNAL(addAndPlay(QString)), MPDConnection::self(), SLOT(addAndPlay(QString)));
    connect(this, SIGNAL(startPlayingSongId(qint32)), MPDConnection::self(), SLOT(startPlayingSongId(qint32)));
    connect(this, SIGNAL(getRating(QString)), MPDConnection::self(), SLOT(getRating(QString)));
    connect(this, SIGNAL(setRating(QStringList,quint8)), MPDConnection::self(), SLOT(setRating(QStringList,quint8)));
    connect(MPDConnection::self(), SIGNAL(rating(QString,quint8)), SLOT(ratingResult(QString,quint8)));
    connect(MPDConnection::self(), SIGNAL(stickerDbChanged()), SLOT(stickerDbChanged()));
    #ifdef ENABLE_DEVICES_SUPPORT //TODO: Problems here with devices support!!!
    connect(DevicesModel::self(), SIGNAL(updatedDetails(QList<Song>)), SLOT(updateDetails(QList<Song>)));
    #endif

    removeDuplicatesAction=new Action(tr("Remove Duplicates"), this);
    removeDuplicatesAction->setEnabled(false);
    QColor col=Utils::monoIconColor();
    undoAction=ActionCollection::get()->createAction("playqueue-undo", tr("Undo"), MonoIcon::icon(FontAwesome::undo, col));
    undoAction->setShortcut(Qt::ControlModifier+Qt::Key_Z);
    redoAction=ActionCollection::get()->createAction("playqueue-redo", tr("Redo"), MonoIcon::icon(FontAwesome::repeat, col));
    redoAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+Qt::Key_Z);
    connect(undoAction, SIGNAL(triggered()), this, SLOT(undo()));
    connect(redoAction, SIGNAL(triggered()), this, SLOT(redo()));
    connect(removeDuplicatesAction, SIGNAL(triggered()), this, SLOT(removeDuplicates()));

    shuffleAction=new Action(tr("Shuffle"), this);
    shuffleAction->setMenu(new QMenu(nullptr));
    Action *shuffleTracksAction = new Action(tr("Tracks"), shuffleAction);
    Action *shuffleAlbumsAction = new Action(tr("Albums"), shuffleAction);
    connect(shuffleTracksAction, SIGNAL(triggered()), MPDConnection::self(), SLOT(shuffle()));
    connect(shuffleAlbumsAction, SIGNAL(triggered()), this, SLOT(shuffleAlbums()));
    shuffleAction->menu()->addAction(shuffleTracksAction);
    shuffleAction->menu()->addAction(shuffleAlbumsAction);

    sortAction=new Action(tr("Sort By"), this);
    sortAction->setMenu(new QMenu(nullptr));
    addSortAction(tr("Artist"), constSortByArtistKey);
    addSortAction(tr("Album Artist"), constSortByAlbumArtistKey);
    addSortAction(tr("Album"), constSortByAlbumKey);
    addSortAction(tr("Track Title"), constSortByTitleKey);
    addSortAction(tr("Track Number"), constSortByNumberKey);
    addSortAction(tr("Genre"), constSortByGenreKey);
    addSortAction(tr("Year"), constSortByYearKey);
    addSortAction(tr("Composer"), constSortByComposerKey);
    addSortAction(tr("Performer"), constSortByPerformerKey);
    controlActions();
    shuffleAction->setEnabled(false);
    sortAction->setEnabled(false);
    alignments[COL_TITLE]=alignments[COL_ARTIST]=alignments[COL_ALBUM]=alignments[COL_GENRE]=alignments[COL_COMPOSER]=
            alignments[COL_PERFORMER]=alignments[COL_FILENAME]=alignments[COL_PATH]=int(Qt::AlignVCenter|Qt::AlignLeft);
    alignments[COL_TRACK]=alignments[COL_LENGTH]=alignments[COL_DISC]=alignments[COL_YEAR]=alignments[COL_ORIGYEAR]=
            alignments[COL_PRIO]=int(Qt::AlignVCenter|Qt::AlignRight);
    alignments[COL_RATING]=int(Qt::AlignVCenter|Qt::AlignHCenter);
}

PlayQueueModel::~PlayQueueModel()
{
}

QModelIndex PlayQueueModel::index(int row, int column, const QModelIndex &parent) const
{
    return hasIndex(row, column, parent) ? createIndex(row, column, (void *)&songs.at(row)) : QModelIndex();
}

QModelIndex PlayQueueModel::parent(const QModelIndex &idx) const
{
    Q_UNUSED(idx)
    return QModelIndex();
}

QVariant PlayQueueModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (Qt::Horizontal==orientation) {
        switch (role) {
        case Qt::DisplayRole:
            return headerText(section);
        case Qt::TextAlignmentRole:
            return alignments[section];
        case Cantata::Role_InitiallyHidden:
            return COL_YEAR==section || COL_ORIGYEAR==section || COL_DISC==section || COL_GENRE==section || COL_PRIO==section ||
                   COL_COMPOSER==section || COL_PERFORMER==section || COL_RATING==section || COL_FILENAME==section || COL_PATH==section;
        case Cantata::Role_Hideable:
            return COL_LENGTH!=section;
        case Cantata::Role_Width:
            switch (section) {
            case COL_TRACK:     return 0.075;
            case COL_DISC:      return 0.03;
            case COL_TITLE:     return 0.3;
            case COL_ARTIST:    return 0.27;
            case COL_ALBUM:     return 0.27;
            case COL_LENGTH:    return 0.05;
            case COL_YEAR:      return 0.05;
            case COL_ORIGYEAR:  return 0.05;
            case COL_GENRE:     return 0.1;
            case COL_PRIO:      return 0.015;
            case COL_COMPOSER:  return 0.2;
            case COL_PERFORMER: return 0.2;
            case COL_RATING:    return 0.08;
            case COL_FILENAME:  return 0.27;
            case COL_PATH:      return 0.27;
            }
        case Cantata::Role_ContextMenuText:
            return COL_TRACK==section ? tr("# (Track Number)") : headerText(section);
        default:
            break;
        }
    }

    return QVariant();
}

bool PlayQueueModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if (Qt::Horizontal==orientation && Qt::TextAlignmentRole==role && section>=0) {
        int al=value.toInt()|Qt::AlignVCenter;
        if (al!=alignments[section]) {
            alignments[section]=al;
            return true;
        }
    }
    return false;
}

int PlayQueueModel::rowCount(const QModelIndex &idx) const
{
    return idx.isValid() ? 0 : songs.size();
}

static QString basicPath(const Song &song)
{
    #ifdef ENABLE_HTTP_SERVER
    if (song.isCantataStream()) {
        Song mod=HttpServer::self()->decodeUrl(song.file);
        if (!mod.file.isEmpty()) {
            return mod.file;
        }
    }
    #endif
    QString path=song.filePath();
    int marker=path.indexOf(QLatin1Char('#'));
    return -1==marker ? path : path.left(marker);
}

QVariant PlayQueueModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() && Cantata::Role_RatingCol==role) {
        return COL_RATING;
    }

    if (!index.isValid() || index.row() >= songs.size()) {
        return QVariant();
    }

    switch (role) {
    case Cantata::Role_MainText: {
        const Song &s=songs.at(index.row());
        return s.title.isEmpty() ? s.file : s.trackAndTitleStr(false);
    }
    case Cantata::Role_SubText: {
        const Song &s=songs.at(index.row());
        return s.artist+Song::constSep+s.displayAlbum();
    }
    case Cantata::Role_Time: {
        const Song &s=songs.at(index.row());
        return s.time>0 ? Utils::formatTime(s.time) : QLatin1String("");
    }
    case Cantata::Role_IsCollection:
        return false;
    case Cantata::Role_CollectionId:
        return 0;
    case Cantata::Role_Key:
        return songs.at(index.row()).key;
    case Cantata::Role_Id:
        return songs.at(index.row()).id;
    case Cantata::Role_SongWithRating:
    case Cantata::Role_Song: {
        QVariant var;
        const Song &s=songs.at(index.row());
        if (Cantata::Role_SongWithRating==role && Song::Standard==s.type && Song::Rating_Null==s.rating) {
            emit getRating(s.file);
            s.rating=Song::Rating_Requested;
        }
        var.setValue<Song>(s);
        return var;
    }
    case Cantata::Role_AlbumDuration: {
        const Song &first = songs.at(index.row());
        quint32 d=first.time;
        for (int i=index.row()+1; i<songs.count(); ++i) {
            const Song &song = songs.at(i);
            if (song.key!=first.key) {
                break;
            }
            d+=song.time;
        }
        if (index.row()>1) {
            for (int i=index.row()-1; i<=0; ++i) {
                const Song &song = songs.at(i);
                if (song.key!=first.key) {
                    break;
                }
                d+=song.time;
            }
        }
        return d;
    }
    case Cantata::Role_SongCount: {
        const Song &first = songs.at(index.row());
        quint32 count=1;
        for (int i=index.row()+1; i<songs.count(); ++i) {
            const Song &song = songs.at(i);
            if (song.key!=first.key) {
                break;
            }
            count++;
        }
        if (index.row()>1) {
            for (int i=index.row()-1; i<=0; ++i) {
                const Song &song = songs.at(i);
                if (song.key!=first.key) {
                    break;
                }
                count++;
            }
        }
        return count;
    }
    case Cantata::Role_CurrentStatus: {
        quint16 key=songs.at(index.row()).key;
        for (int i=index.row()+1; i<songs.count(); ++i) {
            const Song &s=songs.at(i);
            if (s.key!=key) {
                return QVariant();
            }
            if (s.id==currentSongId) {
                switch (mpdState) {
                case MPDState_Inactive:
                case MPDState_Stopped: return (int)GroupedView::State_Stopped;
                case MPDState_Playing: return (int)(stopAfterCurrent ? GroupedView::State_StopAfter : GroupedView::State_Playing);
                case MPDState_Paused:  return (int)GroupedView::State_Paused;
                }
            }  else if (-1!=s.id && s.id==stopAfterTrackId) {
                return GroupedView::State_StopAfterTrack;
            }
        }
        return QVariant();
    }
    case Cantata::Role_Status: {
        qint32 id=songs.at(index.row()).id;
        if (id==currentSongId) {
            switch (mpdState) {
            case MPDState_Inactive:
            case MPDState_Stopped: return (int)GroupedView::State_Stopped;
            case MPDState_Playing: return (int)(stopAfterCurrent ? GroupedView::State_StopAfter : GroupedView::State_Playing);
            case MPDState_Paused:  return (int)GroupedView::State_Paused;
            }
        } else if (-1!=id && id==stopAfterTrackId) {
            return GroupedView::State_StopAfterTrack;
        }
        return (int)GroupedView::State_Default;
        break;
    }
    case Qt::FontRole: {
        Song s=songs.at(index.row());

        if (s.isStream()) {
            QFont font;
            if (songs.at(index.row()).id == currentSongId) {
                font.setBold(true);
            }
            font.setItalic(true);
            return font;
        }
        else if (songs.at(index.row()).id == currentSongId) {
            QFont font;
            font.setBold(true);
            return font;
        }
        break;
    }
//     case Qt::BackgroundRole:
//         if (songs.at(index.row()).id == currentSongId) {
//             QColor col(QPalette().color(QPalette::Highlight));
//             col.setAlphaF(0.2);
//             return QVariant(col);
//         }
//         break;
    case Qt::DisplayRole: {
        const Song &song = songs.at(index.row());
        switch (index.column()) {
        case COL_TITLE:
            return song.title.isEmpty() ? Utils::getFile(basicPath(song)) : song.title;
        case COL_ARTIST:
            return song.artist.isEmpty() ? Song::unknown() : song.artist;
        case COL_ALBUM:
            if (song.isStream() && song.album.isEmpty()) {
                QString n=song.name();
                if (!n.isEmpty()) {
                    return n;
                }
            }
            return song.album;
        case COL_TRACK:
            if (song.track <= 0) {
                return QVariant();
            }
            return song.track;
        case COL_LENGTH:
            return Utils::formatTime(song.time);
        case COL_DISC:
            if (song.disc <= 0) {
                return QVariant();
            }
            return song.disc;
        case COL_YEAR:
            if (song.year <= 0) {
                return QVariant();
            }
            return song.year;
        case COL_ORIGYEAR:
            if (song.origYear <= 0) {
                return QVariant();
            }
            return song.origYear;
        case COL_GENRE:
            return song.displayGenre();
        case COL_PRIO:
            return song.priority;
        case COL_COMPOSER:
            return song.composer();
        case COL_PERFORMER:
            return song.performer();
//        case COL_RATING:{
//            QVariant var;
//            const Song &s=songs.at(index.row());
//            if (Song::Standard==s.type && Song::constNullRating==s.rating) {
//                emit getRating(s.file);
//                s.rating=Song::constRatingRequested;
//            }
//            var.setValue(s.rating);
//            return var;
//        }
        case COL_FILENAME:
            return Utils::getFile(QUrl(song.file).path());
        case COL_PATH: {
            QString dir=Utils::getDir(QUrl(song.file).path());
            return dir.endsWith("/") ? dir.left(dir.length()-1) : dir;
        }
        default:
            break;
        }
        break;
    }
    case Qt::ToolTipRole: {
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        Song s=songs.at(index.row());
        if (s.album.isEmpty() && s.isStream()) {
            return basicPath(s);
        } else {
            return s.toolTip();
        }
    }
    case Qt::TextAlignmentRole:
        return alignments[index.column()];
    case Cantata::Role_Decoration: {
        qint32 id=songs.at(index.row()).id;
        if (id==currentSongId) {
            switch (mpdState) {
            case MPDState_Inactive:
            case MPDState_Stopped: return MonoIcon::icon(FontAwesome::stop, Utils::monoIconColor());
            case MPDState_Playing: return MonoIcon::icon(stopAfterCurrent ? FontAwesome::playcircle : FontAwesome::play, Utils::monoIconColor());
            case MPDState_Paused:  return MonoIcon::icon(FontAwesome::pause, Utils::monoIconColor());
            }
        } else if (-1!=id && id==stopAfterTrackId) {
            return MonoIcon::icon(FontAwesome::stop, Utils::monoIconColor());
        }
        break;
    }
    default:
        break;
    }

    return QVariant();
}

bool PlayQueueModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Cantata::Role_DropAdjust==role) {
        dropAdjust=value.toUInt();
        return true;
    } else {
        return QAbstractItemModel::setData(index, value, role);
    }
}

Qt::DropActions PlayQueueModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags PlayQueueModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    }
    return Qt::ItemIsDropEnabled;
}

/**
 * @return A QStringList with the mimetypes we support
 */
QStringList PlayQueueModel::mimeTypes() const
{
    return QStringList() << constMoveMimeType << constFileNameMimeType << constUriMimeType;
}

/**
 * Convert the data at indexes into mimedata ready for transport
 *
 * @param indexes The indexes to pack into mimedata
 * @return The mimedata
 */
QMimeData *PlayQueueModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QList<quint32> positions;
    QStringList filenames;

    for (const QModelIndex &index: indexes) {
        if (index.isValid() && 0==index.column()) {
            positions.append(index.row());
            filenames.append(static_cast<Song *>(index.internalPointer())->file);
        }
    }

    encode(*mimeData, constMoveMimeType, positions);
    encode(*mimeData, constFileNameMimeType, filenames);
    return mimeData;
}

/**
 * Act on mime data that is dropped in our model
 *
 * @param data The actual data that is dropped
 * @param action The action. This could mean drop/copy etc
 * @param row The row where it is dropper
 * @param column The column where it is dropper
 * @param parent The parent of where we have dropped it
 *
 * @return bool if we accest the drop
 */
bool PlayQueueModel::dropMimeData(const QMimeData *data,
                                  Qt::DropAction action, int row, int /*column*/, const QModelIndex & /*parent*/)
{
    if (Qt::IgnoreAction==action) {
        return true;
    }

    row+=dropAdjust;
    if (data->hasFormat(constMoveMimeType)) { //Act on internal moves
        if (row < 0) {
            emit move(decodeInts(*data, constMoveMimeType), songs.size(), songs.size());
        } else {
            emit move(decodeInts(*data, constMoveMimeType), row, songs.size());
        }
        return true;
    } else if (data->hasFormat(constFileNameMimeType)) {
        //Act on moves from the music library and dir view
        addItems(decode(*data, constFileNameMimeType), row, false, 0, false);
        return true;
    } else if(data->hasFormat(constUriMimeType)/* && MPDConnection::self()->getDetails().isLocal()*/) {
        QStringList useable=parseUrls(decode(*data, constUriMimeType));
        if (useable.count()) {
            addItems(useable, row, false, 0, false);
            return true;
        }
    }
    return false;
}

void PlayQueueModel::load(const QStringList &urls, int action, quint8 priority, bool decreasePriority)
{
    if (-1==action) {
        action = songs.isEmpty() ? MPDConnection::AppendAndPlay : MPDConnection::Append;
    }
    QStringList useable=parseUrls(urls);
    if (useable.count()) {
        addItems(useable, songs.count(), action, priority, decreasePriority);
    }
}

void PlayQueueModel::addItems(const QStringList &items, int row, int action, quint8 priority, bool decreasePriority)
{
    bool haveRadioStream=false;

    for (const QString &f: items) {
        QUrl u(f);
        if (u.scheme().startsWith(StreamsModel::constPrefix)) {
            haveRadioStream=true;
            break;
        }
    }

    if (haveRadioStream) {
        emit fetchingStreams();
        fetcher->get(items, row, action, priority, decreasePriority);
    } else {
        addFiles(items, row, action, priority, decreasePriority);
    }
}

void PlayQueueModel::addFiles(const QStringList &filenames, int row, int action, quint8 priority, bool decreasePriority)
{
    if (MPDConnection::ReplaceAndplay==action) {
        emit filesAdded(filenames, 0, 0, MPDConnection::ReplaceAndplay, priority, decreasePriority);
    } else if (songs.isEmpty()) {
         emit filesAdded(filenames, 0, 0, action, priority, decreasePriority);
    } else if (row < 0) {
        emit filesAdded(filenames, songs.size(), songs.size(), action, priority, decreasePriority);
    } else {
        emit filesAdded(filenames, row, songs.size(), action, priority, decreasePriority);
    }
}

void PlayQueueModel::prioritySet(const QMap<qint32, quint8> &prio)
{
    QList<Song> prev;
    if (undoEnabled) {
        for (const Song &s: songs) {
            prev.append(s);
        }
    }
    QMap<qint32, quint8> copy = prio;
    int row=0;

    for (const Song &s: songs) {
        QMap<qint32, quint8>::ConstIterator it = copy.find(s.id);
        if (copy.end()!=it) {
            s.priority=it.value();
            copy.remove(s.id);
            QModelIndex idx(index(row, 0));
            emit dataChanged(idx, idx);
            if (copy.isEmpty()) {
                break;
            }
        }
        ++row;
    }

    saveHistory(prev);
}

qint32 PlayQueueModel::getIdByRow(qint32 row) const
{
    return row>=songs.size() ? -1 : songs.at(row).id;
}

qint32 PlayQueueModel::getSongId(const QString &file) const
{
    if (CueFile::isCue(file)) {
        QUrl u(file);
        QUrlQuery q(u);
        Song check;
        check.album=q.queryItemValue("album");
        check.artist=q.queryItemValue("artist");
        check.albumartist=q.queryItemValue("albumartist");
        check.album=q.queryItemValue("album");
        check.title=q.queryItemValue("title");
        check.disc=q.queryItemValue("disc").toUInt();
        check.track=q.queryItemValue("track").toUInt();
        check.time=q.queryItemValue("time").toUInt();
        check.year=q.queryItemValue("year").toUInt();
        check.origYear=q.queryItemValue("origYear").toUInt();

        for (const Song &s: songs) {
            if (s.sameMetadata(check)) {
                return s.id;
            }
        }
    } else {
        for (const Song &s: songs) {
            if (s.file==file) {
                return s.id;
            }
        }
    }

    return -1;
}

// qint32 PlayQueueModel::getPosByRow(qint32 row) const
// {
//     return row>=songs.size() ? -1 : songs.at(row).pos;
// }

qint32 PlayQueueModel::getRowById(qint32 id) const
{
    for (int i = 0; i < songs.size(); i++) {
        if (songs.at(i).id == id) {
            return i;
        }
    }

    return -1;
}

Song PlayQueueModel::getSongByRow(const qint32 row) const
{
    return row<0 || row>=songs.size() ? Song() : songs.at(row);
}

Song PlayQueueModel::getSongById(qint32 id) const
{
    for (const Song &s: songs) {
        if (s.id==id) {
            return s;
        }
    }
    return Song();
}

void PlayQueueModel::updateCurrentSong(quint32 id)
{
    qint32 oldIndex = currentSongId;
    currentSongId = id;

    if (-1!=oldIndex) {
        int row=-1==currentSongRowNum ? getRowById(oldIndex) : currentSongRowNum;
        emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex())-1));
    }

    currentSongRowNum=getRowById(currentSongId);
    if (currentSongRowNum>=0 && currentSongRowNum<=songs.count()) {
        const Song &song=songs.at(currentSongRowNum);
        if (Song::Rating_Null==song.rating) {
            song.rating=Song::Rating_Requested;
            emit getRating(song.file);
        } else if (Song::Rating_Requested!=song.rating) {
            emit currentSongRating(song.file, song.rating);
        }
    }
    emit dataChanged(index(currentSongRowNum, 0), index(currentSongRowNum, columnCount(QModelIndex())-1));

    if (-1!=currentSongId && stopAfterTrackId==currentSongId) {
        stopAfterTrackId=-1;
        emit stop(true);
    }
}

void PlayQueueModel::clear()
{
    beginResetModel();
    songs.clear();
    ids.clear();
    currentSongId=-1;
    currentSongRowNum=0;
    stopAfterTrackId=-1;
    time=0;
    endResetModel();
}

qint32 PlayQueueModel::currentSongRow() const
{
    if (-1==currentSongRowNum) {
        currentSongRowNum=getRowById(currentSongId);
    }
    return currentSongRowNum;
}

void PlayQueueModel::setState(MPDState st)
{
    if (st!=mpdState) {
        mpdState=st;
        if (-1!=currentSongId) {
            if (-1==currentSongRowNum) {
                currentSongRowNum=getRowById(currentSongId);
            }
            emit dataChanged(index(currentSongRowNum, 0), index(currentSongRowNum, 2));
        }
    }
}

// Update playqueue with contents returned from MPD.
void PlayQueueModel::update(const QList<Song> &songList, bool isComplete)
{
    currentSongRowNum=-1;
    if (songList.isEmpty()) {
        Song::clearKeyStore(MPDParseUtils::Loc_PlayQueue);
    }

    removeDuplicatesAction->setEnabled(songList.count()>1);
    QList<Song> prev;
    if (undoEnabled) {
        prev=songs;
    }

    QSet<qint32> newIds;
    for (const Song &s: songList) {
        newIds.insert(s.id);
    }

    // If we have too many changes UI can hang, so it is sometimes better just to do a complete reset!
    if (isComplete && (songs.count()>MPDConnection::constMaxPqChanges
                       || !MPDConnection::self()->isPlayQueueIdValid())) {
        songs.clear();
    }

    if (songs.isEmpty() || songList.isEmpty()) {
        beginResetModel();
        songs=songList;
        ids=newIds;
        endResetModel();
        if (songList.isEmpty()) {
            stopAfterTrackId=-1;
        }
    } else {
        time = 0;

        QSet<qint32> removed=ids-newIds;
        for (qint32 id: removed) {
            qint32 row=getRowById(id);
            if (row!=-1) {
                beginRemoveRows(QModelIndex(), row, row);
                songs.removeAt(row);
                endRemoveRows();
            }
        }
        for (qint32 i=0; i<songList.count(); ++i) {
            Song s=songList.at(i);
            bool newSong=i>=songs.count();
            Song currentSongAtPos=newSong ? Song() : songs.at(i);
            bool isEmpty=s.isEmpty();

            if (newSong || s.id!=currentSongAtPos.id) {
                qint32 existingPos=newSong ? -1 : getRowById(s.id);
                if (-1==existingPos) {
                    beginInsertRows(QModelIndex(), i, i);
                    songs.insert(i, s);
                    endInsertRows();
                } else {
                    beginMoveRows(QModelIndex(), existingPos, existingPos, QModelIndex(), i>existingPos ? i+1 : i);
                    Song old=songs.takeAt(existingPos);
//                     old.pos=s.pos;
                    s.rating=old.rating;
                    songs.insert(i, isEmpty ? old : s);
                    endMoveRows();
                }
            } else if (isEmpty) {
                s=currentSongAtPos;
            } else {
                s.key=currentSongAtPos.key;
                s.rating=currentSongAtPos.rating;
                songs.replace(i, s);
                if (s.title!=currentSongAtPos.title || s.artist!=currentSongAtPos.artist || s.name()!=currentSongAtPos.name()) {
                    emit dataChanged(index(i, 0), index(i, columnCount(QModelIndex())-1));
                }
            }

            if (s.id==currentSongId) {
                currentSongRowNum=i;
            }
            time += s.time;
        }

        if (songs.count()>songList.count()) {
            int toBeRemoved=songs.count()-songList.count();
            beginRemoveRows(QModelIndex(), songList.count(), songs.count()-1);
            for (int i=0; i<toBeRemoved; ++i) {
                songs.takeLast();
            }
            endRemoveRows();
        }

        ids=newIds;
        if (-1!=stopAfterTrackId && !ids.contains(stopAfterTrackId)) {
            stopAfterTrackId=-1;
        }
        emit statsUpdated(songs.size(), time);
    }

    saveHistory(prev);
    shuffleAction->setEnabled(songs.count()>1);
    sortAction->setEnabled(songs.count()>1);
}

void PlayQueueModel::setStopAfterTrack(qint32 track)
{
    bool clear=track==stopAfterTrackId || (track==currentSongId && stopAfterCurrent);

    stopAfterTrackId=clear ? -1 : track;
    qint32 row=getRowById(track);
    if (-1!=row) {
        emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex())-1));
    }
    if (clear) {
        emit clearStopAfter();
    } else if (stopAfterTrackId==currentSongId) {
        emit stop(true);
    }
}

bool PlayQueueModel::removeCantataStreams(bool cdOnly)
{
    QList<qint32> ids;
    for (const Song &s: songs) {
        if (s.isCdda() || (!cdOnly && s.isCantataStream())) {
            ids.append(s.id);
        }
    }

    if (!ids.isEmpty()) {
        emit removeSongs(ids);
        return true;
    }
    return false;
}

void PlayQueueModel::removeAll()
{
    emit clearEntries();
}

void PlayQueueModel::remove(const QList<int> &rowsToRemove)
{
    QList<qint32> removeIds;
    for (const int &r: rowsToRemove) {
        if (r>-1 && r<songs.count()) {
            removeIds.append(songs.at(r).id);
        }
    }

    if (!removeIds.isEmpty()) {
        emit removeSongs(removeIds);
    }
}

void PlayQueueModel::crop(const QList<int> &rowsToKeep)
{
    QSet<qint32> allIds;
    for (const Song &song: songs) {
        allIds.insert(song.id);
    }

    QSet<qint32> keepIds;
    for (const int &r: rowsToKeep) {
        if (r>-1 && r<songs.count()) {
            keepIds.insert(songs.at(r).id);
        }
    }

    QSet<qint32> removeIds=allIds-keepIds;
    if (!removeIds.isEmpty()) {
        emit removeSongs(removeIds.toList());
    }
}

void PlayQueueModel::setRating(const QList<int> &rows, quint8 rating) const
{
    QSet<QString> files;
    for (const int &r: rows) {
        if (r>-1 && r<songs.count()) {
            const Song &s=songs.at(r);
            if (Song::Standard==s.type && !files.contains(s.file)) {
                files.insert(s.file);
            }
        }
    }
    emit setRating(files.toList(), rating);
}

void PlayQueueModel::enableUndo(bool e)
{
    if (e==undoEnabled) {
        return;
    }
    undoEnabled=e && undoLimit>0;
    if (!e) {
        undoStack.clear();
        redoStack.clear();
    }
    controlActions();
}

static PlayQueueModel::UndoItem getState(const QList<Song> &songs)
{
    PlayQueueModel::UndoItem item;
    for (const Song &s: songs) {
        item.files.append(s.file);
        item.priority.append(s.priority);
    }
    return item;
}

static bool equalSongList(const QList<Song> &a, const QList<Song> &b)
{
    if (a.count()!=b.count()) {
        return false;
    }

    for (int i=0; i<a.count(); ++i) {
        const Song &sa=a.at(i);
        const Song &sb=b.at(i);
        if (sa.priority!=sb.priority || sa.file!=sb.file) {
            return false;
        }
    }
    return true;
}

void PlayQueueModel::saveHistory(const QList<Song> &prevList)
{
    if (!undoEnabled) {
        return;
    }

    if (equalSongList(prevList, songs)) {
        lastCommand=Cmd_Other;
        return;
    }

    switch (lastCommand) {
    case Cmd_Redo: {
        if (redoStack.isEmpty()) {
            lastCommand=Cmd_Other;
        } else {
            UndoItem actioned=redoStack.pop();
            if (actioned!=getState(songs)) {
                lastCommand=Cmd_Other;
            } else {
                undoStack.push(getState(prevList));
            }
        }
        break;
    }
    case Cmd_Undo: {
        if (undoStack.isEmpty()) {
            lastCommand=Cmd_Other;
        } else {
            UndoItem actioned=undoStack.pop();
            if (actioned!=getState(songs)) {
                lastCommand=Cmd_Other;
            } else {
                redoStack.push(getState(prevList));
            }
        }
        break;
    }
    case Cmd_Other:
        break;
    }

    if (Cmd_Other==lastCommand) {
        redoStack.clear();
        undoStack.push(getState(prevList));
        if (undoStack.size()>undoLimit) {
            undoStack.pop_back();
        }
    }

    controlActions();
    lastCommand=Cmd_Other;
}

void PlayQueueModel::controlActions()
{
    undoAction->setEnabled(!undoStack.isEmpty());
    undoAction->setVisible(undoLimit>0);
    redoAction->setEnabled(!redoStack.isEmpty());
    redoAction->setVisible(undoLimit>0);
}

void PlayQueueModel::addSortAction(const QString &name, const QString &key)
{
    Action *action=new Action(name, sortAction);
    action->setProperty(constSortByKey, key);
    sortAction->menu()->addAction(action);
    connect(action, SIGNAL(triggered()), SLOT(sortBy()));
}

static bool composerSort(const Song *s1, const Song *s2)
{
    const QString v1=s1->hasComposer() ? s1->composer() : QString();
    const QString v2=s2->hasComposer() ? s2->composer() : QString();
    int c=v1.localeAwareCompare(v2);
    return c<0 || (c==0 && (*s1)<(*s2));
}

static bool performerSort(const Song *s1, const Song *s2)
{
    const QString v1=s1->hasPerformer() ? s1->performer() : QString();
    const QString v2=s2->hasPerformer() ? s2->performer() : QString();
    int c=v1.localeAwareCompare(v2);
    return c<0 || (c==0 && (*s1)<(*s2));
}

static bool artistSort(const Song *s1, const Song *s2)
{
    const QString v1=s1->hasArtistSort() ? s1->artistSort() : s1->artist;
    const QString v2=s2->hasArtistSort() ? s2->artistSort() : s2->artist;
    int c=v1.localeAwareCompare(v2);
    return c<0 || (c==0 && (*s1)<(*s2));
}

static bool albumArtistSort(const Song *s1, const Song *s2)
{
    const QString v1=s1->hasAlbumArtistSort() ? s1->albumArtistSort() : s1->albumArtistOrComposer();
    const QString v2=s2->hasAlbumArtistSort() ? s2->albumArtistSort() : s2->albumArtistOrComposer();
    int c=v1.localeAwareCompare(v2);
    return c<0 || (c==0 && (*s1)<(*s2));
}

static bool albumSort(const Song *s1, const Song *s2)
{
    const QString v1=s1->hasAlbumSort() ? s1->albumSort() : s1->album;
    const QString v2=s2->hasAlbumSort() ? s2->albumSort() : s2->album;
    int c=v1.localeAwareCompare(v2);
    return c<0 || (c==0 && (*s1)<(*s2));
}

static bool genreSort(const Song *s1, const Song *s2)
{
    int c=s1->compareGenres(*s2);
    return c<0 || (c==0 && (*s1)<(*s2));
}

static bool yearSort(const Song *s1, const Song *s2)
{
    return s1->year<s2->year || (s1->year==s2->year && (*s1)<(*s2));
}

static bool titleSort(const Song *s1, const Song *s2)
{
    int c=s1->title.localeAwareCompare(s2->title);
    return c<0 || (c==0 && (*s1)<(*s2));
}

static bool trackSort(const Song *s1, const Song *s2)
{
    return s1->track<s2->track || (s1->track==s2->track && (*s1)<(*s2));
}

void PlayQueueModel::sortBy()
{
    Action *act=qobject_cast<Action *>(sender());
    if (act) {
        QString key=act->property(constSortByKey).toString();
        QList<const Song *> copy;
        for (const Song &s: songs) {
            ((Song &)s).populateSorts();
            copy.append(&s);
        }

        if (constSortByArtistKey==key) {
            std::sort(copy.begin(), copy.end(), artistSort);
        } else if (constSortByAlbumArtistKey==key) {
            std::sort(copy.begin(), copy.end(), albumArtistSort);
        } else if (constSortByAlbumKey==key) {
            std::sort(copy.begin(), copy.end(), albumSort);
        } else if (constSortByGenreKey==key) {
            std::sort(copy.begin(), copy.end(), genreSort);
        } else if (constSortByYearKey==key) {
            std::sort(copy.begin(), copy.end(), yearSort);
        } else if (constSortByComposerKey==key) {
            std::sort(copy.begin(), copy.end(), composerSort);
        } else if (constSortByPerformerKey==key) {
            std::sort(copy.begin(), copy.end(), performerSort);
        } else if (constSortByTitleKey==key) {
            std::sort(copy.begin(), copy.end(), titleSort);
        } else if (constSortByNumberKey==key) {
            std::sort(copy.begin(), copy.end(), trackSort);
        }
        QList<quint32> positions;
        for (const Song *s: copy) {
            positions.append(getRowById(s->id));
        }
        emit setOrder(positions);
    }
}

void PlayQueueModel::removeDuplicates()
{
    QMap<QString, QList<Song> > map;
    for (const Song &song: songs) {
        map[song.albumKey()+"-"+song.artistSong()+"-"+song.track].append(song);
    }

    QList<qint32> toRemove;
    for (const QString &key: map.keys()) {
        QList<Song> values=map.value(key);
        if (values.size()>1) {
            Song::sortViaType(values);
            for (int i=1; i<values.count(); ++i) {
                toRemove.append(values.at(i).id);
            }
        }
    }
    if (!toRemove.isEmpty()) {
        emit removeSongs(toRemove);
    }
}

void PlayQueueModel::ratingResult(const QString &file, quint8 r)
{
    QList<Song>::iterator it=songs.begin();
    QList<Song>::iterator end=songs.end();
    int numCols=columnCount(QModelIndex())-1;

    for (int row=0; it!=end; ++it, ++row) {
        if (Song::Standard==(*it).type && r!=(*it).rating && (*it).file==file) {
            (*it).rating=r;
            emit dataChanged(index(row, 0), index(row, numCols));
            if ((*it).id==currentSongId) {
                emit currentSongRating(file, r);
            }
        }
    }
}

void PlayQueueModel::stickerDbChanged()
{
    // Sticker DB changed, need to re-request ratings...
    QSet<QString> requests;
    for (const Song &song: songs) {
        if (Song::Standard==song.type && song.rating<=Song::Rating_Max && !requests.contains(song.file)) {
            emit getRating(song.file);
            requests.insert(song.file);
        }
    }
}

void PlayQueueModel::undo()
{
    if (!undoEnabled || undoStack.isEmpty()) {
        return;
    }
    UndoItem item=undoStack.top();
    emit populate(item.files, item.priority);
    lastCommand=Cmd_Undo;
}

void PlayQueueModel::redo()
{
    if (!undoEnabled || redoStack.isEmpty()) {
        return;
    }
    UndoItem item=redoStack.top();
    emit populate(item.files, item.priority);
    lastCommand=Cmd_Redo;
}

void PlayQueueModel::playSong(const QString &file)
{
    qint32 id=getSongId(file);
    if (-1==id) {
        emit addAndPlay(file);
    } else {
        emit startPlayingSongId(id);
    }
}

void PlayQueueModel::stats()
{
    time = 0;
    //Loop over all songs
    for (const Song &song: songs) {
        time += song.time;
    }

    emit statsUpdated(songs.size(), time);
}

void PlayQueueModel::cancelStreamFetch()
{
    fetcher->cancel();
}

static bool songSort(const Song *s1, const Song *s2)
{
    return s1->disc<s2->disc || (s1->disc==s2->disc && (s1->track<s2->track || (s1->track==s2->track && (*s1)<(*s2))));
}

void PlayQueueModel::shuffleAlbums()
{
    QMap<quint32, QList<const Song *> > albums;
    for (const Song &s: songs) {
        albums[s.key].append(&s);
    }

    QList<quint32> keys=albums.keys();
    if (keys.count()<2) {
        return;
    }

    QList<quint32> positions;
    while (!keys.isEmpty()) {
        quint32 key=keys.takeAt(Utils::random(keys.count()));
        QList<const Song *> albumSongs=albums[key];
        std::sort(albumSongs.begin(), albumSongs.end(), songSort);
        for (const Song *song: albumSongs) {
            positions.append(getRowById(song->id));
        }
    }
    emit setOrder(positions);
}

void PlayQueueModel::stopAfterCurrentChanged(bool afterCurrent)
{
    if (afterCurrent!=stopAfterCurrent) {
        stopAfterCurrent=afterCurrent;
        emit dataChanged(index(currentSongRowNum, 0), index(currentSongRowNum, columnCount(QModelIndex())-1));
    }
}

void PlayQueueModel::remove(const QList<Song> &rem)
{
    QSet<QString> s;
    QList<qint32> ids;

    for (const Song &song: rem) {
        s.insert(song.file);
    }

    for (const Song &song: songs) {
        if (s.contains(song.file)) {
            ids.append(song.id);
            s.remove(song.file);
            if (s.isEmpty()) {
                break;
            }
        }
    }

    if (!ids.isEmpty()) {
        emit removeSongs(ids);
    }
}

void PlayQueueModel::updateDetails(const QList<Song> &updated)
{
    QMap<QString, Song> songMap;
    QList<int> updatedRows;
    bool currentUpdated=false;
    Song currentSong;

    for (const Song &song: updated) {
        songMap[song.file]=song;
    }

    for (int i=0; i<songs.count(); ++i) {
        Song current=songs.at(i);
        if (songMap.contains(current.file)) {
            Song updatedSong=songMap[current.file];
            updatedSong.id=current.id;
            updatedSong.setKey(MPDParseUtils::Loc_PlayQueue);

            if (updatedSong.title!=current.title || updatedSong.artist!=current.artist || updatedSong.name()!=current.name()) {
                songs.replace(i, updatedSong);
                updatedRows.append(i);
                if (currentSongId==current.id) {
                    currentUpdated=true;
                    currentSong=updatedSong;
                }
            }

            songMap.remove(current.file);
            if (songMap.isEmpty()) {
                break;
            }
        }
    }

    if (!updatedRows.isEmpty()) {
        if (updatedRows.count()==updated.count()) {
            beginResetModel();
            endResetModel();
        } else {
            for (int row: updatedRows) {
                emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex())-1));
            }
        }
    }

    if (currentUpdated) {
        emit updateCurrent(currentSong);
    }
}

QStringList PlayQueueModel::filenames()
{
    QStringList names;
    for (const Song &song: songs) {
        names << song.file;
    }
    return names;
}

#include "moc_playqueuemodel.cpp"
