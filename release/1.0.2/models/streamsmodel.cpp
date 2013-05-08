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

#include <QModelIndex>
#include <QDataStream>
#include <QTextStream>
#include <QMimeData>
#include <QStringList>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <qglobal.h>
#include <QCoreApplication>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QNetworkReply>
#include "config.h"
#include "settings.h"
#if defined Q_OS_WIN
#include <QDesktopServices>
#endif
#include "localize.h"
#include "itemview.h"
#include "streamsmodel.h"
#include "playqueuemodel.h"
#include "mpdconnection.h"
#include "config.h"
#include "icons.h"
#include "utils.h"
#include "qtiocompressor/qtiocompressor.h"
#include "networkaccessmanager.h"
#include "stdactions.h"
#include "mpdparseutils.h"

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(StreamsModel, instance)
#endif

StreamsModel * StreamsModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static StreamsModel *instance=0;
    if(!instance) {
        instance=new StreamsModel;
    }
    return instance;
    #endif
}

const QString StreamsModel::constPrefix("cantata-");
static const QString constStreamCategoryMimeType("cantata/streams-category");
static const QString constStreamMimeType("cantata/stream");
static const QLatin1String constSeparator("##Cantata##");
const QLatin1String StreamsModel::constGenreSeparator("|");

static QString encodeStreamItem(StreamsModel::StreamItem *i)
{
    return i->name.replace(constSeparator, " ")+constSeparator+
           i->url.toString()+constSeparator+
           i->genreString()+constSeparator+
           i->icon+constSeparator+
           i->parent->name;
}

struct DndStream
{
    QString name;
    QString url;
    QString genre;
    QString icon;
    QString category;
};

static DndStream decodeStreamItem(const QString &s)
{
    DndStream i;
    QStringList parts=s.split(constSeparator);
    if (parts.size()>=5) {
        i.name=parts.at(0);
        i.url=parts.at(1);
        i.genre=parts.at(2);
        i.icon=parts.at(3);
        i.category=parts.at(4);
    }
    return i;
}

static const QLatin1String constStreamsCompressedFileName("streams.xml.gz");
static const QLatin1String constStreamsOldFileName("streams.xml");

static void convertOldFile(const QString &compressedName)
{
    if (compressedName.startsWith("http:/")) {
        return;
    }
    QString prev=compressedName;
    prev.replace(constStreamsCompressedFileName, constStreamsOldFileName);

    if (QFile::exists(prev) && !QFile::exists(compressedName)) {
        QFile old(prev);
        if (old.open(QIODevice::ReadOnly)) {
            QByteArray a=old.readAll();
            old.close();

            QFile newFile(compressedName);
            QtIOCompressor compressor(&newFile);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::WriteOnly)) {
                compressor.write(a);
                compressor.close();
                QFile::remove(prev);
            }
        }
    }
}

QString StreamsModel::dir()
{
    return Settings::self()->storeStreamsInMpdDir() ? MPDConnection::self()->getDetails().dir : Utils::configDir(QString(), false);
}

static QString getInternalFile(bool createDir=false)
{
    if (Settings::self()->storeStreamsInMpdDir()) {
        return MPDConnection::self()->getDetails().dir+constStreamsCompressedFileName;
    }
    return Utils::configDir(QString(), createDir)+constStreamsCompressedFileName;
}

StreamsModel::StreamsModel()
    : ActionModel(0)
    , modified(false)
    , timer(0)
    , job(0)
{
}

StreamsModel::~StreamsModel()
{
}

QVariant StreamsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int StreamsModel::rowCount(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return items.size();
    }

    Item *item=static_cast<Item *>(index.internalPointer());
    if (item->isCategory()) {
        return static_cast<CategoryItem *>(index.internalPointer())->streams.count();
    }
    return 0;
}

bool StreamsModel::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid() || static_cast<Item *>(parent.internalPointer())->isCategory();
}

QModelIndex StreamsModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if(item->isCategory())
        return QModelIndex();
    else
    {
        StreamItem *stream=static_cast<StreamItem *>(item);

        if (stream->parent) {
            return createIndex(items.indexOf(stream->parent), 0, stream->parent);
        }
    }

    return QModelIndex();
}

