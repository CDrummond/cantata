/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <QMimeData>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDateTime>
#include "support/localize.h"
#include "gui/plurals.h"
#include "dirviewmodel.h"
#include "dirviewitem.h"
#include "dirviewitemfile.h"
#include "playqueuemodel.h"
#include "roles.h"
#include "gui/settings.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdstats.h"
#include "support/icon.h"
#include "widgets/icons.h"
#include "config.h"
#include "support/utils.h"
#include "qtiocompressor/qtiocompressor.h"
#include "support/globalstatic.h"
#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

GLOBAL_STATIC(DirViewModel, instance)

DirViewModel::DirViewModel(QObject *parent)
    : ActionModel(parent)
    , rootItem(new DirViewItemRoot)
    , databaseTimeUnreliable(false)
    , enabled(false)
{
    #if defined ENABLE_MODEL_TEST
    new ModelTest(this, this);
    #endif
    connect(this, SIGNAL(loadFolers()), MPDConnection::self(), SLOT(loadFolders()));
}

DirViewModel::~DirViewModel()
{
    delete rootItem;
}

void DirViewModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;

    if (enabled) {
        connect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *, time_t)), this, SLOT(updateDirView(DirViewItemRoot *, time_t)));
        connect(MPDStats::self(), SIGNAL(updated()), this, SLOT(mpdStatsUpdated()));
    } else {
        clear();
        disconnect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *, time_t)), this, SLOT(updateDirView(DirViewItemRoot *, time_t)));
        disconnect(MPDStats::self(), SIGNAL(updated()), this, SLOT(mpdStatsUpdated()));
    }
}

QModelIndex DirViewModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const DirViewItem * parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem = static_cast<DirViewItem *>(parent.internalPointer());
    }

    DirViewItem * const childItem = parentItem->child(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }

    return QModelIndex();
}

QModelIndex DirViewModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    const DirViewItem * const childItem = static_cast<DirViewItem *>(index.internalPointer());
    DirViewItem * const parentItem = childItem->parent();

    if (parentItem == rootItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

QVariant DirViewModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int DirViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    const DirViewItem *parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem = static_cast<DirViewItem *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int DirViewModel::columnCount(const QModelIndex &) const
{
    return 1;
}

#ifdef ENABLE_UBUNTU
static const QString constFolderIcon=QLatin1String("qrc:/folder.svg");
#endif

QVariant DirViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    DirViewItem *item = static_cast<DirViewItem *>(index.internalPointer());

    switch (role) {
    #ifdef ENABLE_UBUNTU
    case Cantata::Role_Image:
        return DirViewItem::Type_Dir==item->type() ? constFolderIcon : QString();
    #else
    case Qt::DecorationRole: {
        if (DirViewItem::Type_Dir==item->type()) {
            return Icons::self()->folderIcon;
        } else if (DirViewItem::Type_File==item->type()) {
            return DirViewItemFile::Audio!=static_cast<DirViewItemFile *>(item)->fileType() ? Icons::self()->playlistIcon : Icons::self()->audioFileIcon;
        }
        break;
    }
    #endif
    case Cantata::Role_TitleText:
        #ifdef ENABLE_UBUNTU
        switch (item->type()) {
        case DirViewItem::Type_Dir:
            return static_cast<DirViewItemDir *>(item)->fullName();
        default:
        case DirViewItem::Type_File:
            return item->data();
        }
        #endif
    case Cantata::Role_MainText:
    case Qt::DisplayRole:
        return item->data();
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        return 0==item->childCount()
            ? item->data()
            : item->data()+"\n"+Plurals::entries(item->childCount());
    case Cantata::Role_SubText:
        switch (item->type()) {
        case DirViewItem::Type_Dir:
            return Plurals::entries(item->childCount());
        case DirViewItem::Type_File:
            switch (static_cast<DirViewItemFile *>(item)->fileType()) {
            case DirViewItemFile::Audio:    return i18n("Audio File");
            case DirViewItemFile::Playlist: return i18n("Playlist");
            case DirViewItemFile::CueSheet: return i18n("Cue Sheet");
            }
        default:
            return QVariant();
        }
    default:
        return ActionModel::data(index, role);
    }
    return QVariant();
}

void DirViewModel::load()
{
    if (enabled && (!rootItem || 0==rootItem->childCount())) {
        emit loadFolers();
    }
}

void DirViewModel::clear()
{
    if (!rootItem || 0==rootItem->childCount()) {
        return;
    }
    const DirViewItemRoot *oldRoot = rootItem;
    beginResetModel();
    databaseTime = 0;
    rootItem = new DirViewItemRoot;
    delete oldRoot;
    endResetModel();
}

void DirViewModel::updateDirView(DirViewItemRoot *newroot, time_t dbUpdate)
{
    if (databaseTime>0 && databaseTime >= dbUpdate) {
        delete newroot;
        return;
    }

    bool incremental=enabled && rootItem->childCount() && newroot->childCount();

    databaseTime=dbUpdate;
    if (incremental) {
        QSet<QString> currentFiles=rootItem->allFiles();
        QSet<QString> updateFiles=newroot->allFiles();
        QSet<QString> removed=currentFiles-updateFiles;
        QSet<QString> added=updateFiles-currentFiles;

        foreach (const QString &s, added) {
            if (s.startsWith(Song::constMopidyLocal)) {
                addFileToList(Song::decodePath(s), s);
            } else {
                addFileToList(s, QString());
            }
        }
        foreach (const QString &s, removed) {
            removeFileFromList(Song::decodePath(s));
        }
    } else {
        const DirViewItemRoot *oldRoot = rootItem;
        beginResetModel();
        rootItem = newroot;
        delete oldRoot;
        endResetModel();
    }

    #ifdef ENABLE_UBUNTU
    emit updated();
    #endif
}

