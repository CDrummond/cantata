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

#include "streamsmodel.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdparseutils.h"
#include "widgets/icons.h"
#include "network/networkaccessmanager.h"
#include "support/utils.h"
#include "support/monoicon.h"
#include "gui/settings.h"
#include "playqueuemodel.h"
#include "roles.h"
#include "support/action.h"
#include "gui/stdactions.h"
#include "support/actioncollection.h"
#include "digitallyimported.h"
#include "qtiocompressor/qtiocompressor.h"
#include "config.h"
#include "support/globalstatic.h"
#include <QModelIndex>
#include <QString>
#include <QSet>
#include <QVariant>
#include <QMimeData>
#include <QXmlStreamReader>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QLocale>
#include <QFileSystemWatcher>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#if defined Q_OS_WIN
#include <QCoreApplication>
#endif
#include <stdio.h>
#include <time.h>
#include <algorithm>

#include <QDebug>
GLOBAL_STATIC(StreamsModel, instance)

const QString StreamsModel::constSubDir=QLatin1String("streams");
const QString StreamsModel::constCacheExt=QLatin1String(".xml.gz");
const QString StreamsModel::constShoutCastHost=QLatin1String("api.shoutcast.com");
const QString StreamsModel::constDirbleHost=QLatin1String("api.dirble.com");
const QString StreamsModel::constCommunityHost=QLatin1String("www.radio-browser.info");
const QString StreamsModel::constCompressedXmlFile=QLatin1String("streams.xml.gz");
const QString StreamsModel::constXmlFile=QLatin1String("streams.xml");
const QString StreamsModel::constSettingsFile=QLatin1String("settings.json");
const QString StreamsModel::constPngIcon=QLatin1String("icon.png");
const QString StreamsModel::constSvgIcon=QLatin1String("icon.svg");

static const char * constOrigUrlProperty = "orig-url";

static QString constRadioTimeHost=QLatin1String("opml.radiotime.com");
static QString constRadioTimeUrl=QLatin1String("http://")+constRadioTimeHost+QLatin1String("/Browse.ashx");
static QString constFavouritesUrl=QLatin1String("cantata://internal");
static QString constIceCastUrl=QLatin1String("http://dir.xiph.org/yp.xml");

static const QString constDiChannelListHost=QLatin1String("api.v2.audioaddict.com");
static const QString constDiChannelListUrl=QLatin1String("http://")+constDiChannelListHost+("/v1/%1/mobile/batch_update?asset_group_key=mobile_icons&stream_set_key=");
static const QString constDiStdUrl=QLatin1String("http://%1/public3/%2.pls");

static QString constShoutCastUrl=QLatin1String("http://")+StreamsModel::constShoutCastHost+QLatin1String("/genre/primary?f=xml");
static QString constDirbleUrl=QLatin1String("http://")+StreamsModel::constDirbleHost+QLatin1String("/v2/categories/primary");

static const QLatin1String constBookmarksDir=QLatin1String("bookmarks");

static QIcon getExternalIcon(const QString &path, QStringList files=QStringList() << StreamsModel::constSvgIcon <<  StreamsModel::constPngIcon)
{
    for (const QString &file: files) {
        QString iconFile=path+Utils::constDirSep+file;
        if (QFile::exists(iconFile)) {
            QIcon icon;
            icon.addFile(iconFile);
            return icon;
        }
    }

    return QIcon();
}

static QString categoryCacheName(const QString &name, bool createDir=false)
{
    return Utils::cacheDir(StreamsModel::constSubDir, createDir)+name+StreamsModel::constCacheExt;
}

static QString categoryBookmarksName(const QString &name, bool createDir=false)
{
    return Utils::dataDir(constBookmarksDir, createDir)+name+StreamsModel::constCacheExt;
}

QString StreamsModel::Item::modifiedName() const
{
    if (isCategory()) {
        return name;
    }
    const CategoryItem *cat=getTopLevelCategory();
    if (!cat || !cat->addCatToModifiedName || name.startsWith(cat->name)) {
        return name;
    }
    return cat->name+Song::constSep+name;
}

StreamsModel::CategoryItem * StreamsModel::Item::getTopLevelCategory() const
{
    StreamsModel::Item *item=const_cast<StreamsModel::Item *>(this);
    while (item->parent && item->parent->parent) {
        item=item->parent;
    }
    return item && item->isCategory() ? static_cast<CategoryItem *>(item) : nullptr;
}

void StreamsModel::CategoryItem::removeBookmarks()
{
    if (bookmarksName.isEmpty()) {
        return;
    }
    QString fileName=categoryBookmarksName(bookmarksName);
    if (QFile::exists(fileName)) {
        QFile::remove(fileName);
    }
}

void StreamsModel::CategoryItem::saveBookmarks()
{
    if (bookmarksName.isEmpty() || !supportsBookmarks) {
        return;
    }

    for (Item *child: children) {
        if (child->isCategory()) {
             CategoryItem *cat=static_cast<CategoryItem *>(child);
             if (cat->isBookmarks) {
                 if (cat->children.count()) {
                     QFile file(categoryBookmarksName(bookmarksName, true));
                     QtIOCompressor compressor(&file);
                     compressor.setStreamFormat(QtIOCompressor::GzipFormat);
                     if (compressor.open(QIODevice::WriteOnly)) {
                         QXmlStreamWriter doc(&compressor);
                         doc.writeStartDocument();
                         doc.writeStartElement("bookmarks");
                         doc.writeAttribute("version", "1.0");
                         doc.setAutoFormatting(false);
                         for (Item *i: cat->children) {
                             doc.writeStartElement("bookmark");
                             doc.writeAttribute("name", i->name);
                             doc.writeAttribute("url", i->fullUrl());
                             doc.writeEndElement();
                         }
                         doc.writeEndElement();
                         doc.writeEndElement();
                     }
                 }
                 break;
            }
        }
    }
}