QModelIndex StreamsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        Item *p=static_cast<Item *>(parent.internalPointer());

        if (p->isCategory()) {
            CategoryItem *cat=static_cast<CategoryItem *>(p);
            return row<cat->streams.count() ? createIndex(row, column, cat->streams.at(row)) : QModelIndex();
        }
    }

    return row<items.count() ? createIndex(row, column, items.at(row)) : QModelIndex();
}

QVariant StreamsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isCategory()) {
        CategoryItem *cat=static_cast<CategoryItem *>(item);
        switch(role) {
        case Qt::DisplayRole:    return cat->name;
        case Qt::ToolTipRole:
            return 0==cat->streams.count()
                ? cat->name
                : cat->name+"\n"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Stream", "%1 Streams", cat->streams.count());
                    #else
                    QTP_STREAMS_STR(cat->streams.count());
                    #endif
        case Qt::DecorationRole: {
            if (!cat->icon.isEmpty()) {
                QIcon i=icon(cat->icon);
                if (!i.isNull()) {
                    return i;
                }
            }
            return Icons::streamCategoryIcon;
        }
        case ItemView::Role_SubText:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Stream", "%1 Streams", cat->streams.count());
            #else
            return QTP_STREAMS_STR(cat->streams.count());
            #endif
        case ItemView::Role_Actions: {
//            QVariant v;
//            v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction);
//            return v;
            break;
        }
        }
    } else {
        StreamItem *stream=static_cast<StreamItem *>(item);
        switch(role) {
        case Qt::DisplayRole:    return stream->name;
        case ItemView::Role_SubText:
        case Qt::ToolTipRole:    return stream->url;
        case Qt::DecorationRole: {
            if (!stream->icon.isEmpty()) {
                QIcon i=icon(stream->icon);
                if (!i.isNull()) {
                    return i;
                }
            }
            return Icons::radioStreamIcon;
        }
        case ItemView::Role_Actions: {
            QVariant v;
            v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction);
            return v;
        }
        }
    }

    return QVariant();
}

void StreamsModel::reload()
{
    beginResetModel();
    clearCategories();
    // clearCategories sets modified, so we need to reset here - otherwise we save file on exit, eventhough nothing has changed!
    modified=false;
    load(getInternalFile(), true);
    endResetModel();
}

void StreamsModel::save(bool force)
{
    if (force) {
        if (timer) {
            timer->stop();
        }
        persist();
    } else if (!QFile::exists(getInternalFile(false)) || !QFileInfo(getInternalFile(false)).isWritable()) {
        if (timer) {
            timer->stop();
        }
        persist(); // Call persist now so as to log errors immediately
    } else {
        if (!timer) {
            timer=new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(persist()));
        }
        timer->start(30*1000);
    }
}

bool StreamsModel::load(const QString &filename, bool isInternal)
{
    if (isInternal) {
        if (filename.startsWith("http:/")) {
            if (job) {
                return false;
            }
            emit downloading(true);
            job=NetworkAccessManager::self()->get(QUrl(filename));
            connect(job, SIGNAL(finished()), SLOT(downloadFinished()));
            return true;
        } else {
            convertOldFile(filename);
        }
    }

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Check for gzip header...
    QByteArray header=file.read(2);
    bool isCompressed=((unsigned char)header[0])==0x1f && ((unsigned char)header[1])==0x8b;
    file.seek(0);

    QtIOCompressor compressor(&file);
    if (isCompressed) {
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (!compressor.open(QIODevice::ReadOnly)) {
            return false;
        }
    }
    return load(isCompressed ? (QIODevice *)&compressor : (QIODevice *)&file, isInternal);
}

void StreamsModel::downloadFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());

    if (reply==job) {
        job=0;
        if(QNetworkReply::NoError==reply->error()) {
            QtIOCompressor comp(reply);
            comp.setStreamFormat(QtIOCompressor::GzipFormat);
            if (comp.open(QIODevice::ReadOnly)) {
                beginResetModel();
                if (!load(&comp, true)) {
                    emit error(i18n("Failed to parse downloaded stream list."));
                }
                endResetModel();
            } else {
                emit error(i18n("Failed to read downloaded stream list."));
            }
        } else {
            emit error(i18n("Failed to download stream list."));
        }
        emit downloading(false);
    }
    reply->deleteLater();
}

