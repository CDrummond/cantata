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
#include "localize.h"
#include "qtplural.h"
#include "dirviewmodel.h"
#include "dirviewitem.h"
#include "dirviewitemfile.h"
#include "playqueuemodel.h"
#include "musiclibrarymodel.h"
#include "itemview.h"
#include "settings.h"
#include "mpdconnection.h"
#include "icon.h"
#include "icons.h"
#include "config.h"
#include "utils.h"
#include "qtiocompressor/qtiocompressor.h"

#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(DirViewModel, instance)
#endif

static const QLatin1String constCacheName("-folder-listing");

static QString cacheFileName()
{
    MPDConnectionDetails details=MPDConnection::self()->getDetails();
    QString fileName=(!details.isLocal() ? details.hostname+'_'+QString::number(details.port) : details.hostname)
                     +constCacheName+MusicLibraryModel::constLibraryCompressedExt;
    fileName.replace('/', '_');
    fileName.replace('~', '_');
    return Utils::cacheDir(MusicLibraryModel::constLibraryCache)+fileName;
}

DirViewModel * DirViewModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static DirViewModel *instance=0;
    if(!instance) {
        instance=new DirViewModel;
        #if defined ENABLE_MODEL_TEST
        new ModelTest(instance, instance);
        #endif
    }
    return instance;
    #endif
}

DirViewModel::DirViewModel(QObject *parent)
    : ActionModel(parent)
    , rootItem(new DirViewItemRoot)
    , enabled(false)
{
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
        connect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *, const QDateTime &)), this, SLOT(updateDirView(DirViewItemRoot *, const QDateTime &)));
    } else {
        clear();
        removeCache();
        disconnect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *, const QDateTime &)), this, SLOT(updateDirView(DirViewItemRoot *, const QDateTime &)));
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

QVariant DirViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    DirViewItem *item = static_cast<DirViewItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole: {
        if (item->type() == DirViewItem::Type_Dir) {
            return Icons::self()->folderIcon;
        } else if (item->type() == DirViewItem::Type_File) {
            return DirViewItemFile::Audio!=static_cast<DirViewItemFile *>(item)->fileType() ? Icons::self()->playlistIcon : Icons::self()->audioFileIcon;
        }
        break;
    }
    case Qt::DisplayRole:
        return item->data();
    case Qt::ToolTipRole:
        return 0==item->childCount()
            ? item->data()
            : item->data()+"\n"+
                #ifdef ENABLE_KDE_SUPPORT
                i18np("1 Entry", "%1 Entries", item->childCount());
                #else
                QTP_ENTRIES_STR(item->childCount());
                #endif
    case ItemView::Role_SubText:
        switch (item->type()) {
        case DirViewItem::Type_Dir:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Entry", "%1 Entries", item->childCount());
            #else
            return QTP_ENTRIES_STR(item->childCount());
            #endif
            break;
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

void DirViewModel::clear()
{
    if (!rootItem || 0==rootItem->childCount()) {
        return;
    }
    const DirViewItemRoot *oldRoot = rootItem;
    beginResetModel();
    databaseTime = QDateTime();
    rootItem = new DirViewItemRoot;
    delete oldRoot;
    endResetModel();
}

static QLatin1String constTopTag("CantataFolders");
static const QString constVersionAttribute=QLatin1String("version");
static const QString constDateAttribute=QLatin1String("date");
static const QString constNameAttribute=QLatin1String("name");
static const QString constDirTag=QLatin1String("dir");
static const QString constFileTag=QLatin1String("file");

static quint32 constVersion=1;

void DirViewModel::toXML()
{
    QString filename=cacheFileName();
    // If saving device cache, and we have NO items, then remove cache file...
    if ((!rootItem || 0==rootItem->childCount()) && databaseTime==QDateTime()) {
        if (QFile::exists(filename)) {
            QFile::remove(filename);
        }
        return;
    }

    QFile file(filename);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::WriteOnly)) {
        return;
    }

    QXmlStreamWriter writer(&compressor);

    writer.writeStartDocument();
    writer.writeStartElement(constTopTag);
    writer.writeAttribute(constVersionAttribute, QString::number(constVersion));
    writer.writeAttribute(constDateAttribute, QString::number(databaseTime.toTime_t()));

    if (rootItem) {
        foreach (const DirViewItem *i, rootItem->childItems()) {
            toXML(i, writer);
        }
    }
    writer.writeEndElement();
    writer.writeEndDocument();
    compressor.close();
}

void DirViewModel::removeCache()
{
    QString cacheFile(cacheFileName());
    if (QFile::exists(cacheFile)) {
        QFile::remove(cacheFile);
    }

    databaseTime = QDateTime();
}

void DirViewModel::toXML(const DirViewItem *item, QXmlStreamWriter &writer)
{
    writer.writeStartElement(DirViewItem::Type_File==item->type() ? constFileTag : constDirTag);
    writer.writeAttribute(constNameAttribute, item->name());
    if (DirViewItem::Type_Dir==item->type()) {
        foreach (const DirViewItem *i, static_cast<const DirViewItemDir *>(item)->childItems()) {
            toXML(i, writer);
        }
    }
    writer.writeEndElement();
}

bool DirViewModel::fromXML()
{
    clear();
    QFile file(cacheFileName());
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::ReadOnly)) {
        return false;
    }

    DirViewItemRoot *root=new DirViewItemRoot;
    quint32 date=fromXML(&compressor, MPDStats::self()->dbUpdate(), root);
    compressor.close();
    if (!date) {
        delete root;
        return false;
    }

    QDateTime dt;
    dt.setTime_t(date);
    updateDirView(root, dt, true);
    return true;
}