QList<StreamsModel::Item *> StreamsModel::CategoryItem::loadBookmarks()
{
    QList<Item *> newItems;
    if (bookmarksName.isEmpty() || !supportsBookmarks) {
        return newItems;
    }

    QString fileName=categoryBookmarksName(bookmarksName);
    if (!QFile::exists(fileName)) {
        return newItems;
    }

    QFile file(fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (compressor.open(QIODevice::ReadOnly)) {
        QXmlStreamReader doc(&compressor);
        while (!doc.atEnd()) {
            doc.readNext();

            if (doc.isStartElement() && QLatin1String("bookmark")==doc.name()) {
                QString name=doc.attributes().value("name").toString();
                QString url=doc.attributes().value("url").toString();
                if (!name.isEmpty() && !url.isEmpty()) {
                    newItems.append(new CategoryItem(url, name, this));
                }
            }
        }
    }
    return newItems;
}

StreamsModel::CategoryItem * StreamsModel::CategoryItem::getBookmarksCategory()
{
    for (Item *i: children) {
        if (i->isCategory() && static_cast<CategoryItem *>(i)->isBookmarks) {
            return static_cast<CategoryItem *>(i);
        }
    }
    return nullptr;
}

StreamsModel::CategoryItem * StreamsModel::CategoryItem::createBookmarksCategory()
{
    CategoryItem *bookmarkCat = new CategoryItem(QString(), tr("Bookmarks"), this, MonoIcon::icon(FontAwesome::bookmark, Utils::monoIconColor()));
    bookmarkCat->state=CategoryItem::Fetched;
    bookmarkCat->isBookmarks=true;
    return bookmarkCat;
}

void StreamsModel::CategoryItem::removeCache()
{
    if (!cacheName.isEmpty()) {
        QString cache=categoryCacheName(cacheName);
        if (QFile::exists(cache)) {
            QFile::remove(cache);
        }
    }
}

void StreamsModel::CategoryItem::saveCache() const
{
    if (!cacheName.isEmpty()) {
        saveXml(categoryCacheName(cacheName, true));
    }
}

QList<StreamsModel::Item *> StreamsModel::CategoryItem::loadCache()
{
    if (!cacheName.isEmpty()) {
        QString cache=categoryCacheName(cacheName);
        if (!cache.isEmpty() && QFile::exists(cache)) {
            return loadXml(cache);
        }
    }

    return QList<Item *>();
}

QList<StreamsModel::Item *> StreamsModel::XmlCategoryItem::loadCache()
{
    QList<Item *> newItems;

    if (QFile::exists(cacheName)) {
        newItems=loadXml(cacheName);
        QString dir=Utils::getDir(cacheName);
        for (Item *i: newItems) {
            if (i->isCategory()) {
                StreamsModel::CategoryItem *cat=static_cast<StreamsModel::CategoryItem *>(i);
                QString name=cat->name;
                name=name.replace(Utils::constDirSep, "_");
                cat->icon=getExternalIcon(dir, QStringList() << name+".svg" << name+".png");
            }
        }
    }

    return newItems;
}

bool StreamsModel::CategoryItem::saveXml(const QString &fileName, bool format) const
{
    if (children.isEmpty()) {
        // No children, so remove XML...
        return !QFile::exists(fileName) || QFile::remove(fileName);
    }

    QFile file(fileName);

    if (fileName.endsWith(".xml")) {
        return file.open(QIODevice::WriteOnly) && saveXml(&file, format);
    } else {
        QtIOCompressor compressor(&file);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        return compressor.open(QIODevice::WriteOnly) && saveXml(&compressor, format);
    }
}

static void saveStream(QXmlStreamWriter &doc, const StreamsModel::Item *item)
{
    doc.writeStartElement("stream");
    doc.writeAttribute("name", item->name);
    doc.writeAttribute("url", item->fullUrl());
    doc.writeEndElement();
}

static void saveCategory(QXmlStreamWriter &doc, const StreamsModel::CategoryItem *cat)
{
    if (cat->isBookmarks) {
        return;
    }
    doc.writeStartElement("category");
    doc.writeAttribute("name", cat->name);
    if (cat->isAll) {
        doc.writeAttribute("isAll", "true");
    }
    for (const StreamsModel::Item *i: cat->children) {
        if (i->isCategory()) {
            saveCategory(doc, static_cast<const StreamsModel::CategoryItem *>(i));
        } else {
            saveStream(doc, static_cast<const StreamsModel::Item *>(i));
        }
    }
    doc.writeEndElement();
}

bool StreamsModel::CategoryItem::saveXml(QIODevice *dev, bool format) const
{
    QXmlStreamWriter doc(dev);
    doc.writeStartDocument();
    doc.writeStartElement("streams");
    doc.writeAttribute("version", "1.0");
    if (format) {
        doc.setAutoFormatting(true);
        doc.setAutoFormattingIndent(1);
    } else {
        doc.setAutoFormatting(false);
    }

    for (const Item *i: children) {
        if (i->isCategory()) {
            ::saveCategory(doc, static_cast<const CategoryItem *>(i));
        } else {
            ::saveStream(doc, i);
        }
    }
    doc.writeEndElement();
    doc.writeEndDocument();
    return true;
}

QList<StreamsModel::Item *> StreamsModel::CategoryItem::loadXml(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return QList<StreamsModel::Item *>();
    }
    // Check for gzip header...
    QByteArray header=file.read(2);
    bool isCompressed=((unsigned char)header[0])==0x1f && ((unsigned char)header[1])==0x8b;
    file.seek(0);

    QtIOCompressor compressor(&file);
    if (isCompressed) {
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (!compressor.open(QIODevice::ReadOnly)) {
            return QList<StreamsModel::Item *>();
        }
    }

    return loadXml(isCompressed ? (QIODevice *)&compressor : (QIODevice *)&file);
}

QList<StreamsModel::Item *> StreamsModel::CategoryItem::loadXml(QIODevice *dev)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    CategoryItem *currentCat=this;
    CategoryItem *prevCat=this;

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            if (QLatin1String("streams")==doc.name()) {
                if (QLatin1String("true")==doc.attributes().value("addCategoryName").toString()) {
                    addCatToModifiedName=true;
                }
            } else if (QLatin1String("stream")==doc.name()) {
                QString name=doc.attributes().value("name").toString();
                QString url=doc.attributes().value("url").toString();
                if (currentCat==this) {
                    newItems.append(new Item(url, name, currentCat));
                } else {
                    currentCat->children.append(new Item(url, name, currentCat));
                }
            } else if (QLatin1String("category")==doc.name()) {
                prevCat=currentCat;
                QString name=doc.attributes().value("name").toString();
                currentCat=new CategoryItem(QString(), name, prevCat);
                currentCat->state=CategoryItem::Fetched;
                currentCat->isAll=QLatin1String("true")==doc.attributes().value("isAll").toString();
                newItems.append(currentCat);
            }
        } else if (doc.isEndElement() && QLatin1String("category")==doc.name()) {
            currentCat=prevCat;
        }
    }

    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::FavouritesCategoryItem::loadXml(QIODevice *dev)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    QSet<QString> existingUrls;
    QSet<QString> existingNames;

    for (Item *i: children) {
        existingUrls.insert(i->fullUrl());
        existingNames.insert(i->name);
    }

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement() && QLatin1String("stream")==doc.name()) {
            QString name=doc.attributes().value("name").toString();
            QString origName=name;
            QString url=doc.attributes().value("url").toString();

            if (!url.isEmpty() && !name.isEmpty() && !existingUrls.contains(url)) {
                int i=1;
                for (; i<100 && existingNames.contains(name); ++i) {
                    name=origName+QLatin1String(" (")+QString::number(i)+QLatin1Char(')');
                }

                if (i<100) {
                    existingNames.insert(name);
                    existingUrls.insert(url);
                    newItems.append(new Item(url, name, this));
                }
            }
        }
    }

    return newItems;
}

void StreamsModel::IceCastCategoryItem::addHeaders(QNetworkRequest &req)
{
    req.setRawHeader("Accept-Encoding", "gzip");
}

void StreamsModel::ShoutCastCategoryItem::addHeaders(QNetworkRequest &req)
{
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
}

NetworkJob * StreamsModel::ShoutCastCategoryItem::fetchSecondardyUrl()
{
    if (!url.startsWith(constShoutCastUrl)) {
        // Get stations...
        QUrl url(QLatin1String("http://")+constShoutCastHost+QLatin1String("/legacy/genresearch"));
        QUrlQuery query;
        ApiKeys::self()->addKey(query, ApiKeys::ShoutCast);
        query.addQueryItem("genre", name);
        url.setQuery(query);

        QNetworkRequest req(url);
        addHeaders(req);
        NetworkJob *job=NetworkAccessManager::self()->get(req);
        job->setProperty(constOrigUrlProperty, url.toString());
        return job;
    }
    return nullptr;
}