bool StreamsModel::load(QIODevice *dev, bool isInternal)
{
    QXmlStreamReader doc(dev);
    bool haveInserted=false;
    int level=0;
    CategoryItem *cat=0;
    QString unknown=i18n("Unknown");

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            ++level;
            if (2==level && QLatin1String("category")==doc.name()) {
                QString catName=doc.attributes().value("name").toString();
                QString catIcon=doc.attributes().value("icon").toString();
                cat=getCategory(catName, true, !isInternal);
                if (cat && cat->icon.isEmpty() && !catIcon.isEmpty()) {
                    cat->icon=catIcon;
                }
            } else if (cat && 3==level && QLatin1String("stream")==doc.name()) {
                QString name=doc.attributes().value("name").toString();
                QString icon=doc.attributes().value("icon").toString();
                QString genre=doc.attributes().value("genre").toString();
                QString origName=name;
                QUrl url=QUrl(doc.attributes().value("url").toString());

                if (!name.isEmpty() && url.isValid() && (isInternal || !entryExists(cat, QString(), url))) {
                    int i=1;
                    for (; i<100 && entryExists(cat, name); ++i) {
                        name=origName+QLatin1String("_")+QString::number(i);
                    }

                    if (i<100) {
                        if (!haveInserted) {
                            haveInserted=true;
                        }
                        if (!isInternal) {
                            beginInsertRows(createIndex(items.indexOf(cat), 0, cat), cat->streams.count(), cat->streams.count());
                        }
                        StreamItem *stream=new StreamItem(name, genre.isEmpty() ? unknown : genre, icon, url, cat);
                        cat->itemMap.insert(url.toString(), stream);
                        cat->streams.append(stream);
                        if (!isInternal) {
                            endInsertRows();
                        }
                    }
                }
            }
        } else if (doc.isEndElement()) {
            --level;
            if (2==level && QLatin1String("category")==doc.name()) {
                cat=0;
            }
        }
    }

    if (haveInserted) {
        updateGenres();
    }
    if (haveInserted && !isInternal) {
        modified=true;
        save();
    }

    return haveInserted;
}

bool StreamsModel::save(const QString &filename, const QSet<StreamsModel::Item *> &selection)
{
    QFile file(filename);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::WriteOnly)) {
        return false;
    }

    QXmlStreamWriter doc(&compressor);
    doc.writeStartDocument();
    doc.writeStartElement("streams");
    doc.writeAttribute("version", "1.0");
    if (filename==getInternalFile(false)) {
        doc.setAutoFormatting(false);
    } else {
        doc.setAutoFormatting(true);
        doc.setAutoFormattingIndent(1);
    }

    QString unknown=i18n("Unknown");

    foreach (CategoryItem *c, items) {
        if (selection.isEmpty() || selection.contains(c)) {
            doc.writeStartElement("category");
            doc.writeAttribute("name", c->name);
            if (!c->icon.isEmpty()) {
                doc.writeAttribute("icon", c->icon);
            }
            foreach (StreamItem *s, c->streams) {
                if (selection.isEmpty() || selection.contains(s)) {
                    doc.writeStartElement("stream");
                    doc.writeAttribute("name", s->name);
                    doc.writeAttribute("url", s->url.toString());
                    if (!s->icon.isEmpty()) {
                        doc.writeAttribute("icon", s->icon);
                    }
                    QSet<QString> genres=s->genres;
                    genres.remove(unknown);
                    if (!genres.isEmpty()) {
                        doc.writeAttribute("genre", QStringList(genres.toList()).join(constGenreSeparator));
                    }
                    doc.writeEndElement();
                }
            }
            doc.writeEndElement();
        }
    }
    doc.writeEndElement();
    doc.writeEndDocument();
    return true;
}

bool StreamsModel::add(const QString &cat, const QString &name, const QString &genre, const QString &icon, const QString &url)
{
    CategoryItem *c=getCategory(cat, true, true);

    if (entryExists(c, name, url)) {
        return false;
    }

    beginInsertRows(createIndex(items.indexOf(c), 0, c), c->streams.count(), c->streams.count());
    StreamItem *stream=new StreamItem(name, genreSet(genre), icon, QUrl(url), c);
    c->itemMap.insert(url, stream);
    c->streams.append(stream);
    endInsertRows();
    updateGenres();
    modified=true;
    save();
    return true;
}

