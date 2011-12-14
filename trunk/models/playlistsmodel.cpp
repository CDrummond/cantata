/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtCore/QModelIndex>
#include <QtGui/QMenu>
#include "playlistsmodel.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#include <KDE/KGlobal>
K_GLOBAL_STATIC(PlaylistsModel, instance)
#endif
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdconnection.h"

PlaylistsModel * PlaylistsModel::self()
{
#ifdef ENABLE_KDE_SUPPORT
    return instance;
#else
    static PlaylistsModel *instance=0;;
    if(!instance) {
        instance=new Covers;
    }
    return instance;
#endif
}

PlaylistsModel::PlaylistsModel(QObject *parent)
    : QAbstractItemModel(parent)
    , itemMenu(0)
{
    connect(MPDConnection::self(), SIGNAL(playlistsRetrieved(const QList<Playlist> &)), this, SLOT(setPlaylists(const QList<Playlist> &)));
    connect(MPDConnection::self(), SIGNAL(playlistInfoRetrieved(const QString &, const QList<Song> &)), this, SLOT(playlistInfoRetrieved(const QString &, const QList<Song> &)));
    connect(this, SIGNAL(listPlaylists()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(this, SIGNAL(playlistInfo(const QString &)), MPDConnection::self(), SLOT(playlistInfo(const QString &)));
    updateItemMenu();
}

PlaylistsModel::~PlaylistsModel()
{
    itemMenu->deleteLater();
    itemMenu=0;
}

QVariant PlaylistsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

// QModelIndex PlaylistsModel::index(int row, int column, const QModelIndex &parent) const
// {
//     Q_UNUSED(parent)
//
//     if(row<items.count())
//         return createIndex(row, column, (void *)&items.at(row));
//
//     return QModelIndex();
// }

int PlaylistsModel::rowCount(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return items.size();
    }

    return static_cast<Playlist *>(index.internalPointer())->songs.count();
}

QModelIndex PlaylistsModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

//     const MusicLibraryItem * const childItem = static_cast<MusicLibraryItem *>(index.internalPointer());
//     MusicLibraryItem * const parentItem = childItem->parent();
//
//     if (parentItem == rootItem)
        return QModelIndex();

//     return createIndex(parentItem->row(), 0, parentItem);
}

QModelIndex PlaylistsModel::index(int row, int col, const QModelIndex &parent) const
{
    if (!hasIndex(row, col, parent))
        return QModelIndex();

    if (parent.isValid()) {
        Playlist *pl=static_cast<Playlist *>(parent.internalPointer());
        return row<pl->songs.count() ? createIndex(row, col, (void *)&pl->songs.at(row)) : QModelIndex();
    }

    return row<items.count() ? createIndex(row, col, (void *)&items.at(row)) : QModelIndex();
}

QVariant PlaylistsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    qWarning() << index.row() << index.parent().row();
    if (index.parent().isValid()) {
        Playlist *pl=static_cast<Playlist *>(index.parent().internalPointer());

        if (index.row() >= pl->songs.size())
            return QVariant();

        const Song &song=pl->songs.at(index.row());

        switch(role) {
        case Qt::DisplayRole:
            if (song.track>9) {
                return QString::number(song.track)+QChar(' ')+song.title;
            } else if (song.track>0) {
                return QChar('0')+QString::number(song.track)+QChar(' ')+song.title;
            }
        case Qt::ToolTipRole: {
            QString duration=MPDParseUtils::formatDuration(song.time);
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(3);
            }
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(1);
            }
            return data(index, Qt::DisplayRole).toString()+QChar('\n')+duration;
        }
        default: break;
        }
    }

    if (index.row() >= items.size())
        return QVariant();

    const Playlist &pl=items.at(index.row());

    switch(role) {
    case Qt::DisplayRole: return pl.name;
    case Qt::ToolTipRole:
        return 0==pl.songs.count()
            ? pl.name
            :
                #ifdef ENABLE_KDE_SUPPORT
                i18np("%1\n1 Song", "%1\n%2 Songs", pl.name, pl.songs.count());
                #else
                (pl.songs.count()>1
                    ? tr("%1\n%2 Songs").arg(pl.name).arg(pl.songs.count())
                    : tr("%1\n1 Song").arg(pl.name);
                #endif
#if 0
            QString duration=MPDParseUtils::formatDuration(static_cast<MusicLibraryItemSong *>(item)->time());
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(3);
            }
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(1);
            }
            return song.name+!Char('\n')+duration;
#endif
    default: break;
    }

    return QVariant();
}

void PlaylistsModel::getPlaylists()
{
    emit listPlaylists();
}

void PlaylistsModel::clear()
{
    beginResetModel();
    items=QList<Playlist>();
    updateItemMenu();
    endResetModel();
}

bool PlaylistsModel::exists(const QString &n) const
{
    foreach (const Playlist &p, items) {
        if (p.name==n) {
            return true;
        }
    }

    return false;
}

void PlaylistsModel::setPlaylists(const QList<Playlist> &playlists)
{
    bool diff=playlists.count()!=items.count();

    if (!diff) {
        foreach (const Playlist &p, playlists) {
            if (!items.contains(p)) {
                diff=true;
                break;
            }
        }
    }
    if (diff) {
        beginResetModel();
        items=playlists;
        updateItemMenu();
    }
    foreach (const Playlist &p, items) {
        emit playlistInfo(p.name);
    }
    if (diff) {
        endResetModel();
    }
}

void PlaylistsModel::playlistInfoRetrieved(const QString &name, const QList<Song> &songs)
{
    int index=items.indexOf(Playlist(name));

    if (index>=0 && index<items.count()) {
        Playlist &pl=items[index];
        bool diff=songs.count()!=pl.songs.count();

        if (!diff) {
            foreach (const Song &s, songs) {
                if (-1!=pl.songs.indexOf(s)) {
                    diff=true;
                    break;
                }
            }
        }

        if (diff) {
//             if (pl.songs.count()) {
//                 beginRemoveRows(index(
//             }
            // TODO!!!!
            beginResetModel();
            pl.songs=songs;
            endResetModel();
        }
    } else {
        emit listPlaylists();
    }
}

static QString qt_strippedText(QString s)
{
    s.remove(QString::fromLatin1("..."));
    int i = 0;
    while (i < s.size()) {
        ++i;
        if (s.at(i - 1) != QLatin1Char('&')) {
            continue;
        }

        if (i < s.size() && s.at(i) == QLatin1Char('&')) {
            ++i;
        }
        s.remove(i - 1, 1);
    }
    return s.trimmed();
}

void PlaylistsModel::emitAddToExisting()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        emit addToExisting(qt_strippedText(act->text()));
    }
}

void PlaylistsModel::updateItemMenu()
{
    if (!itemMenu) {
        itemMenu = new QMenu(0);
    }

    itemMenu->clear();
    #ifdef ENABLE_KDE_SUPPORT
    itemMenu->addAction(QIcon::fromTheme("document-new"), i18n("New Playlist..."), this, SIGNAL(addToNew()));
    #else
    itemMenu->addAction(QIcon::fromTheme("document-new"), tr("New Playlist..."), this, SIGNAL(addToNew()));
    #endif

    QStringList names;
    foreach (const Playlist &p, items) {
        names << p.name;
    }
    qSort(names);
    foreach (const QString &n, names) {
        itemMenu->addAction(n, this, SLOT(emitAddToExisting()));
    }
}