void StreamsModel::DiCategoryItem::addHeaders(QNetworkRequest &req)
{
    DigitallyImported::self()->addAuthHeader(req);
}

StreamsModel::StreamsModel(QObject *parent)
    : ActionModel(parent)
    , root(new CategoryItem(QString(), "root"))
{
    QColor col = Utils::monoIconColor();
    icn=MonoIcon::icon(":radio.svg", col);
    tuneIn=new CategoryItem(constRadioTimeUrl+QLatin1String("?locale=")+QLocale::system().name(), tr("TuneIn"), root, MonoIcon::icon(":tunein.svg", col), QString(), "tunein");
    tuneIn->supportsBookmarks=true;
    root->children.append(tuneIn);
    root->children.append(new IceCastCategoryItem(constIceCastUrl, tr("IceCast"), root, MonoIcon::icon(FontAwesome::cube, col), "icecast"));
    shoutCast=new ShoutCastCategoryItem(constShoutCastUrl, tr("ShoutCast"), root, MonoIcon::icon(":shoutcast.svg", col));
    shoutCast->configName="shoutcast";
    root->children.append(shoutCast);
    dirble=new DirbleCategoryItem(constDirbleUrl, tr("Dirble"), root, MonoIcon::icon(":station.svg", col));
    dirble->configName="dirble";
    root->children.append(dirble);
    favourites=new FavouritesCategoryItem(constFavouritesUrl, tr("Favorites"), root, MonoIcon::icon(FontAwesome::heart, MonoIcon::constRed));
    root->children.append(favourites);
    loadInstalledProviders();
    addBookmarkAction = new Action(Icons::self()->addBookmarkIcon, tr("Bookmark Category"), this);
    addToFavouritesAction = new Action(favouritesIcon(), tr("Add Stream To Favorites"), this);
    configureDiAction = new Action(Icons::self()->configureIcon, tr("Configure Digitally Imported"), this);
    reloadAction = new Action(Icons::self()->reloadIcon, tr("Reload"), this);

    QSet<QString> hidden=Settings::self()->hiddenStreamCategories().toSet();
    for (Item *c: root->children) {
        if (c!=favourites) {
            CategoryItem *cat=static_cast<CategoryItem *>(c);
            if (hidden.contains(cat->configName)) {
                hiddenCategories.append(c);
                root->children.removeAll(c);
            }
        }
    }

    connect(MPDConnection::self(), SIGNAL(savedStream(QString,QString)), SLOT(savedFavouriteStream(QString,QString)));
    connect(MPDConnection::self(), SIGNAL(removedStreams(QList<quint32>)), SLOT(removedFavouriteStreams(QList<quint32>)));
    connect(MPDConnection::self(), SIGNAL(streamList(QList<Stream>)), SLOT(favouriteStreams(QList<Stream>)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionState(bool)));
    connect(this, SIGNAL(listFavouriteStreams()), MPDConnection::self(), SLOT(listStreams()));
    connect(this, SIGNAL(saveFavouriteStream(QString,QString)), MPDConnection::self(), SLOT(saveStream(QString,QString)));
    connect(this, SIGNAL(removeFavouriteStreams(QList<quint32>)), MPDConnection::self(), SLOT(removeStreams(QList<quint32>)));
    connect(this, SIGNAL(editFavouriteStream(QString,QString,quint32)), MPDConnection::self(), SLOT(editStream(QString,QString,quint32)));
}

StreamsModel::~StreamsModel()
{
    delete root;
}

QString StreamsModel::name() const
{
    return QLatin1String("streams");
}

QString StreamsModel::title() const
{
    return tr("Streams");
}

QString StreamsModel::descr() const
{
    return tr("Radio stations");
}

QModelIndex StreamsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const CategoryItem * p = parent.isValid() ? static_cast<CategoryItem *>(parent.internalPointer()) : root;
    const Item * c = row<p->children.count() ? p->children.at(row) : nullptr;
    return c ? createIndex(row, column, (void *)c) : QModelIndex();
}

QModelIndex StreamsModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    Item * parent = toItem(index)->parent;

    if (!parent || parent == root || !parent->parent) {
        return QModelIndex();
    }

    return createIndex(static_cast<const CategoryItem *>(parent->parent)->children.indexOf(parent), 0, parent);
}

QVariant StreamsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int StreamsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        Item *item = toItem(parent);
        return item->isCategory() ? static_cast<CategoryItem *>(item)->children.count() : 0;
    }

    return root->children.count();
}

int StreamsModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant StreamsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        switch (role) {
        case Cantata::Role_TitleText:
            return title();
        case Cantata::Role_SubText:
            return descr();
        case Qt::DecorationRole:
            return icon();
        }
        return QVariant();
    }

    const Item *item = toItem(index);

    switch (role) {
    case Qt::DecorationRole:
        if (item->isCategory()) {
            const CategoryItem *cat=static_cast<const CategoryItem *>(item);
            return cat->icon.isNull() ? Icons::self()->streamCategoryIcon : cat->icon;
        } else {
            return Icons::self()->streamListIcon;
        }
    case Qt::DisplayRole:
        return item->name;
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        return item->isCategory() ? item->name : (item->name+QLatin1String("<br><small><i>")+item->fullUrl()+QLatin1String("</i></small>"));
    case Cantata::Role_SubText:
        if (item->isCategory()) {
            const CategoryItem *cat=static_cast<const CategoryItem *>(item);
            switch (cat->state) {
            case CategoryItem::Initial:
                return tr("Not Loaded");
            case CategoryItem::Fetching:
                return tr("Loading...");
            default:
                return tr("%n Entry(s)", "", cat->children.count());
            }
        }
        break;
    case Cantata::Role_Actions: {
        QList<Action *> actions;
        if (item->isCategory()){
            const CategoryItem *cat=static_cast<const CategoryItem *>(item);
            if (cat->isDi()) {
                actions << configureDiAction;
            }
            if (cat->canReload()) {
                actions << reloadAction;
            }
            if (cat->canBookmark) {
                actions << addBookmarkAction;
            }
        } else {
            actions << StdActions::self()->replacePlayQueueAction;
            if (item->parent!=favourites) {
                actions << addToFavouritesAction;
            }
        }
        if (!actions.isEmpty()) {
            QVariant v;
            v.setValue<QList<Action *> >(actions);
            return v;
        }
        break;
    }
    default:
        break;
    }
    return ActionModel::data(index, role);
}

Qt::ItemFlags StreamsModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        if (toItem(index)->isCategory()) {
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        } else {
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
        }
    }
    return Qt::NoItemFlags;
}

bool StreamsModel::hasChildren(const QModelIndex &index) const
{
    return index.isValid() ? toItem(index)->isCategory() : true;
}

bool StreamsModel::canFetchMore(const QModelIndex &index) const
{
    if (index.isValid()) {
        Item *item = toItem(index);
        return item->isCategory() && CategoryItem::Initial==static_cast<CategoryItem *>(item)->state && !item->url.isEmpty() &&
               !static_cast<CategoryItem *>(item)->isFavourites();
    } else {
        return false;
    }
}