void DirViewModel::mpdStatsUpdated()
{
    if (0==databaseTime || MPDStats::self()->dbUpdate() > databaseTime) {
        emit loadFolers();
    }
}

void DirViewModel::addFileToList(const QString &file, const QString &mopidyPath)
{
    if (!enabled) {
        return;
    }
    addFileToList(file.split('/'), QModelIndex(), rootItem, mopidyPath);
}

void DirViewModel::removeFileFromList(const QString &file)
{
    if (!enabled) {
        return;
    }
    removeFileFromList(file.split('/'), QModelIndex(), rootItem);
}

void DirViewModel::addFileToList(const QStringList &parts, const QModelIndex &parent, DirViewItemDir *dir, const QString &mopidyPath)
{
    if (0==parts.count()) {
        return;
    }

    QString p=parts[0];

    DirViewItem *child=dir->child(p);
    if (child) {
        if (DirViewItem::Type_Dir==child->type()) {
            addFileToList(parts.mid(1), index(dir->indexOf(child), 0, parent), static_cast<DirViewItemDir *>(child), mopidyPath);
        }
    } else {
        beginInsertRows(parent, dir->childCount(), dir->childCount());
        dir->insertFile(parts, mopidyPath);
        endInsertRows();
    }
}

void DirViewModel::removeFileFromList(const QStringList &parts, const QModelIndex &parent, DirViewItemDir *dir)
{
    if (0==parts.count()) {
        return;
    }

    QString p=parts[0];

    DirViewItem *child=dir->child(p);
    if (child) {
        if (DirViewItem::Type_Dir==child->type()) {
            removeFileFromList(parts.mid(1), index(dir->indexOf(child), 0, parent), static_cast<DirViewItemDir *>(child));
        } else if (DirViewItem::Type_File==child->type()) {
            int index=dir->indexOf(child);
            beginRemoveRows(parent, index, index);
            dir->remove(child);
            endRemoveRows();
        }
    }

    if (0==dir->childCount() && dir->parent()) {
        DirViewItemDir *pd=static_cast<DirViewItemDir *>(dir->parent());
        int index=pd->indexOf(dir);
        beginRemoveRows(parent.parent(), index, index);
        pd->remove(dir);
        delete dir;
        endRemoveRows();
    }
}

Qt::ItemFlags DirViewModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    } else {
        return Qt::ItemIsDropEnabled;
    }
}

static inline void addFile(DirViewItemFile *item, QList<DirViewItemFile *> &insertInto, QList<DirViewItemFile *> &checkAgainst, bool allowPlaylists)
{
    if ((allowPlaylists || DirViewItemFile::Audio==item->fileType()) && !checkAgainst.contains(item)) {
        insertInto << item;
    }
}

static bool lessThan(const DirViewItemFile *left, const DirViewItemFile *right)
{
    return left->fullName().compare(right->fullName(), Qt::CaseInsensitive) < 0;
}

static void getFiles(DirViewItem *item, QList<DirViewItemFile *> &files, bool allowPlaylists)
{
    if (!item) {
        return;
    }

    switch (item->type()) {
        case DirViewItem::Type_File:
            addFile(static_cast<DirViewItemFile *>(item), files, files, allowPlaylists);
        break;
        case DirViewItem::Type_Dir: {
            QList<DirViewItemFile *> dirFiles;
            for (int c=0; c<item->childCount(); c++) {
                DirViewItem *child=item->child(c);
                if (DirViewItem::Type_Dir==child->type()) {
                    getFiles(child, files, allowPlaylists);
                } else if (DirViewItem::Type_File==child->type()) {
                    addFile(static_cast<DirViewItemFile *>(child), dirFiles, files, allowPlaylists);
                }
            }

            qSort(dirFiles.begin(), dirFiles.end(), lessThan);
            files+=dirFiles;
        }
        default:
        break;
    }
}

QStringList DirViewModel::filenames(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<DirViewItemFile *> files;
    QStringList names;

    foreach(QModelIndex index, indexes) {
        getFiles(static_cast<DirViewItem *>(index.internalPointer()), files, allowPlaylists);
    }

    if (allowPlaylists) {
        QSet<DirViewItem *> cueFolders;

        foreach (const DirViewItemFile *f, files) {
            if (DirViewItemFile::Audio!=f->fileType()) {
                cueFolders.insert(f->parent());
            }
        }

        if (!cueFolders.isEmpty()) {
            foreach (const DirViewItemFile *f, files) {
                if (DirViewItemFile::Audio!=f->fileType() || !cueFolders.contains(f->parent())) {
                    names.append(f->fullName());
                }
            }
            return names;
        }
    }

    foreach (const DirViewItemFile *f, files) {
        names.append(f->fullName());
    }
    return names;
}

#ifndef ENABLE_UBUNTU
QMimeData *DirViewModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files=filenames(indexes, true);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
//    if (!MPDConnection::self()->getDetails().dir.isEmpty()) {
//        QStringList paths;
//        foreach (const QString &f, files) {
//            paths << MPDConnection::self()->getDetails().dir+f;
//        }
//        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
//    }
    return mimeData;
}
#endif