void StreamsModel::add(const QString &cat, const QString &icon, const QList<StreamsModel::StreamItem *> &streams)
{
    if (streams.isEmpty()) {
        return;
    }
    StreamsModel::CategoryItem *ci=getCategory(cat, false, true);
    if (ci) {
        removeCategory(ci);
    }
    ci=getCategory(cat, true, true);
    ci->icon=icon;
    beginInsertRows(createIndex(items.indexOf(ci), 0, ci), 0, streams.count()-1);
    foreach (StreamsModel::StreamItem *s, streams) {
        s->parent=ci;
        ci->itemMap.insert(s->url.toString(), s);
        ci->streams.append(s);
    }
    endInsertRows();
    updateGenres();
    modified=true;
    save();
}

void StreamsModel::editCategory(const QModelIndex &index, const QString &name, const QString &icon)
{
    if (!index.isValid()) {
        return;
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isCategory() && (item->name!=name || item->icon!=icon)) {
        item->name=name;
        item->icon=icon;
        emit dataChanged(index, index);
        modified=true;
        save();
    }
}

void StreamsModel::editStream(const QModelIndex &index, const QString &oldCat, const QString &newCat, const QString &name, const QString &genre, const QString &icon, const QString &url)
{
    if (!index.isValid()) {
        return;
    }

    CategoryItem *cat=getCategory(oldCat);

    if (!cat) {
        return;
    }

    if (!newCat.isEmpty() && oldCat!=newCat) {
        if(add(newCat, name, genre, icon, url)) {
            updateGenres();
            remove(index);
        }
        return;
    }

    int row=index.row();

    if (row<cat->streams.count()) {
        StreamItem *stream=cat->streams.at(row);
        QString oldUrl(stream->url.toString());
        stream->name=name;
        stream->url=url;
        stream->icon=icon;
        if (oldUrl!=url) {
            cat->itemMap.remove(oldUrl);
            cat->itemMap.insert(url, stream);
        }
        QSet<QString> genres=genreSet(genre);
        if (stream->genres!=genres) {
            stream->genres=genres;
            updateGenres();
        }
        emit dataChanged(index, index);
        modified=true;
        save();
    }
}

void StreamsModel::remove(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    int row=index.row();
    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isCategory()) {
        if (row<items.count()) {
            CategoryItem *old=items.at(row);
            beginRemoveRows(QModelIndex(), row, row);
            items.removeAt(row);
            delete old;
            endRemoveRows();
            modified=true;
            updateGenres();
        }
    } else {
        StreamItem *stream=static_cast<StreamItem *>(item);
        if (row<stream->parent->streams.count()) {
            CategoryItem *cat=stream->parent;
            StreamItem *old=cat->streams.at(row);

            /*if (1==cat->streams.count()) {
                int catRow=items.indexOf(cat);
                beginRemoveRows(QModelIndex(), catRow, catRow);
                items.removeAt(catRow);
                delete cat;
                endRemoveRows();
            } else*/ {
                beginRemoveRows(createIndex(items.indexOf(cat), 0, cat), row, row);
                cat->streams.removeAt(row);
                cat->itemMap.remove(old->url.toString());
                endRemoveRows();
                delete old;
                updateGenres();
            }
            modified=true;
        }
    }

    save();
}

void StreamsModel::removeCategory(CategoryItem *cat)
{
    int row=items.indexOf(cat);
    if (-1==row) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    items.removeAt(row);
    delete cat;
    endRemoveRows();
    modified=true;
}

void StreamsModel::removeStream(StreamItem *stream)
{
    int parentRow=items.indexOf(stream->parent);
    if (-1==parentRow) {
        return;
    }
    CategoryItem *cat=stream->parent;
    int row=cat->streams.indexOf(stream);
    if (-1==row) {
        return;
    }
    beginRemoveRows(createIndex(parentRow, 0, cat), row, row);
    cat->streams.removeAt(row);
    cat->itemMap.remove(stream->url.toString());
    endRemoveRows();
    delete stream;
    modified=true;
}