void StreamsModel::fetchMore(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    Item *item = toItem(index);
    if (item->isCategory() && !item->url.isEmpty()) {
        CategoryItem *cat=static_cast<CategoryItem *>(item);
        if (!cat->isFavourites() && !loadCache(cat)) {
            QNetworkRequest req;
            if (cat->isDi()) {
                req=QNetworkRequest(constDiChannelListUrl.arg(cat->fullUrl().split(".").at(1)));
            } else {
                req=QNetworkRequest(cat->fullUrl());
            }
            cat->addHeaders(req);
            NetworkJob *job=NetworkAccessManager::self()->get(req);
            job->setProperty(constOrigUrlProperty, cat->fullUrl());
            if (jobs.isEmpty()) {
                emit loading();
            }
            jobs.insert(job, cat);
            connect(job, SIGNAL(finished()), SLOT(jobFinished()));
            cat->state=CategoryItem::Fetching;

            job=cat->fetchSecondardyUrl();
            if (job) {
                jobs.insert(job, cat);
                connect(job, SIGNAL(finished()), SLOT(jobFinished()));
            }
        }
        emit dataChanged(index, index);
    }
}

void StreamsModel::reload(const QModelIndex &index)
{
    Item *item = toItem(index);
    if (!item->isCategory()) {
        return;
    }
    CategoryItem *cat=static_cast<CategoryItem *>(item);
    if (!cat->children.isEmpty()) {
        cat->removeCache();
        beginRemoveRows(index, 0, cat->children.count()-1);
        qDeleteAll(cat->children);
        cat->children.clear();
        endRemoveRows();
    }

    fetchMore(index);
}

bool StreamsModel::exportFavourites(const QString &fileName)
{
    return favourites->saveXml(fileName, true);
}

void StreamsModel::importIntoFavourites(const QString &fileName)
{
    QList<Item *> newItems=favourites->CategoryItem::loadXml(fileName);
    if (!newItems.isEmpty()) {
        for (Item *i: newItems) {
            emit saveFavouriteStream(i->fullUrl(), i->name);
        }
        qDeleteAll(newItems);
    }
}

void StreamsModel::removeFromFavourites(const QModelIndexList &indexes)
{
    QList<quint32> rows;
    for (const QModelIndex &idx: indexes) {
        rows.append(idx.row());
    }

    if (!rows.isEmpty()) {
        emit removeFavouriteStreams(rows);
    }
}

bool StreamsModel::addToFavourites(const QString &url, const QString &name)
{
    QSet<QString> existingNames;

    for (Item *i: favourites->children) {
        if (i->fullUrl()==url) {
            return false;
        }
        existingNames.insert(i->name);
    }

    QString n=name;
    int i=1;
    for (; i<100 && existingNames.contains(n); ++i) {
        n=name+QLatin1String("_")+QString::number(i);
    }

    if (i<100) {
        emit saveFavouriteStream(url, name);
        return true;
    }
    return false;
}

QString StreamsModel::favouritesNameForUrl(const QString &u)
{
    for (Item *i: favourites->children) {
        if (i->fullUrl()==u) {
            return i->name;
        }
    }
    return QString();
}

bool StreamsModel::nameExistsInFavourites(const QString &n)
{
    for (Item *i: favourites->children) {
        if (i->name==n) {
            return true;
        }
    }
    return false;
}

void StreamsModel::updateFavouriteStream(const QString &url, const QString &name, const QModelIndex &idx)
{
    emit editFavouriteStream(url, name, idx.row());
}

bool StreamsModel::addBookmark(const QString &url, const QString &name, CategoryItem *bookmarkParentCat)
{
    if (!bookmarkParentCat) {
        bookmarkParentCat=tuneIn;
    }

    if (bookmarkParentCat && !url.isEmpty() && !name.isEmpty()) {
        CategoryItem *bookmarkCat=bookmarkParentCat->getBookmarksCategory();
        if (!bookmarkCat) {
            QModelIndex index=createIndex(bookmarkParentCat->parent->children.indexOf(bookmarkParentCat), 0, (void *)bookmarkParentCat);
            beginInsertRows(index, bookmarkParentCat->children.count(), bookmarkParentCat->children.count());
            bookmarkCat=bookmarkParentCat->createBookmarksCategory();
            bookmarkParentCat->children.append(bookmarkCat);
            endInsertRows();
        }

        for (Item *i: bookmarkCat->children) {
            if (i->fullUrl()==url) {
                return false;
            }
        }

        QModelIndex index=createIndex(bookmarkCat->parent->children.indexOf(bookmarkCat), 0, (void *)bookmarkCat);
        beginInsertRows(index, bookmarkCat->children.count(), bookmarkCat->children.count());
        bookmarkCat->children.append(new CategoryItem(url, name, bookmarkCat));
        endInsertRows();
        bookmarkParentCat->saveBookmarks();
        return true;
    }
    return false;
}

void StreamsModel::removeBookmark(const QModelIndex &index)
{
    Item *item=toItem(index);

    if (item->isCategory() && item->parent && item->parent->isBookmarks) {
        CategoryItem *bookmarkCat=item->parent; // 'Bookmarks'
        CategoryItem *cat=bookmarkCat->parent; // e.g. 'TuneIn'
        if (1==bookmarkCat->children.count()) { // Only 1 bookark, so remove 'Bookmarks' folder...
            int pos=cat->children.indexOf(bookmarkCat);
            QModelIndex index=createIndex(cat->parent->children.indexOf(cat), 0, (void *)cat);
            beginRemoveRows(index, pos, pos);
            delete cat->children.takeAt(pos);
            endRemoveRows();
            cat->removeBookmarks();
        } else if (!bookmarkCat->children.isEmpty()) { // More than 1 bookmark...
            int pos=bookmarkCat->children.indexOf(item);
            QModelIndex index=createIndex(bookmarkCat->parent->children.indexOf(bookmarkCat), 0, (void *)bookmarkCat);
            beginRemoveRows(index, pos, pos);
            delete bookmarkCat->children.takeAt(pos);
            endRemoveRows();
            cat->saveBookmarks();
        }
    }
}

void StreamsModel::removeAllBookmarks(const QModelIndex &index)
{
    Item *item=toItem(index);

    if (item->isCategory() && static_cast<CategoryItem *>(item)->isBookmarks) {
        CategoryItem *cat=item->parent; // e.g. 'TuneIn'
        int pos=cat->children.indexOf(item);
        QModelIndex index=createIndex(cat->parent->children.indexOf(cat), 0, (void *)cat);
        beginRemoveRows(index, pos, pos);
        delete cat->children.takeAt(pos);
        endRemoveRows();
        cat->removeBookmarks();
    }
}

QModelIndex StreamsModel::favouritesIndex() const
{
    return createIndex(root->children.indexOf(favourites), 0, (void *)favourites);
}

static QString addDiHash(const StreamsModel::Item *item)
{
    return ( (item->parent && item->parent->isDi()) || DigitallyImported::self()->isDiUrl(item->fullUrl()) )
            ? DigitallyImported::self()->modifyUrl(item->fullUrl()) : item->fullUrl();
}

