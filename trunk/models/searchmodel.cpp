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

#include "searchmodel.h"
#include "icons.h"
#include "itemview.h"
#include "localize.h"
#include "qtplural.h"
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

SearchModel::SearchModel(QObject *parent)
    : ActionModel(parent)
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

QVariant SearchModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int SearchModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return songList.count();
}

int SearchModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    const Song *song = toSong(index);

    if (!song) {
        return QVariant();
    }
    
    switch (role) {
    case Qt::DecorationRole:
        return song->isStream() ? Icons::self()->streamIcon : Icon("audio-x-generic");
    case Qt::DisplayRole:
    case Qt::ToolTipRole: {
        QString text=song->entryName();

        if (Qt::ToolTipRole==role) {
            text=text.replace("\n", "<br/>");
            if (!song->title.isEmpty()) {
                text+=QLatin1String("<br/>")+Song::formattedTime(song->time);
                text+=QLatin1String("<br/><small><i>")+song->file+QLatin1String("</i></small>");
            }
        }
        return text;
    }
    case ItemView::Role_MainText:
        return song->title.isEmpty() ? song->file : song->title;
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
    foreach(QModelIndex index, indexes) {
        Song *song=static_cast<Song *>(index.internalPointer());
        if ((allowPlaylists || Song::Playlist!=song->type) && !list.contains(*song)) {
            list << *song;
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
}

void SearchModel::search(const QString &key, const QString &value)
{
    if (key==currentKey && value==currentValue) {
        return;
    }
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
}