quint32 DirViewModel::fromXML(QIODevice *dev, const QDateTime &dt, DirViewItemRoot *root)
{
    QXmlStreamReader reader(dev);
    quint32 xmlDate=0;
    DirViewItemDir *currentDir=root;
    QList<DirViewItemDir *> dirStack;

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.error()) {
            delete root;
            return 0;
        }
        if (reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                quint32 version = attributes.value(constVersionAttribute).toString().toUInt();
                xmlDate = attributes.value(constDateAttribute).toString().toUInt();

                if ( version < constVersion || (dt.isValid() && xmlDate < dt.toTime_t())) {
                    return 0;
                }
            } else if (constDirTag==element) {
                DirViewItemDir *dir=new DirViewItemDir(attributes.value(constNameAttribute).toString(), currentDir);
                currentDir->add(dir);
                dirStack.append(currentDir);
                currentDir=dir;
            } else if (constFileTag==element) {
                currentDir->add(new DirViewItemFile(attributes.value(constNameAttribute).toString(), currentDir));
            } else {
                return 0;
            }
        } else if (reader.isEndElement()) {
            if (constDirTag==reader.name().toString()) {
                currentDir=dirStack.takeLast();
            }
        }
    }

    return xmlDate;
}

void DirViewModel::updateDirView(DirViewItemRoot *newroot, const QDateTime &dbUpdate, bool fromFile)
{
    if (databaseTime.isValid() && databaseTime >= dbUpdate) {
        delete newroot;
        return;
    }

    bool incremental=enabled && rootItem->childCount() && newroot->childCount();
    bool updatedListing=false;
    bool needToUpdate=!databaseTime.isValid();
    bool needToSave=dbUpdate>databaseTime;

    if (incremental && !QFile::exists(cacheFileName())) {
        incremental=false;
    }

    if (incremental) {
        QSet<QString> currentFiles=rootItem->allFiles();
        QSet<QString> updateFiles=newroot->allFiles();
        QSet<QString> removed=currentFiles-updateFiles;
        QSet<QString> added=updateFiles-currentFiles;

        foreach (const QString &s, added) {
            addFileToList(s);
        }
        foreach (const QString &s, removed) {
            removeFileFromList(s);
        }
        updatedListing=!added.isEmpty() || !removed.isEmpty();
    } else {
        const DirViewItemRoot *oldRoot = rootItem;
        beginResetModel();
        rootItem = newroot;
        delete oldRoot;
        endResetModel();
        updatedListing=true;
    }

    // MPD proxy DB plugin does not provide a datetime for the DB. Therefore, just use current datetime
    // so that we dont keep requesting DB listing each time Cantata starts...
    //
    // Users of this plugin will have to force Cantata to refresh :-(
    //
    if (!databaseTime.isValid() && !dbUpdate.isValid()) {
        databaseTime=QDateTime::currentDateTime();
    }

    if ((updatedListing || needToUpdate) && (!fromFile && (needToSave || needToUpdate))) {
        toXML();
    }
}

void DirViewModel::addFileToList(const QString &file)
{
    if (!enabled) {
        return;
    }
    addFileToList(file.split('/'), QModelIndex(), rootItem);
}

void DirViewModel::removeFileFromList(const QString &file)
{
    if (!enabled) {
        return;
    }
    removeFileFromList(file.split('/'), QModelIndex(), rootItem);
}

void DirViewModel::addFileToList(const QStringList &parts, const QModelIndex &parent, DirViewItemDir *dir)
{
    if (0==parts.count()) {
        return;
    }

    QString p=parts[0];

    DirViewItem *child=dir->child(p);
    if (child) {
        if (DirViewItem::Type_Dir==child->type()) {
            addFileToList(parts.mid(1), index(dir->indexOf(child), 0, parent), static_cast<DirViewItemDir *>(child));
        }
    } else {
        beginInsertRows(parent, dir->childCount(), dir->childCount());
        dir->insertFile(parts);
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

QStringList DirViewModel::filenames(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QStringList fnames;

    foreach(QModelIndex index, indexes) {
        getFiles(static_cast<DirViewItem *>(index.internalPointer()), fnames, allowPlaylists);
    }
    return fnames;
}

QMimeData *DirViewModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files=filenames(indexes, true);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
    if (!MPDConnection::self()->getDetails().dir.isEmpty()) {
        QStringList paths;
        foreach (const QString &f, files) {
            paths << MPDConnection::self()->getDetails().dir+f;
        }
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}

static inline void addFile(DirViewItem *item, QStringList &insertInto, QStringList &checkAgainst, bool allowPlaylists)
{
    QString path=item->fullName();
    if ((allowPlaylists || !MPDConnection::isPlaylist(path)) && !checkAgainst.contains(path)) {
        insertInto << path;
    }
}

void DirViewModel::getFiles(DirViewItem *item, QStringList &filenames, bool allowPlaylists) const
{
    if (!item) {
        return;
    }

    switch (item->type()) {
        case DirViewItem::Type_File:
            addFile(item, filenames, filenames, allowPlaylists);
        break;
        case DirViewItem::Type_Dir: {
            QStringList dirFiles;
            for (int c=0; c<item->childCount(); c++) {
                DirViewItem *child=item->child(c);
                if (DirViewItem::Type_Dir==child->type()) {
                    getFiles(child, filenames, allowPlaylists);
                } else if (DirViewItem::Type_File==child->type()) {
                    addFile(child, dirFiles, filenames, allowPlaylists);
                }
            }

            qSort(dirFiles);
            filenames+=dirFiles;
        }
        default:
        break;
    }
}