QStringList StreamsModel::filenames(const QModelIndexList &indexes, bool addPrefix) const
{
    QStringList fnames;
    for (const QModelIndex &index: indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());
        if (!item->isCategory()) {
            fnames << modifyUrl(addDiHash(item), addPrefix, addPrefix ? item->modifiedName() : item->name);
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

QStringList StreamsModel::mimeTypes() const
{
    QStringList types;
    types << PlayQueueModel::constFileNameMimeType;
    return types;
}

void StreamsModel::save()
{
    QStringList disabled;
    for (Item *i: hiddenCategories) {
        disabled.append(static_cast<CategoryItem *>(i)->configName);
    }
    disabled.sort();
    Settings::self()->saveHiddenStreamCategories(disabled);
}

QList<StreamsModel::Category> StreamsModel::getCategories() const
{
    QList<StreamsModel::Category> categories;
    for (Item *i: hiddenCategories) {
        categories.append(Category(i->name, static_cast<CategoryItem *>(i)->icon, static_cast<CategoryItem *>(i)->configName,
                                   static_cast<CategoryItem *>(i)->isBuiltIn(), true, static_cast<CategoryItem *>(i)->isDi()));
    }
    for (Item *i: root->children) {
        if (i!=favourites) {
            categories.append(Category(i->name, static_cast<CategoryItem *>(i)->icon, static_cast<CategoryItem *>(i)->configName,
                                       static_cast<CategoryItem *>(i)->isBuiltIn(), false, static_cast<CategoryItem *>(i)->isDi()));
        }
    }
    return categories;
}

void StreamsModel::setHiddenCategories(const QSet<QString> &cats)
{
    bool changed=false;
    for (Item *i: hiddenCategories) {
        if (!cats.contains(static_cast<CategoryItem *>(i)->configName)) {
            beginInsertRows(QModelIndex(), root->children.count(), root->children.count());
            root->children.append(i);
            hiddenCategories.removeAll(i);
            endInsertRows();
            changed=true;
        }
    }

    for (Item *i: root->children) {
        if (cats.contains(static_cast<CategoryItem *>(i)->configName)) {
            int row=root->children.indexOf(i);
            if (row>0) {
                beginRemoveRows(QModelIndex(), row, row);
                hiddenCategories.append(root->children.takeAt(row));
                endRemoveRows();
                changed=true;
            }
        }
    }
    if (changed) {
        emit categoriesChanged();
    }
}

bool StreamsModel::loadCache(CategoryItem *cat)
{
    QList<Item *> newItems=cat->loadCache();
    if (!newItems.isEmpty()) {
        QModelIndex index=createIndex(cat->parent->children.indexOf(cat), 0, (void *)cat);
        beginInsertRows(index, cat->children.count(), (cat->children.count()+newItems.count())-1);
        cat->children+=newItems;
        endInsertRows();
        cat->state=CategoryItem::Fetched;
        emit dataChanged(index, index);
        return true;
    }

    return false;
}

void StreamsModel::jobFinished()
{
    NetworkJob *job=dynamic_cast<NetworkJob *>(sender());

    if (!job) {
        return;
    }

    job->deleteLater();
    if (jobs.contains(job)) {
        CategoryItem *cat=jobs[job];
        if (!cat) {
            return;
        }
        jobs.remove(job);

        // ShoutCast and Dirble have two jobs when listing a category - child categories and station list.
        // So, only set as fetched if both are finished.
        bool haveOtherJob=false;
        for (CategoryItem *c: jobs.values()) {
            if (c==cat) {
                haveOtherJob=true;
                break;
            }
        }

        if (!haveOtherJob) {
            cat->state=CategoryItem::Fetched;
        }

        QModelIndex index=createIndex(cat->parent->children.indexOf(cat), 0, (void *)cat);
        QString url=job->origUrl().toString();

        if (job->ok()) {
            QList<Item *> newItems;
            if (cat!=favourites && QLatin1String("http")==job->url().scheme()) {
                if (constRadioTimeHost==job->origUrl().host()) {
                    newItems=parseRadioTimeResponse(job->actualJob(), cat);
                } else if (constIceCastUrl==url) {
                    newItems=parseIceCastResponse(job->actualJob(), cat);
                } else if (cat->isSoma()) {
                    newItems=parseSomaFmResponse(job->actualJob(), cat);
                } else if (constDiChannelListHost==job->origUrl().host()) {
                    newItems=parseDigitallyImportedResponse(job->actualJob(), cat);
                } else if (constShoutCastHost==job->origUrl().host()) {
                    newItems=parseShoutCastResponse(job->actualJob(), cat);
                } else if (constDirbleHost==job->origUrl().host()) {
                    newItems=parseDirbleResponse(job->actualJob(), cat, job->property(constOrigUrlProperty).toString());
                } else if (cat->isListenLive()) {
                    newItems=parseListenLiveResponse(job->actualJob(), cat);
                }
            }

            if (cat->parent==root && cat->supportsBookmarks) {
                QList<Item *> bookmarks=cat->loadBookmarks();
                if (bookmarks.count()) {
                    CategoryItem *bookmarksCat=cat->getBookmarksCategory();

                    if (bookmarksCat) {
                        QList<Item *> newBookmarks;
                        for (Item *bm: bookmarks) {
                            for (Item *ex: bookmarksCat->children) {
                                if (ex->fullUrl()==bm->fullUrl()) {
                                    delete bm;
                                    bm=nullptr;
                                    break;
                                }
                            }
                            if (bm) {
                                newBookmarks.append(bm);
                                bm->parent=bookmarksCat;
                            }
                        }
                        if (newBookmarks.count()) {
                            QModelIndex index=createIndex(bookmarksCat->parent->children.indexOf(bookmarksCat), 0, (void *)bookmarksCat);
                            beginInsertRows(index, bookmarksCat->children.count(), (bookmarksCat->children.count()+newBookmarks.count())-1);
                            bookmarksCat->children+=newBookmarks;
                            endInsertRows();
                        }
                    } else {
                        bookmarksCat=cat->createBookmarksCategory();
                        for (Item *i: bookmarks) {
                            i->parent=bookmarksCat;
                        }
                        bookmarksCat->children=bookmarks;
                        newItems.append(bookmarksCat);
                    }
                }
            }
            
            if (!newItems.isEmpty()) {
                beginInsertRows(index, cat->children.count(), (cat->children.count()+newItems.count())-1);
                cat->children+=newItems;
                endInsertRows();
                if (cat!=favourites) {
                    cat->saveCache();
                }
            }
        } else {
            if (constShoutCastHost==job->origUrl().host()) {
                ApiKeys::self()->isLimitReached(job->actualJob(), ApiKeys::ShoutCast);
            } else if (constDirbleHost==job->origUrl().host()) {
                ApiKeys::self()->isLimitReached(job->actualJob(), ApiKeys::Dirble);
            }
        }
        emit dataChanged(index, index);
        if (jobs.isEmpty()) {
            emit loaded();
        }
    }
}

void StreamsModel::savedFavouriteStream(const QString &url, const QString &name)
{
    if (favouritesNameForUrl(url).isEmpty()) {
        beginInsertRows(favouritesIndex(), favourites->children.count(), favourites->children.count());
        favourites->children.append(new Item(url, name, favourites));
        endInsertRows();
        emit addedToFavourites(name);
    } else {
        emit listFavouriteStreams();
    }
}

void StreamsModel::removedFavouriteStreams(const QList<quint32> &removed)
{
    if (removed.isEmpty()) {
        return;
    }

    quint32 adjust=0;
    QModelIndex parent=favouritesIndex();
    QList<quint32>::ConstIterator it=removed.constBegin();
    QList<quint32>::ConstIterator end=removed.constEnd();
    while(it!=end) {
        quint32 rowBegin=*it;
        quint32 rowEnd=*it;
        QList<quint32>::ConstIterator next=it+1;
        while(next!=end) {
            if (*next!=(rowEnd+1)) {
                break;
            } else {
                it=next;
                rowEnd=*next;
                next++;
            }
        }
        beginRemoveRows(parent, rowBegin-adjust, rowEnd-adjust);
        for (quint32 i=rowBegin; i<=rowEnd; ++i) {
            delete favourites->children.takeAt(rowBegin-adjust);
        }
        adjust+=(rowEnd-rowBegin)+1;
        endRemoveRows();
        it++;
    }
    emit dataChanged(parent, parent);
}

static StreamsModel::Item * getStream(StreamsModel::FavouritesCategoryItem *fav, const Stream &stream, int offset)
{
    int i=0;
    for (StreamsModel::Item *s: fav->children) {
        if (++i<offset) {
            continue;
        }
        if (s->name==stream.name && s->fullUrl()==stream.url) {
            return s;
        }
    }

    return nullptr;
}

void StreamsModel::favouriteStreams(const QList<Stream> &streams)
{
    QModelIndex idx=favouritesIndex();
    if (favourites->children.isEmpty()) {
        if (streams.isEmpty()) {
            if (CategoryItem::Fetched!=favourites->state) {
                favourites->state=CategoryItem::Fetched;
                emit dataChanged(idx, idx);
            }
            return;
        }
        beginInsertRows(idx, 0, streams.count()-1);
        for (const Stream &s: streams) {
            favourites->children.append(new Item(s.url, s.name, favourites));
        }
        endInsertRows();
    } else if (streams.isEmpty()) {
        beginRemoveRows(idx, 0, favourites->children.count()-1);
        qDeleteAll(favourites->children);
        favourites->children.clear();
        endRemoveRows();
    } else {
        for (qint32 i=0; i<streams.count(); ++i) {
            Stream s=streams.at(i);
            Item *si=i<favourites->children.count() ? favourites->children.at(i) : nullptr;
            if (!si || si->name!=s.name || si->fullUrl()!=s.url) {
                si=i<favourites->children.count() ? getStream(favourites, s, i) : nullptr;
                if (!si) {
                    beginInsertRows(idx, i, i);
                    favourites->children.insert(i, new Item(s.url, s.name, favourites));
                    endInsertRows();
                } else {
                    int existing=favourites->children.indexOf(si);
                    beginMoveRows(idx, existing, existing, idx, i>existing ? i+1 : i);
                    Item *si=favourites->children.takeAt(existing);
                    favourites->children.insert(i, si);
                    endMoveRows();
                }
            }
        }
        if (favourites->children.count()>streams.count()) {
            int toRemove=favourites->children.count()-streams.count();
            beginRemoveRows(idx, favourites->children.count()-toRemove, favourites->children.count()-1);
            for (int i=0; i<toRemove; ++i) {
                delete favourites->children.takeLast();
            }
            endRemoveRows();
        }
    }

    if (CategoryItem::Fetched!=favourites->state) {
        favourites->state=CategoryItem::Fetched;
        emit dataChanged(idx, idx);
        emit favouritesLoaded();
    }
}

void StreamsModel::mpdConnectionState(bool c)
{
    if (c) {
        emit listFavouriteStreams();
    }
}

QList<StreamsModel::Item *> StreamsModel::parseRadioTimeResponse(QIODevice *dev, CategoryItem *cat, bool parseSubText)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("outline")==doc.name()) {
            Item *item = parseRadioTimeEntry(doc, cat, parseSubText);
            if (item) {
                newItems.append(item);
            }
        }
    }

    return newItems;
}