void StreamsModel::removeStream(const QString &category, const QString &name, const QString &url)
{
    CategoryItem *cat=getCategory(category);
    if (!cat) {
        return;
    }
    int parentRow=items.indexOf(cat);
    if (-1==parentRow) {
        return;
    }
    StreamItem *stream=getStream(cat, name, QUrl(url));
    if (0==stream) {
        return;
    }
    int row=cat->streams.indexOf(stream);
    if (-1==row) {
        return;
    }
    beginRemoveRows(createIndex(parentRow, 0, cat), row, row);
    cat->streams.removeAt(row);
    cat->itemMap.remove(stream->url.toString());
    endRemoveRows();
    delete stream;

    if (cat->streams.isEmpty()) {
        removeCategory(cat);
    }
    modified=true;
}

StreamsModel::CategoryItem * StreamsModel::getCategory(const QString &name, bool create, bool signal)
{
    foreach (CategoryItem *c, items) {
        if (c->name==name) {
            return c;
        }
    }

    if (create) {
        CategoryItem *cat=new CategoryItem(name);
        if (signal) {
            beginInsertRows(QModelIndex(), items.count(), items.count());
        }
        items.append(cat);
        if (signal) {
            endInsertRows();
        }
        return cat;
    }
    return 0;
}

QString StreamsModel::name(CategoryItem *cat, const QString &url)
{
    if(cat) {
        QHash<QString, StreamItem *>::ConstIterator it=cat->itemMap.find(url);

        return it==cat->itemMap.end() ? QString() : it.value()->name;
    }

    return QString();
}

StreamsModel::StreamItem * StreamsModel::getStream(CategoryItem *cat, const QString &name, const QUrl &url)
{
    if(cat) {
        foreach (StreamItem *s, cat->streams) {
            if ( (!name.isEmpty() && s->name==name) || (!url.isEmpty() && s->url==url)) {
                return s;
            }
        }
    }

    return 0;
}

Qt::ItemFlags StreamsModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return index.internalPointer() &&  static_cast<Item*>(index.internalPointer())->isCategory()
                ? (Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled)
                : (Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled);
    } else {
        return Qt::NoItemFlags;
    }
}


Qt::DropActions StreamsModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

bool StreamsModel::validProtocol(const QString &file) const
{
    QString scheme=QUrl(file).scheme();
    return scheme.isEmpty() || MPDConnection::self()->urlHandlers().contains(scheme);
}

QString StreamsModel::modifyUrl(const QString &u, bool addPrefix, const QString &name)
{
    return MPDParseUtils::addStreamName(!addPrefix || !u.startsWith("http:") ? u : (constPrefix+u), name);
}

QStringList StreamsModel::filenames(const QModelIndexList &indexes, bool addPrefix) const
{
    QStringList fnames;
    QSet<Item *> selectedCategories;
    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isCategory()) {
            selectedCategories.insert(item);
            foreach (const StreamItem *s, static_cast<CategoryItem*>(item)->streams) {
                QString f=s->url.toString();
                if (!fnames.contains(f) && validProtocol(f)) {
                    fnames << modifyUrl(f, addPrefix, s->name);
                }
            }
        } else if (!selectedCategories.contains(static_cast<StreamItem*>(item)->parent)) {
            QString f=static_cast<StreamItem*>(item)->url.toString();
            if (!fnames.contains(f) && validProtocol(f)) {
                fnames << modifyUrl(f, addPrefix, static_cast<StreamItem*>(item)->name);
            }
        }
    }

    return fnames;
}

QMimeData * StreamsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames(indexes, true));
    QStringList categories;
    QStringList streams;

    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isCategory()) {
            categories.append(item->name);
        } else {
            streams.append(encodeStreamItem(static_cast<StreamItem *>(item)));
        }
    }

    if (!categories.isEmpty()) {
        PlayQueueModel::encode(*mimeData, constStreamCategoryMimeType, categories);
    }
    if (!streams.isEmpty()) {
        PlayQueueModel::encode(*mimeData, constStreamMimeType, streams);
    }
    return mimeData;
}

