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
#include "plurals.h"
#include "dirviewmodel.h"
#include "dirviewitem.h"
#include "dirviewitemfile.h"
#include "playqueuemodel.h"
#include "musiclibrarymodel.h"
#include "roles.h"
#include "settings.h"
#include "mpdconnection.h"
#include "icon.h"
#include "icons.h"
#include "config.h"
#include "utils.h"
#include "qtiocompressor/qtiocompressor.h"
#include "globalstatic.h"
#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
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
        connect(MPDConnection::self(), SIGNAL(updatingDatabase()), this, SLOT(updatingMpd()));
        connect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *, const QDateTime &)), this, SLOT(updateDirView(DirViewItemRoot *, const QDateTime &)));
    } else {
        clear();
        removeCache();
        disconnect(MPDConnection::self(), SIGNAL(updatingDatabase()), this, SLOT(updatingMpd()));
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
    #ifdef ENABLE_UBUNTU
    case Cantata::Role_TitleText:
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
static const QString constDateUnreliableAttribute=QLatin1String("dateUnreliable");
static const QString constNameAttribute=QLatin1String("name");
static const QString constPathAttribute=QLatin1String("path");
static const QString constDirTag=QLatin1String("dir");
static const QString constFileTag=QLatin1String("file");
static const QString constTrueValue=QLatin1String("true");

static quint32 constVersion=2;

void DirViewModel::toXML()
{
    QString filename=cacheFileName();
    if ((!rootItem || 0==rootItem->childCount()) && !MusicLibraryModel::validCacheDate(databaseTime)) {
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
    if (databaseTimeUnreliable) {
        writer.writeAttribute(constDateUnreliableAttribute, constTrueValue);
    }

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
    } else {
         const DirViewItemFile *f=static_cast<const DirViewItemFile *>(item);
         if (!f->filePath().isEmpty()) {
             writer.writeAttribute(constPathAttribute, f->filePath());
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
                databaseTimeUnreliable=constTrueValue==attributes.value(constDateUnreliableAttribute).toString();
            } else if (constDirTag==element) {
                DirViewItemDir *dir=new DirViewItemDir(attributes.value(constNameAttribute).toString(), currentDir);
                currentDir->add(dir);
                dirStack.append(currentDir);
                currentDir=dir;
            } else if (constFileTag==element) {
                currentDir->add(new DirViewItemFile(attributes.value(constNameAttribute).toString(),
                                                    attributes.value(constPathAttribute).toString(), currentDir));
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
    bool needToSave=!databaseTime.isValid() || (MusicLibraryModel::validCacheDate(dbUpdate) && dbUpdate>databaseTime);

    if (incremental && !QFile::exists(cacheFileName())) {
        incremental=false;
    }

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
        updatedListing=!added.isEmpty() || !removed.isEmpty();
    } else {
        const DirViewItemRoot *oldRoot = rootItem;
        beginResetModel();
        rootItem = newroot;
        delete oldRoot;
        endResetModel();
        updatedListing=true;
    }

    // MPD proxy DB plugin (MPD < 0.18.5) does not provide a datetime for the DB. Also, Mopidy
    // returns 0 for the database time (which equates to 1am Jan 1st 1970!). Therefore, in these
    // cases we just use current datetime so that we dont keep requesting DB listing each time
    // Cantata starts...
    //
    // Mopidy users, and users of the proxy DB plugin, will have to force Cantata to refresh :-(
    if (!fromFile) {
        databaseTimeUnreliable=!MusicLibraryModel::validCacheDate(dbUpdate); // See note in updatingMpd()
    }
    if (!MusicLibraryModel::validCacheDate(databaseTime) && !MusicLibraryModel::validCacheDate(dbUpdate)) {
        databaseTime=QDateTime::currentDateTime();
    }

    if (!fromFile && (needToSave || updatedListing)) {
        toXML();
    }

    #ifdef ENABLE_UBUNTU
    emit updated();
    #endif
}

void DirViewModel::updatingMpd()
{
    // MPD/Mopidy is being updated. If MPD's database-time is not reliable (as is the case for older proxy DBs, and Mopidy)
    // then we set the databaseTime to NOW when updated. This means we will miss any updates. So, for these scenarios, when
    // a user presses 'Refresh Database' in Cantata's main window, we need to reset our view of the databaseTime to null, so
    // that we update. This does mean that we will ALWAYS fetch the whole listing - but we have no way of knowing if it changed
    // or not :-(
    if (databaseTimeUnreliable) {
        removeCache();
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

QStringList DirViewModel::filenames(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QStringList fnames;

    foreach(QModelIndex index, indexes) {
        getFiles(static_cast<DirViewItem *>(index.internalPointer()), fnames, allowPlaylists);
    }
    return fnames;
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

static inline void addFile(DirViewItem *item, QStringList &insertInto, QStringList &checkAgainst, bool allowPlaylists)
{
    QString path=item->fullName();
    if ((allowPlaylists || !MPDConnection::isPlaylist(path)) && !checkAgainst.contains(path)) {
        insertInto << path;
    }
}


static bool lessThan(const QString &left, const QString &right)
{
    return left.compare(right, Qt::CaseInsensitive) < 0;
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

            qSort(dirFiles.begin(), dirFiles.end(), lessThan);
            filenames+=dirFiles;
        }
        default:
        break;
    }
}