static QStringList fixGenres(const QString &genre)
{
    QStringList allGenres=Song::capitalize(genre).split(' ', QString::SkipEmptyParts);
    QStringList fixed;
    for (const QString &genre: allGenres) {
        if (genre.length() < 2 ||
            genre.contains("ÃÂ") ||  // Broken unicode.
            genre.contains(QRegExp("^#x[0-9a-f][0-9a-f]"))) { // Broken XML entities.
            continue;
        }
        // Convert 80 -> 80s.
        if (genre.contains(QRegExp("^[0-9]0$"))) {
            fixed << genre + 's';
        } else {
            fixed << genre;
        }
    }
    if (fixed.isEmpty()) {
        fixed << QObject::tr("Other");
    }
    return fixed;
}

static void trimGenres(QMap<QString, QList<StreamsModel::Item *> > &genres)
{
    QString other=QObject::tr("Other");
    QSet<QString> genreSet = genres.keys().toSet();
    for (const QString &genre: genreSet) {
        if (other!=genre && genres[genre].count() < 2) {
            genres[other]+=genres[genre];
            genres.remove(genre);
        }
    }
}

QList<StreamsModel::Item *> StreamsModel::parseIceCastResponse(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QtIOCompressor compressor(dev);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    compressor.open(QIODevice::ReadOnly);
    QXmlStreamReader doc(&compressor);
    QSet<QString> names;
    QMap<QString, QList<Item *> > genres;
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("entry")==doc.name()) {
            QString name;
            QString url;
            QStringList stationGenres;
            while (!doc.atEnd()) {
                doc.readNext();

                if (QXmlStreamReader::StartElement==doc.tokenType()) {
                    QStringRef elem = doc.name();

                    if (QLatin1String("server_name")==elem) {
                        name=doc.readElementText().trimmed();
                    } else if (QLatin1String("listen_url")==elem) {
                        url=doc.readElementText().trimmed();
                    } else if (QLatin1String("genre")==elem) {
                        stationGenres=fixGenres(doc.readElementText().trimmed());
                    }
                } else if (doc.isEndElement() && QLatin1String("entry")==doc.name()) {
                    break;
                }
            }

            if (!name.isEmpty() && !url.isEmpty() && !names.contains(name)) {
                names.insert(name);
                for (const QString &g: stationGenres) {
                    genres[g].append(new Item(url, name, cat));
                }
            }
        }
    }
    trimGenres(genres);
    QMap<QString, QList<Item *> >::ConstIterator it(genres.constBegin());
    QMap<QString, QList<Item *> >::ConstIterator end(genres.constEnd());

    for (; it!=end; ++it) {
        CategoryItem *genre=new CategoryItem(QString(), it.key(), cat);
        genre->state=CategoryItem::Fetched;
        for (Item *i: it.value()) {
            i->parent=genre;
            genre->children.append(i);
        }
        newItems.append(genre);
    }

    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseSomaFmResponse(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("channel")==doc.name()) {
            Item *item = parseSomaFmEntry(doc, cat);
            if (item) {
                newItems.append(item);
            }
        }
    }
    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseDigitallyImportedResponse(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QVariantMap data=QJsonDocument::fromJson(dev->readAll()).toVariant().toMap();
    QString listenHost=QLatin1String("listen.")+QUrl(cat->fullUrl()).host().remove("www.");

    if (data.contains("channel_filters")) {
        QVariantList filters = data["channel_filters"].toList();

        for (const QVariant &filter: filters) {
            // Find the filter called "All"
            QVariantMap filterMap = filter.toMap();
            if (filterMap.value("name", QString()).toString() != "All") {
                continue;
            }

            // Add all its stations to the result
            QVariantList channels = filterMap.value("channels", QVariantList()).toList();
            for (const QVariant &channel: channels) {
                QVariantMap channelMap = channel.toMap();
                QString url=constDiStdUrl.arg(listenHost).arg(channelMap.value("key").toString());
                newItems.append(new Item(url, channelMap.value("name").toString(), cat));
            }

            break;
        }
    }

    return newItems;
}

