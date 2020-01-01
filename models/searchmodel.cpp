/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "widgets/icons.h"
#include "roles.h"
#include "playqueuemodel.h"
#include "gui/settings.h"
#include <QString>
#include <QVariant>
#include <QMimeData>
#include <QLocale>
#include <QUrl>

QString SearchModel::headerText(int col)
{
    switch (col) {
    case COL_TRACK:     return PlayQueueModel::headerText(PlayQueueModel::COL_TRACK);
    case COL_DISC:      return PlayQueueModel::headerText(PlayQueueModel::COL_DISC);
    case COL_TITLE:     return PlayQueueModel::headerText(PlayQueueModel::COL_TITLE);
    case COL_ARTIST:    return PlayQueueModel::headerText(PlayQueueModel::COL_ARTIST);
    case COL_ALBUM:     return PlayQueueModel::headerText(PlayQueueModel::COL_ALBUM);
    case COL_LENGTH:    return PlayQueueModel::headerText(PlayQueueModel::COL_LENGTH);
    case COL_YEAR:      return PlayQueueModel::headerText(PlayQueueModel::COL_YEAR);
    case COL_ORIGYEAR:  return PlayQueueModel::headerText(PlayQueueModel::COL_ORIGYEAR);
    case COL_GENRE:     return PlayQueueModel::headerText(PlayQueueModel::COL_GENRE);
    case COL_COMPOSER:  return PlayQueueModel::headerText(PlayQueueModel::COL_COMPOSER);
    case COL_PERFORMER: return PlayQueueModel::headerText(PlayQueueModel::COL_PERFORMER);
    case COL_RATING:    return PlayQueueModel::headerText(PlayQueueModel::COL_RATING);
    default:            return QString();
    }
}

SearchModel::SearchModel(QObject *parent)
    : ActionModel(parent)
    , multiCol(false)
{
    alignments[COL_TITLE]=alignments[COL_ARTIST]=alignments[COL_ALBUM]=alignments[COL_GENRE]=alignments[COL_COMPOSER]=alignments[COL_PERFORMER]=int(Qt::AlignVCenter|Qt::AlignLeft);
    alignments[COL_TRACK]=alignments[COL_LENGTH]=alignments[COL_DISC]=alignments[COL_YEAR]=alignments[COL_ORIGYEAR]=int(Qt::AlignVCenter|Qt::AlignRight);
    alignments[COL_RATING]=int(Qt::AlignVCenter|Qt::AlignHCenter);
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
        case Cantata::Role_ContextMenuText:
            return COL_TRACK==section ? tr("# (Track Number)") : headerText(section);
        case Qt::TextAlignmentRole:
            return alignments[section];
        case Cantata::Role_InitiallyHidden:
            return COL_YEAR==section || COL_ORIGYEAR==section || COL_DISC==section || COL_GENRE==section ||
                   COL_COMPOSER==section || COL_PERFORMER==section || COL_RATING==section;
        case Cantata::Role_Hideable:
            return COL_TITLE!=section && COL_ARTIST!=section;
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
            case COL_COMPOSER:  return 0.2;
            case COL_PERFORMER: return 0.2;
            case COL_RATING:    return 0.08;
            }
        default:
            break;
        }
    }
    return QVariant();
}

bool SearchModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
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

int SearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : songList.count();
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() && Cantata::Role_RatingCol==role) {
        return COL_RATING;
    }

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
                ? Icons::self()->playlistListIcon
                : song->isStream()
                  ? Icons::self()->streamIcon
                  : Icons::self()->audioListIcon;
    case Qt::TextAlignmentRole:
        return alignments[index.column()];
    case Qt::DisplayRole:
        if (multiCol) {
            switch (index.column()) {
            case COL_TITLE:
                return song->title.isEmpty() ? Utils::getFile(song->file) : song->title;
            case COL_ARTIST:
                return song->artist.isEmpty() ? Song::unknown() : song->artist;
            case COL_ALBUM:
                if (song->isStream() && song->album.isEmpty()) {
                    QString n=song->name();
                    if (!n.isEmpty()) {
                        return n;
                    }
                }
                return song->album;
            case COL_TRACK:
                if (song->track <= 0) {
                    return QVariant();
                }
                return song->track;
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
            case COL_ORIGYEAR:
                if (song->origYear <= 0) {
                    return QVariant();
                }
                return song->origYear;
            case COL_GENRE:
                return song->displayGenre();
            case COL_COMPOSER:
                return song->composer();
            case COL_PERFORMER:
                return song->performer();
            default:
                break;
            }
            break;
        }
        return song->entryName();
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        return song->toolTip();
    case Cantata::Role_MainText:
        return song->title.isEmpty() ? song->file : song->trackAndTitleStr();
    case Cantata::Role_SubText: {
        QString resp=song->artist;
        QString al=song->displayAlbum();
        if (!al.isEmpty()) {
            if (!resp.isEmpty()) {
                resp+=Song::constSep;
            }
            resp+=al;
        }
        if (song->time>0) {
            if (!resp.isEmpty()) {
                resp+=Song::constSep;
            }
            resp+=Utils::formatTime(song->time);
        }
        return resp;
    }
    case Cantata::Role_ListImage:
        return true;
    case Cantata::Role_CoverSong: {
        QVariant v;
        v.setValue<Song>(*song);
        return v;
    }
    case Cantata::Role_Song: {
        QVariant var;
        var.setValue<Song>(*song);
        return var;
    }
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
    for (const Song &s: list) {
        fnames.append(s.file);
    }
    return fnames;
}

QList<Song> SearchModel::songs(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<Song> list;
    QSet<QString> files;
    for (const QModelIndex &index: indexes) {
        if (!index.isValid() || 0!=index.column()) {
            continue;
        }
        Song *song=static_cast<Song *>(index.internalPointer());
        if ((allowPlaylists || Song::Playlist!=song->type) && !files.contains(song->file)) {
            Song s=*song;
            fixPath(s);
            list << s;
            files << s.file;
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
    emit statsUpdated(0, 0);
}

void SearchModel::results(const QList<Song> &songs)
{
    beginResetModel();
    songList.clear();
    songList=songs;
    endResetModel();
    quint32 time=0;
    for (const Song &s: songList) {
        time+=s.time;
    }

    emit statsUpdated(songList.size(), time);
    emit searched();
}

#include "moc_searchmodel.cpp"
