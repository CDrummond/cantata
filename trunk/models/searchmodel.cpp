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

#include "searchmodel.h"
#include "icons.h"
#include "itemview.h"
#include "tableview.h"
#include "mpdconnection.h"
#include "playqueuemodel.h"
#include <QString>
#include <QVariant>
#include <QMimeData>
#include <QLocale>
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

QString SearchModel::headerText(int col)
{
    switch (col) {
    case COL_DISC:   return PlayQueueModel::headerText(PlayQueueModel::COL_DISC);
    case COL_TITLE:  return PlayQueueModel::headerText(PlayQueueModel::COL_TITLE);
    case COL_ARTIST: return PlayQueueModel::headerText(PlayQueueModel::COL_ARTIST);
    case COL_ALBUM:  return PlayQueueModel::headerText(PlayQueueModel::COL_ALBUM);
    case COL_LENGTH: return PlayQueueModel::headerText(PlayQueueModel::COL_LENGTH);
    case COL_YEAR:   return PlayQueueModel::headerText(PlayQueueModel::COL_YEAR);
    case COL_GENRE:  return PlayQueueModel::headerText(PlayQueueModel::COL_GENRE);
    default:         return QString();
    }
}

SearchModel::SearchModel(QObject *parent)
    : ActionModel(parent)
    , multiCol(false)
    , currentId(0)
{
    connect(this, SIGNAL(search(QString,QString,int)), MPDConnection::self(), SLOT(search(QString,QString,int)));
    connect(MPDConnection::self(), SIGNAL(searchResponse(int,QList<Song>)), this, SLOT(searchFinished(int,QList<Song>)));
}

SearchModel::~SearchModel()
{
    clear();
}

QModelIndex SearchModel::index(int row, int col, const QModelIndex &parent) const
{
    if (!hasIndex(row, col, parent)) {
        return QModelIndex();
    }

    return row<songList.count() ? createIndex(row, col, (void *)(&songList.at(row))) : QModelIndex();
}

QModelIndex SearchModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QModelIndex();
}

QVariant SearchModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (Qt::Horizontal==orientation) {
        switch (role) {
        case Qt::DisplayRole:
            return headerText(section);
        case Qt::TextAlignmentRole:
            switch (section) {
            case COL_TITLE:
            case COL_ARTIST:
            case COL_ALBUM:
            case COL_GENRE:
            default:
                return int(Qt::AlignVCenter|Qt::AlignLeft);
            case COL_LENGTH:
            case COL_DISC:
            case COL_YEAR:
                return int(Qt::AlignVCenter|Qt::AlignRight);
            }
        case TableView::Role_Hideable:
            return COL_YEAR==section || COL_DISC==section || COL_GENRE==section ? true : false;
        case TableView::Role_Width:
            switch (section) {
            case COL_DISC:   return 0.03;
            case COL_TITLE:  return 0.375;
            case COL_ARTIST: return 0.27;
            case COL_ALBUM:  return 0.27;
            case COL_LENGTH: return 0.05;
            case COL_YEAR:   return 0.05;
            case COL_GENRE:  return 0.115;
            }
        default:
            break;
        }
    }
    return QVariant();
}


int SearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : songList.count();
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    const Song *song = toSong(index);

    if (!song) {
        return QVariant();
    }
    
    switch (role) {
    case Qt::DecorationRole:
        if (multiCol) {
            return QVariant();
        }
        return Song::Playlist==song->type
                ? Icons::self()->playlistIcon
                : song->isStream()
                  ? Icons::self()->streamIcon
                  : Icons::self()->audioFileIcon;
    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case COL_TITLE:
        case COL_ARTIST:
        case COL_ALBUM:
        case COL_GENRE:
        default:
            return int(Qt::AlignVCenter|Qt::AlignLeft);
        case COL_LENGTH:
        case COL_DISC:
        case COL_YEAR:
            return int(Qt::AlignVCenter|Qt::AlignRight);
        }
    case Qt::DisplayRole:
        if (multiCol) {
            switch (index.column()) {
            case COL_TITLE:
                return song->title.isEmpty() ? Utils::getFile(song->file) : song->trackAndTitleStr(Song::isVariousArtists(song->albumArtist()));;
            case COL_ARTIST:
                return song->artist.isEmpty() ? Song::unknown() : song->artist;
            case COL_ALBUM:
                return song->album.isEmpty() && !song->name.isEmpty() && song->isStream() ? song->name : song->album;
            case COL_LENGTH:
                return Utils::formatTime(song->time);
            case COL_DISC:
                if (song->disc <= 0) {
                    return QVariant();
                }
                return song->disc;
            case COL_YEAR:
                if (song->year <= 0) {
                    return QVariant();
                }
                return song->year;
            case COL_GENRE:
                return song->genre;
            default:
                break;
            }
            break;
        }
    case Qt::ToolTipRole: {
        QString text=song->entryName();

        if (Qt::ToolTipRole==role) {
            text=text.replace("\n", "<br/>");
            if (!song->title.isEmpty()) {
                text+=QLatin1String("<br/>")+Utils::formatTime(song->time);
                text+=QLatin1String("<br/><small><i>")+song->file+QLatin1String("</i></small>");
            }
        }
        return text;
    }
    case ItemView::Role_MainText:
        return song->title.isEmpty() ? song->file : song->trackAndTitleStr();
    case ItemView::Role_SubText:
        return song->artist+QLatin1String(" - ")+song->album;
    default:
        return ActionModel::data(index, role);
    }
    return QVariant();
}

Qt::ItemFlags SearchModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    } else {
        return Qt::NoItemFlags;
    }
}

QStringList SearchModel::filenames(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<Song> list=songs(indexes, allowPlaylists);
    QStringList fnames;
    foreach (const Song &s, list) {
        fnames.append(s.file);
    }
    return fnames;
}

QList<Song> SearchModel::songs(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<Song> list;
    QSet<QString> files;
    foreach(QModelIndex index, indexes) {
        Song *song=static_cast<Song *>(index.internalPointer());
        if ((allowPlaylists || Song::Playlist!=song->type) && !files.contains(song->file)) {
            list << *song;
            files << song->file;
        }
    }
    return list;
}

QMimeData * SearchModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames(indexes, true));
    return mimeData;
}

QStringList SearchModel::mimeTypes() const
{
    QStringList types;
    types << PlayQueueModel::constFileNameMimeType;
    return types;
}

void SearchModel::refresh()
{
    QString k=currentKey;
    QString v=currentValue;
    clear();
    search(k, v);
}

void SearchModel::clear()
{
    if (!songList.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, songList.count()-1);
        songList.clear();
        endRemoveRows();
    }
    currentKey=currentValue=QString();
    currentId++;
    emit statsUpdated(0, 0);
}

void SearchModel::search(const QString &key, const QString &value)
{
    if (key==currentKey && value==currentValue) {
        return;
    }
    emit searching();
    clear();
    currentKey=key;
    currentValue=value;
    currentId++;
    emit search(key, value, currentId);
}

void SearchModel::searchFinished(int id, const QList<Song> &result)
{
    if (id!=currentId) {
        return;
    }

    beginResetModel();
    songList.clear();
    songList=result;
    endResetModel();
    quint32 time=0;
    foreach (const Song &s, songList) {
        time+=s.time;
    }

    emit statsUpdated(songList.size(), time);
    emit searched();
}