struct ListenLiveStream {
    enum Format {
        Unknown,
        WMA,
        OGG,
        MP3,
        AAC
    };

    ListenLiveStream() : format(Unknown), bitrate(0) { }
    bool operator<(const ListenLiveStream &o) const { return weight()>o.weight(); }

    int weight() const { return ((bitrate&0xff)<<4)+(format&0x0f); }

    void setFormat(const QString &f) {
        if (QLatin1String("mp3")==f.toLower()) {
            format=MP3;
        } else if (QLatin1String("aacplus")==f.toLower()) {
            format=AAC;
        } else if (QLatin1String("ogg vorbis")==f.toLower()) {
            format=OGG;
        } else if (QLatin1String("windows media")==f.toLower()) {
            format=WMA;
        } else {
            format=Unknown;
        }
    }

    QString url;
    Format format;
    unsigned int bitrate;
};

struct ListenLiveStationEntry {
    ListenLiveStationEntry() { clear(); }
    void clear() { name=location=QString(); streams.clear(); }
    QString name;
    QString location;
    QList<ListenLiveStream> streams;
};

static QString getString(QString &str, const QString &start, const QString &end)
{
    QString rv;
    int b=str.indexOf(start);
    int e=-1==b ? -1 : str.indexOf(end, b+start.length());
    if (-1!=e) {
        rv=str.mid(b+start.length(), e-(b+start.length())).trimmed();
        str=str.mid(e+end.length());
    }
    return rv;
}

