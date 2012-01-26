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
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QtGui/QIcon>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include "itemview.h"
#include "streamsmodel.h"
#include "playqueuemodel.h"
#include "config.h"

static QString configDir()
{
    QString env = qgetenv("XDG_CONFIG_HOME");
    QString dir = (env.isEmpty() ? QDir::homePath() + "/.config/" : env) + QLatin1String("/"PACKAGE_NAME"/");
    QDir d(dir);
    return d.exists() || d.mkpath(dir) ? QDir::toNativeSeparators(dir) : QString();
}

static QString getInternalFile()
{
    return configDir()+"streams.xml";
}

StreamsModel::StreamsModel()
    : QAbstractItemModel(0)
    , modified(false)
    , timer(0)
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
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Stream", "%1\n%2 Streams", cat->name, cat->streams.count());
                    #else
                    (cat->streams.count()>1
                        ? tr("%1\n%2 Streams").arg(cat->name).arg(cat->streams.count())
                        : tr("%1\n1 Stream").arg(cat->name));
                    #endif
        case Qt::DecorationRole: return QIcon::fromTheme("inode-directory");
        case ItemView::Role_SubText:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Stream", "%1 Streams", cat->streams.count());
            #else
            return (cat->streams.count()>1
                ? tr("%1 Streams").arg(cat->streams.count())
                : tr("1 Stream"));
            #endif
        default: break;
        }
    } else {
        StreamItem *stream=static_cast<StreamItem *>(item);
        switch(role) {
        case Qt::DisplayRole:         return stream->name;
        case ItemView::Role_SubText:
        case Qt::ToolTipRole:         return stream->url;
        case Qt::DecorationRole:      return QIcon::fromTheme("applications-internet");
        default: break;
        }
    }

    return QVariant();
}

void StreamsModel::reload()
{
    beginResetModel();
    clearCategories();
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
                CategoryItem *cat=getCategory(catName, true, !isInternal);
                QDomElement stream = category.firstChildElement("stream");
                while (!stream.isNull()) {
                    if (stream.hasAttribute("name") && stream.hasAttribute("url")) {
                        QString name=stream.attribute("name");
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
                                StreamItem *stream=new StreamItem(name, url, cat);
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

    if (haveInserted && !isInternal) {
        modified=true;
        save();
    }

    return haveInserted;
}

bool StreamsModel::save(const QString &filename, const QModelIndexList &selection)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QDomDocument doc;
    QDomElement root = doc.createElement("cantata");
    QSet<const Item *> selectedItems;
    foreach (const QModelIndex &idx, selection) {
        selectedItems.insert(static_cast<const Item *>(idx.internalPointer()));
    }
    root.setAttribute("version", "1.0");
    doc.appendChild(root);
    foreach (const CategoryItem *c, items) {
        if (selectedItems.isEmpty() || selectedItems.contains(c)) {
            QDomElement cat = doc.createElement("category");
            cat.setAttribute("name", c->name);
            root.appendChild(cat);
            foreach (const StreamItem *s, c->streams) {
                QDomElement stream = doc.createElement("stream");
                stream.setAttribute("name", s->name);
                stream.setAttribute("url", s->url.toString());
                cat.appendChild(stream);
            }
        }
    }

    QTextStream(&file) << doc.toString();
    return true;
}

bool StreamsModel::add(const QString &cat, const QString &name, const QString &url)
{
    QUrl u(url);
    CategoryItem *c=getCategory(cat, true, true);

    if (entryExists(c, name, url)) {
        return false;
    }

    beginInsertRows(createIndex(items.indexOf(c), 0, c), c->streams.count(), c->streams.count());
    StreamItem *stream=new StreamItem(name, QUrl(url), c);
    c->itemMap.insert(url, stream);
    c->streams.append(stream);
    endInsertRows();

    modified=true;
    return true;
}

void StreamsModel::editCategory(const QModelIndex &index, const QString &name)
{
    if (!index.isValid()) {
        return;
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isCategory() && item->name!=name) {
        item->name=name;
        emit dataChanged(index, index);
        modified=true;
        save();
    }
}

void StreamsModel::editStream(const QModelIndex &index, const QString &oldCat, const QString &newCat, const QString &name, const QString &url)
{
    if (!index.isValid()) {
        return;
    }

    CategoryItem *cat=getCategory(oldCat);

    if (!cat) {
        return;
    }

    if (!newCat.isEmpty() && oldCat!=newCat) {
        if(add(newCat, name, url)) {
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
        if (oldUrl!=url) {
            cat->itemMap.remove(oldUrl);
            cat->itemMap.insert(url, stream);
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
            items.removeAt(index.row());
            delete old;
            endRemoveRows();
            modified=true;
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
                cat->streams.removeAt(index.row());
                cat->itemMap.remove(old->url.toString());
                endRemoveRows();
                delete old;
            }
            modified=true;
        }
    }

    save();
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

QStringList StreamsModel::filenames(const QModelIndexList &indexes) const
{
    QStringList fnames;
    QSet<Item *> selectedCategories;
    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isCategory()) {
            selectedCategories.insert(item);
            foreach (const StreamItem *s, static_cast<CategoryItem*>(item)->streams) {
                QString f=s->url.toString();
                if (!fnames.contains(f)) {
                    fnames << f;
                }
            }
        } else if (!selectedCategories.contains(static_cast<StreamItem*>(item)->parent)) {
            QString f=static_cast<StreamItem*>(item)->url.toString();
            if (!fnames.contains(f)) {
                fnames << f;
            }
        }
    }

    return fnames;
}

QMimeData * StreamsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames(indexes));
    return mimeData;
}

void StreamsModel::persist()
{
    if (modified) {
        save(getInternalFile());
        modified=false;
    }
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