bool StreamsModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int col, const QModelIndex &parent)
{
    Q_UNUSED(col)
    Q_UNUSED(row)

    if (!writable) {
        return false;
    }

    if (Qt::IgnoreAction==action) {
        return true;
    }

    if (!parent.isValid()) {
        return false;
    }

    if (data->hasFormat(constStreamCategoryMimeType)) {
        // Cant drag categories onto categories...
        return false;
    }

    if (data->hasFormat(constStreamMimeType)) {
        Item *item=static_cast<Item *>(parent.internalPointer());
        if (!item->isCategory()) {
            // Should not happen, due to flags() - but make sure!!!
            return false;
        }
        CategoryItem *dest=static_cast<CategoryItem *>(item);
        QStringList streams=PlayQueueModel::decode(*data, constStreamMimeType);
        bool ok=false;

        foreach (const QString &s, streams) {
            DndStream stream=decodeStreamItem(s);
            if (!stream.category.isEmpty() && stream.category!=dest->name) {
                if (add(dest->name, stream.name, stream.genre, stream.icon, stream.url)) {
                    removeStream(stream.category, stream.name, stream.url);
                }
                ok=true;
            }
        }
        return ok;
    }

    return false;
}

QStringList StreamsModel::mimeTypes() const
{
    QStringList types;
    types << PlayQueueModel::constFileNameMimeType << constStreamCategoryMimeType;
    return types;
}


void StreamsModel::persist()
{
    if (modified) {
        QString fileName=getInternalFile(true);
        modified=false;
        if (items.isEmpty()) {
            // No entries, so remove file...
            if (QFile::exists(fileName) && !QFile::remove(fileName)) {
                emit error(i18n("Failed to save stream list. Please check %1 is writable.").arg(fileName));
                reload();
            }
        }
        else if (save(fileName)) {
            Utils::setFilePerms(fileName);
        } else {
            emit error(i18n("Failed to save stream list. Please check %1 is writable.").arg(fileName));
            reload();
        }
    }
}

void StreamsModel::updateGenres()
{
    QSet<QString> genres;
    foreach (CategoryItem *c, items) {
        c->genres.clear();
        foreach (const StreamItem *s, c->streams) {
            c->genres+=s->genres;
            genres+=s->genres;
        }
    }

    emit updateGenres(genres);
}

bool StreamsModel::checkWritable()
{
    QString dirName=dir();
    bool isHttp=dirName.startsWith("http:/");
    writable=!isHttp && QFileInfo(dirName).isWritable();
    if (writable) {
        QString fileName=getInternalFile(false);
        if (QFile::exists(fileName) && !QFile(fileName).isWritable()) {
            writable=false;
        }
    }
    return writable;
}

Action * StreamsModel::getAction(const QModelIndex &idx, int num)
{
    Q_UNUSED(idx)
    return 0==num ? StdActions::self()->replacePlayQueueAction : 0;
}

void StreamsModel::clearCategories()
{
    qDeleteAll(items);
    items.clear();
    modified=true;
}

void StreamsModel::CategoryItem::clearStreams()
{
    qDeleteAll(streams);
    streams.clear();
}

const QMap<QString, QIcon> & StreamsModel::icons()
{
    static bool loaded=false;
    if (!loaded) {
        loaded=true;
        #ifdef Q_OS_WIN
        QString dir(QCoreApplication::applicationDirPath()+"/streamicons/");
        #else
        QString dir(QString(INSTALL_PREFIX"/share/")+QCoreApplication::applicationName()+"/streamicons/");
        #endif
        QStringList names=QDir(dir).entryList(QStringList() << "*.svg" << "*.png");
        foreach (const QString &name, names) {
            QString n=QString(name).remove(".svg").remove(".png");
            if (!iconMap.contains(n)) {
                iconMap.insert(n, QIcon(dir+name));
            }
        }
    }
    return iconMap;
}

QIcon StreamsModel::icon(const QString &name) const
{
    if (name.isEmpty()) {
        return QIcon();
    }

    if (!iconMap.contains(name)) {
        #ifdef Q_OS_WIN
        QString dir(QCoreApplication::applicationDirPath()+"/streamicons/");
        #else
        QString dir(QString(INSTALL_PREFIX"/share/")+QCoreApplication::applicationName()+"/streamicons/");
        #endif
        iconMap.insert(name, QFile::exists(dir+name+".svg") ? QIcon(dir+name+".svg") : (QFile::exists(dir+name+".png") ? QIcon(dir+name+".png") : QIcon()));
    }

    return iconMap[name];
}