QList<StreamsModel::Item *> StreamsModel::parseListenLiveResponse(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;

    if (dev) {
        ListenLiveStationEntry entry;

        while (!dev->atEnd()) {
            QString line=QString::fromUtf8(dev->readLine()).trimmed().replace("> <", "><").replace("<td><b><a href", "<td><a href")
                                                  .replace("</b></a></b>", "</b></a>").replace("<br />", "<br/>")
                                                  .replace("</a> ,", "</a>,");
            if ("<tr>"==line) {
                entry.clear();
            } else if (line.startsWith("<td><a href=")) {
                if (entry.name.isEmpty()) {
                    entry.name=getString(line, "<b>", "</b>");
                    QString extra=getString(line, "</a>", "</td>");
                    if (!extra.isEmpty()) {
                        entry.name+=" "+extra;
                    }
                } else {
                    // Station URLs...
                    QString url;
                    QString bitrate;
                    int idx=0;
                    do {
                        url=getString(line, "href=\"", "\"");
                        bitrate=getString(line, ">", " Kbps");
                        bool sameFormatAsLast=line.startsWith("</a>,");
                        if (!url.isEmpty() && !bitrate.isEmpty() && !url.startsWith(QLatin1String("javascript")) && idx<entry.streams.count()) {
                            if (sameFormatAsLast && 0!=idx) {
                                ListenLiveStream stream;
                                stream.format=entry.streams[idx-1].format;
                                entry.streams.insert(idx, stream);
                            }
                            entry.streams[idx].url=url;
                            entry.streams[idx].bitrate=bitrate.toUInt();
                            idx++;
                        }
                    } while (!url.isEmpty() && !bitrate.isEmpty());
                }
            } else if (line.startsWith("<td><img src")) {
                // Station formats...
                QString format;
                do {
                    format=getString(line, "alt=\"", "\"");
                    if (!format.isEmpty()) {
                        ListenLiveStream stream;
                        stream.setFormat(format);
                        entry.streams.append(stream);
                    }
                } while (!format.isEmpty());
            } else if (line.startsWith("<td>")) {
                if (entry.location.isEmpty()) {
                    entry.location=getString(line, "<td>", "</td>");
                }
            } else if ("</tr>"==line) {
                if (entry.streams.count()) {
                    std::sort(entry.streams.begin(), entry.streams.end());
                    QString name;
                    QString url=entry.streams.at(0).url;

                    if (QLatin1String("National")==entry.location || entry.name.endsWith(QLatin1Char('(')+entry.location+QLatin1Char(')'))) {
                        name=entry.name;
                    } else if (entry.name.endsWith(QLatin1Char(')'))) {
                        name=entry.name.left(entry.name.length()-1)+QLatin1String(", ")+entry.location+QLatin1Char(')');
                    } else {
                        name=entry.name+QLatin1String(" (")+entry.location+QLatin1Char(')');
                    }

                    if (!name.isEmpty()) {
                        newItems.append(new Item(url, name, cat));
                    }
                }
            }
        }
    }

    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseShoutCastSearchResponse(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("stationlist")==doc.name()) {
            newItems+=parseShoutCastStations(doc, cat);
        }
    }
    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseShoutCastResponse(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("genrelist")==doc.name()) {
            newItems+=parseShoutCastLinks(doc, cat);
        } else if (doc.isStartElement() && QLatin1String("stationlist")==doc.name()) {
            newItems+=parseShoutCastStations(doc, cat);
        }
    }

    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseShoutCastLinks(QXmlStreamReader &doc, CategoryItem *cat)
{
    QList<Item *> newItems;
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("genre")==doc.name()) {
            newItems.append(new ShoutCastCategoryItem(ApiKeys::self()->addKey(QLatin1String("http://")+constShoutCastHost+QLatin1String("/genre/secondary?parentid=")+
                                                                              doc.attributes().value("id").toString()+QLatin1String("&f=xml"), ApiKeys::ShoutCast),
                                                      doc.attributes().value("name").toString(), cat));
        } else if (doc.isEndElement() && QLatin1String("genrelist")==doc.name()) {
            return newItems;
        }
    }

    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseShoutCastStations(QXmlStreamReader &doc, CategoryItem *cat)
{
    QList<Item *> newItems;
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("station")==doc.name()) {
            QString name=doc.attributes().value("name").toString().trimmed().simplified();
            if (!name.isEmpty()) {
                newItems.append(new Item(QLatin1String("http://yp.shoutcast.com/sbin/tunein-station.pls?id=")+
                                         doc.attributes().value("id").toString(),
                                         doc.attributes().value("name").toString(), cat));
            }
        } else if (doc.isEndElement() && QLatin1String("stationlist")==doc.name()) {
            return newItems;
        }
    }

    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseDirbleResponse(QIODevice *dev, CategoryItem *cat, const QString &origUrl)
{
    if (origUrl.contains("/v2/category/") && origUrl.contains("/stations")) {
        return parseDirbleStations(dev, cat);
    }
    QList<Item *> newItems;
    QVariantList data=QJsonDocument::fromJson(dev->readAll()).toVariant().toList();

    // Get categories...::f
    if (!data.isEmpty()) {
        for (const QVariant &d: data) {
            QVariantMap map = d.toMap();
            newItems.append(new DirbleCategoryItem(ApiKeys::self()->addKey(QLatin1String("http://")+constDirbleHost+QLatin1String("/v2/category/")+map["id"].toString()+
                                                                           QLatin1String("/stations"), ApiKeys::Dirble),
                                                   map["title"].toString(), cat));
        }
    }
    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseDirbleStations(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QVariantList data=QJsonDocument::fromJson(dev->readAll()).toVariant().toList();

    QSet<QString> added;
    for (const QVariant &d: data) {
        QVariantMap map = d.toMap();
        QString name=map["name"].toString().trimmed().simplified();
        if (!name.isEmpty()) {
            QVariantList streams=map["streams"].toList();
            int bitrate=0;
            QString url;
            for (const QVariant &s: streams) {
                QVariantMap streamMap=s.toMap();
                int br=streamMap["bitrate"].toInt();
                QString u=streamMap["stream"].toString().trimmed().simplified();
                if (br>bitrate && !u.isEmpty()) {
                    bitrate=br;
                    url=u;
                }
            }

            if (!url.isEmpty() && !added.contains(url)) {
                added.insert(url);
                newItems.append(new Item(url, name, cat, QString::number(bitrate)));
            }
        }
    }
    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseCommunityStations(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QVariantList data=QJsonDocument::fromJson(dev->readAll()).toVariant().toList();

    QSet<QString> added;
    for (const QVariant &d: data) {
        QVariantMap map = d.toMap();
        QString name=map["name"].toString().trimmed().simplified();
        if (!name.isEmpty()) {
            QString url=map["url"].toString().trimmed().simplified();

            if (!url.isEmpty() && !added.contains(url)) {
                added.insert(url);
                QString bitrate=map["bitrate"].toString().trimmed().simplified();
                QString codec=map["codec"].toString().trimmed().simplified();
                newItems.append(new Item(url, name, cat, bitrate.isEmpty() ? codec : (bitrate + " kbps" + (codec.isEmpty() ? "" : (", "+codec)))));
            }
        }
    }
    return newItems;
}

StreamsModel::Item * StreamsModel::parseRadioTimeEntry(QXmlStreamReader &doc, CategoryItem *parent, bool parseSubText)
{
    Item *item=nullptr;
    CategoryItem *cat=nullptr;
    while (!doc.atEnd()) {
        if (doc.isStartElement()) {
            QString text=doc.attributes().value("text").toString();
            if (!text.isEmpty()) {
                QString url=doc.attributes().value("URL").toString();
                QString subText=parseSubText ? doc.attributes().value("subtext").toString() : QString();
                bool isStation=QLatin1String("audio")==doc.attributes().value("type").toString();
                if (isStation) {
                    item=new Item(url, text, parent, subText);
                } else {
                    cat=new CategoryItem(url, text, parent);
                    cat->canBookmark=!url.isEmpty();
                    item=cat;
                }
            }
        }

        doc.readNext();
        if (doc.isStartElement() && QLatin1String("outline")==doc.name()) {
            Item *child = parseRadioTimeEntry(doc, cat);
            if (child) {
                if (cat) {
                    cat->state=CategoryItem::Fetched;
                    cat->children.append(child);
                } else {
                    delete child;
                }
            }
        } else if (doc.isEndElement() && QLatin1String("outline")==doc.name()) {
            break;
        }
    }

    return item;
}

StreamsModel::Item * StreamsModel::parseSomaFmEntry(QXmlStreamReader &doc, CategoryItem *parent)
{
    QString name;
    QString url;
    QString streamFormat;

    while (!doc.atEnd()) {
        doc.readNext();

        if (QXmlStreamReader::StartElement==doc.tokenType()) {
            QStringRef elem = doc.name();

            if (QLatin1String("title")==elem) {
                name=doc.readElementText().trimmed();
            } else if (QLatin1String("fastpls")==elem) {
                if (streamFormat.isEmpty() || QLatin1String("mp3")!=streamFormat) {
                    streamFormat=doc.attributes().value("format").toString();
                    url=doc.readElementText();
                }
            }
        } else if (doc.isEndElement() && QLatin1String("channel")==doc.name()) {
            break;
        }
    }

    return name.isEmpty() || url.isEmpty() ? nullptr : new Item(url, name, parent);
}

void StreamsModel::loadInstalledProviders()
{
    QSet<QString> added;
    QString dir=Utils::dataDir(StreamsModel::constSubDir);
    QStringList subDirs=QDir(dir).entryList(QStringList() << "*", QDir::Dirs|QDir::Readable|QDir::NoDot|QDir::NoDotDot);
    QStringList streamFiles=QStringList() << constCompressedXmlFile << constXmlFile << constSettingsFile;
    for (const QString &sub: subDirs) {
        if (!added.contains(sub)) {
            for (const QString &streamFile: streamFiles) {
                if (QFile::exists(dir+sub+Utils::constDirSep+streamFile)) {
                    addInstalledProvider(sub, getExternalIcon(dir+sub), dir+sub+Utils::constDirSep+streamFile, false);
                    added.insert(sub);
                    break;
                }
            }
        }
    }
}

StreamsModel::CategoryItem * StreamsModel::addInstalledProvider(const QString &name, const QIcon &icon, const QString &streamsFileName, bool replace)
{
    CategoryItem *cat=nullptr;
    if (streamsFileName.endsWith(constSettingsFile)) {
        QFile file(streamsFileName);
        if (file.open(QIODevice::ReadOnly)) {
            QVariantMap map=QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
            QString type=map["type"].toString();
            QString url=map["url"].toString();

            if (!url.isEmpty() && !type.isEmpty()) {
                QStringList toRemove=QStringList() << " " << "." << "/" << "\\" << "(" << ")";
                QString cacheName=name;
                for (const QString &rem: toRemove) {
                    cacheName=cacheName.replace(rem, "");
                }
                cacheName=cacheName.toLower();
                if (type=="di") {
                    cat=new DiCategoryItem(url, name, root, icon, cacheName);
                } else if (type=="soma") {
                    cat=new SomaCategoryItem(url, name, root, icon, cacheName, map["modName"].toBool());
                } else if (type=="listenlive") {
                    cat=new ListenLiveCategoryItem(url, name, root, icon, cacheName);
                }
            }
        }
    } else {
        cat=new XmlCategoryItem(name, root, icon, streamsFileName);
    }

    if (!cat) {
        return nullptr;
    }

    cat->configName="x-"+name;

    if (replace) {
        // Remove any existing entry...
        removeInstalledProvider(cat->configName);
    }

    if (replace) {
        beginInsertRows(QModelIndex(), root->children.count(), root->children.count());
        root->children.append(cat);
        endInsertRows();
    } else {
        root->children.append(cat);
    }
    return cat;
}

void StreamsModel::removeInstalledProvider(const QString &key)
{
    for (Item *i: root->children) {
        if (key==static_cast<CategoryItem *>(i)->configName) {
            int row=root->children.indexOf(i);
            if (row>=0) {
                static_cast<CategoryItem *>(i)->removeCache();
                beginRemoveRows(QModelIndex(), row, row);
                delete root->children.takeAt(row);
                endRemoveRows();
            }
            break;
        }
    }
}

QModelIndex StreamsModel::categoryIndex(const CategoryItem *cat) const
{
    int row=root->children.indexOf(const_cast<CategoryItem *>(cat));
    return -1==row ? QModelIndex() : createIndex(row, 0, (void *)cat);
}

const QString StreamsModel::constPrefix=QLatin1String("cantata-stream-");

QString StreamsModel::modifyUrl(const QString &u, bool addPrefix, const QString &name)
{
    return MPDParseUtils::addStreamName(!addPrefix || !u.startsWith("http:") ? u : (constPrefix+u), name);
}

#include "moc_streamsmodel.cpp"
