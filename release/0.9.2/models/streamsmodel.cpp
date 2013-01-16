/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtCore/QModelIndex>
#include <QtCore/QDataStream>
#include <QtCore/QTextStream>
#include <QtCore/QMimeData>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/qglobal.h>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include "config.h"
#include "settings.h"
#if defined Q_OS_WIN
#include <QtGui/QDesktopServices>
#endif
#include "localize.h"
#include "itemview.h"
#include "streamsmodel.h"
#include "playqueuemodel.h"
#include "mpdconnection.h"
#include "config.h"
#include "icons.h"
#include "utils.h"

const QLatin1String StreamsModel::constDefaultCategoryIcon("inode-directory");

static bool iconIsValid(const QString &icon)
{
    return icon.startsWith('/') ? QFile::exists(icon) : QIcon::hasThemeIcon(icon);
}

static QString getInternalFile(bool createDir=false)
{
    return Utils::configDir(QString(), createDir)+"streams.xml";
}

StreamsModel::StreamsModel()
    : QAbstractItemModel(0)
    , modified(false)
    , timer(0)
{
    connect(MPDConnection::self(), SIGNAL(urlHandlers(const QStringList &)), SLOT(urlHandlers(const QStringList &)));
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
        case Qt::DecorationRole: return cat->icon.isEmpty() ? Icon(constDefaultCategoryIcon)
                                                            : cat->icon.startsWith('/') ? QIcon(cat->icon) : Icon(cat->icon);
        case ItemView::Role_SubText:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Stream", "%1 Streams", cat->streams.count());
            #else
            return QTP_STREAMS_STR(cat->streams.count());
            #endif
        default: break;
        }
    } else {
        StreamItem *stream=static_cast<StreamItem *>(item);
        switch(role) {
        case Qt::DisplayRole:    return stream->name;
        case ItemView::Role_SubText:
        case Qt::ToolTipRole:    return stream->url;
        case Qt::DecorationRole: return stream->icon.isEmpty() ? Icons::streamIcon
                                                               : stream->icon.startsWith('/') ? QIcon(stream->icon) : Icon(stream->icon);
        default: break;
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
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QDomDocument doc;
    bool haveInserted=false;
    doc.setContent(&file);
    QDomElement root = doc.documentElement();

    if ("cantata" == root.tagName() && root.hasAttribute("version") && "1.0" == root.attribute("version")) {
        QDomElement category = root.firstChildElement("category");
        while (!category.isNull()) {
            if (category.hasAttribute("name")) {
                QString catName=category.attribute("name");
                QString catIcon=category.attribute("icon");
                CategoryItem *cat=getCategory(catName, true, !isInternal);
                if (cat && cat->icon.isEmpty() && !catIcon.isEmpty() && iconIsValid(catIcon)) {
                    cat->icon=catIcon;
                }
                QDomElement stream = category.firstChildElement("stream");
                while (!stream.isNull()) {
                    if (stream.hasAttribute("name") && stream.hasAttribute("url")) {
                        QString name=stream.attribute("name");
                        QString icon=stream.attribute("icon");
                        QString genre=stream.attribute("genre");
                        QString origName=name;
                        QUrl url=QUrl(stream.attribute("url"));

                        if (!entryExists(cat, QString(), url)) {
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
                                StreamItem *stream=new StreamItem(name, genre, iconIsValid(icon) ? icon : QString(), url, cat);
                                cat->itemMap.insert(url.toString(), stream);
                                cat->streams.append(stream);
                                if (!isInternal) {
                                    endInsertRows();
                                }
                            }
                        }
                    }
                    stream=stream.nextSiblingElement("stream");
                }
            }
            category=category.nextSiblingElement("category");
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
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QDomDocument doc;
    QDomElement root = doc.createElement("cantata");

    root.setAttribute("version", "1.0");
    doc.appendChild(root);
    foreach (CategoryItem *c, items) {
        if (selection.isEmpty() || selection.contains(c)) {
            QDomElement cat = doc.createElement("category");
            cat.setAttribute("name", c->name);
            if (!c->icon.isEmpty() && c->icon!=constDefaultCategoryIcon) {
                cat.setAttribute("icon", c->icon);
            }
            root.appendChild(cat);
            foreach (StreamItem *s, c->streams) {
                if (selection.isEmpty() || selection.contains(s)) {
                    QDomElement stream = doc.createElement("stream");
                    stream.setAttribute("name", s->name);
                    stream.setAttribute("url", s->url.toString());
                    if (!s->icon.isEmpty() && s->icon!=Icons::streamIcon.name()) {
                        stream.setAttribute("icon", s->icon);
                    }
                    if (!s->genre.isEmpty()) {
                        stream.setAttribute("genre", s->genre);
                    }
                    cat.appendChild(stream);
                }
            }
        }
    }

    QTextStream(&file) << doc.toString();
    return true;
}

bool StreamsModel::add(const QString &cat, const QString &name, const QString &genre, const QString &icon, const QString &url)
{
    QUrl u(url);
    CategoryItem *c=getCategory(cat, true, true);

    if (entryExists(c, name, url)) {
        return false;
    }

    beginInsertRows(createIndex(items.indexOf(c), 0, c), c->streams.count(), c->streams.count());
    StreamItem *stream=new StreamItem(name, genre, icon.isEmpty() || icon==Icons::streamIcon.name() ? QString() : icon, QUrl(url), c);
    c->itemMap.insert(url, stream);
    c->streams.append(stream);
    endInsertRows();

    modified=true;
    return true;
}

void StreamsModel::editCategory(const QModelIndex &index, const QString &name, const QString &icon)
{
    if (!index.isValid()) {
        return;
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isCategory() && (item->name!=name || item->icon!=icon)) {
        item->name=name;
        item->icon=icon.isEmpty() || icon==Icons::streamIcon.name() ? QString() : icon;
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
        if(add(newCat, name, genre, icon.isEmpty() || icon==Icons::streamIcon.name() ? QString() : icon, url)) {
            updateGenres();
            remove(index);
        }
        return;
    }

    QUrl u(url);
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
        if (stream->genre!=genre) {
            stream->genre=genre;
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

bool StreamsModel::entryExists(CategoryItem *cat, const QString &name, const QUrl &url)
{
    if(cat) {
        foreach (const StreamItem *s, cat->streams) {
            if ( (!name.isEmpty() && s->name==name) || (!url.isEmpty() && s->url==url)) {
                return true;
            }
        }
    }

    return false;
}

Qt::ItemFlags StreamsModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::NoItemFlags;
}

bool StreamsModel::validProtocol(const QString &file) const
{
    QString scheme=QUrl(file).scheme();
    return scheme.isEmpty() || handlers.contains(scheme);
}

static QString prefixName(const QString &n, bool addPrefix)
{
    if (!addPrefix || !n.startsWith("http:")) {
        return n;
    }
    return QLatin1String("cantata-")+n;
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
                    fnames << prefixName(f, addPrefix);
                }
            }
        } else if (!selectedCategories.contains(static_cast<StreamItem*>(item)->parent)) {
            QString f=static_cast<StreamItem*>(item)->url.toString();
            if (!fnames.contains(f) && validProtocol(f)) {
                fnames << prefixName(f, addPrefix);
            }
        }
    }

    return fnames;
}

QMimeData * StreamsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames(indexes, true));
    return mimeData;
}

void StreamsModel::persist()
{
    if (modified) {
        save(getInternalFile(true));
        modified=false;
    }
}

void StreamsModel::urlHandlers(const QStringList &h)
{
    handlers=h.toSet();
}

void StreamsModel::updateGenres()
{
    QSet<QString> genres;
    foreach (CategoryItem *c, items) {
        c->genres.clear();
        foreach (const StreamItem *s, c->streams) {
            if (!s->genre.isEmpty()) {
                c->genres.insert(s->genre);
                genres.insert(s->genre);
            }
        }
    }

    emit updateGenres(genres);
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
