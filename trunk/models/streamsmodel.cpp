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

#include "streamsmodel.h"
#include "icons.h"
#include "networkaccessmanager.h"
#include "localize.h"
#include "qtplural.h"
#include "utils.h"
#include "mpdconnection.h"
#include "mpdparseutils.h"
#include "settings.h"
#include "playqueuemodel.h"
#include "itemview.h"
#include "action.h"
#include "stdactions.h"
#include "actioncollection.h"
#include "digitallyimported.h"
#include "qjson/parser.h"
#include "qtiocompressor/qtiocompressor.h"
#include "utils.h"
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <QMimeData>
#include <QXmlStreamReader>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QLocale>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(StreamsModel, instance)
#endif
#if defined Q_OS_WIN
#include <QDesktopServices>
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

const QString StreamsModel::constPrefix=QLatin1String("cantata-");
const QString StreamsModel::constCacheDir=QLatin1String("streams");
const QString StreamsModel::constCacheExt=QLatin1String(".xml.gz");

const QString StreamsModel::constShoutCastApiKey=QLatin1String("fa1669MuiRPorUBw");
const QString StreamsModel::constShoutCastHost=QLatin1String("api.shoutcast.com");

static const char * constOrigUrlProperty = "orig-url";

static QString constRadioTimeHost=QLatin1String("opml.radiotime.com");
static QString constRadioTimeUrl=QLatin1String("http://")+constRadioTimeHost+QLatin1String("/Browse.ashx");
static QString constFavouritesUrl=QLatin1String("cantata://internal");
static QString constIceCastUrl=QLatin1String("http://dir.xiph.org/yp.xml");
static QString constSomaFMUrl=QLatin1String("http://somafm.com/channels.xml");

static QString constDigitallyImportedUrl=QLatin1String("http://www.di.fm");
static QString constJazzRadioUrl=QLatin1String("http://www.jazzradio.com");
static QString constRockRadioUrl=QLatin1String("http://www.rockradio.com");
static QString constSkyFmUrl=QLatin1String("http://www.sky.fm");
static QStringList constDiUrls=QStringList() << constDigitallyImportedUrl << constJazzRadioUrl << constSkyFmUrl << constRockRadioUrl;

static const QString constDiChannelListHost=QLatin1String("api.v2.audioaddict.com");
static const QString constDiChannelListUrl=QLatin1String("http://")+constDiChannelListHost+("/v1/%1/mobile/batch_update?asset_group_key=mobile_icons&stream_set_key=");
static const QString constDiStdUrl=QLatin1String("http://%1/public3/%2.pls");

static QString constShoutCastApiKey=QLatin1String("fa1669MuiRPorUBw");
static QString constShoutCastHost=QLatin1String("api.shoutcast.com");
static QString constShoutCastUrl=QLatin1String("http://")+constShoutCastHost+QLatin1String("/genre/primary?f=xml&k=")+constShoutCastApiKey;

static const QLatin1String constFavouritesFileName("streams.xml.gz");
static const QStringList constExternalStreamFiles=QStringList() << QLatin1String("/streams.xml") << QLatin1String("/streams.xml.gz");

QString StreamsModel::favouritesDir()
{
    return Settings::self()->storeStreamsInMpdDir() ? MPDConnection::self()->getDetails().dir : Utils::configDir(QString(), false);
}

static QString getInternalFile(bool createDir=false)
{
    if (Settings::self()->storeStreamsInMpdDir()) {
        return MPDConnection::self()->getDetails().dir+constFavouritesFileName;
    }
    return Utils::configDir(QString(), createDir)+constFavouritesFileName;
}

static QIcon getIcon(const QString &name)
{
    QIcon icon;
    icon.addFile(":"+name);
    return icon.isNull() ? Icons::self()->streamCategoryIcon : icon;
}

static QIcon getExternalIcon(const QString &path)
{
    static const QString constStreamIcon=QLatin1String("/icon");
    static const QStringList constIconTypes=QStringList() << QLatin1String(".svg") << QLatin1String(".png");

    foreach (const QString &type, constIconTypes) {
        QString iconFile=path+constStreamIcon+type;
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
    return Utils::cacheDir(StreamsModel::constCacheDir, createDir)+name+StreamsModel::constCacheExt;
}

static QString categoryBookmarksName(const QString &name, bool createDir=false)
{
    return Utils::configDir(QLatin1String("bookmarks"), createDir)+name+StreamsModel::constCacheExt;
}

QString StreamsModel::Item::modifiedName() const
{
    if (isCategory()) {
        return name;
    }
    const CategoryItem *cat=getTopLevelCategory();
    if (!cat || !cat->addCatToModifiedName) {
        return name;
    }
    return cat->name+QLatin1String(" - ")+name;
}

const StreamsModel::CategoryItem * StreamsModel::Item::getTopLevelCategory() const
{
    const StreamsModel::Item *item=this;
    while (item->parent && item->parent->parent) {
        item=item->parent;
    }
    return item && item->isCategory() ? static_cast<const CategoryItem *>(item) : 0;
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

    foreach (Item *child, children) {
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
                         foreach (Item *i, cat->children) {
                             doc.writeStartElement("bookmark");
                             doc.writeAttribute("name", i->name);
                             doc.writeAttribute("url", i->url);
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
    foreach (Item *i, children) {
        if (i->isCategory() && static_cast<CategoryItem *>(i)->isBookmarks) {
            return static_cast<CategoryItem *>(i);
        }
    }
    return 0;
}

StreamsModel::CategoryItem * StreamsModel::CategoryItem::createBookmarksCategory()
{
    Icon icon=Icon("bookmarks");
    if (icon.isNull()) {
        icon=Icon("user-bookmarks");
    }
    CategoryItem *bookmarkCat = new CategoryItem(QString(), i18n("Bookmarks"), this, icon);
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

void StreamsModel::ListenLiveCategoryItem::removeCache()
{
    foreach (Item *i, children) {
        if (i->isCategory()) {
            static_cast<CategoryItem *>(i)->removeCache();
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
    QList<Item *> newItems;

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
    if (QFile::exists(cacheName)) {
        return loadXml(cacheName);
    }

    return QList<Item *>();
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
    doc.writeAttribute("url", item->url);
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
    foreach (const StreamsModel::Item *i, cat->children) {
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

    foreach (const Item *i, children) {
        if (i->isCategory()) {
            saveCategory(doc, static_cast<const CategoryItem *>(i));
        } else {
            saveStream(doc, i);
        }
    }
    doc.writeEndElement();
    doc.writeEndDocument();
    return true;
}

QList<StreamsModel::Item *> StreamsModel::CategoryItem::loadXml(const QString &fileName, bool importing)
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

    return loadXml(isCompressed ? (QIODevice *)&compressor : (QIODevice *)&file, importing);
}

QList<StreamsModel::Item *> StreamsModel::CategoryItem::loadXml(QIODevice *dev, bool importing)
{
    Q_UNUSED(importing)

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

QList<StreamsModel::Item *> StreamsModel::FavouritesCategoryItem::loadXml(QIODevice *dev, bool importing)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    QSet<QString> existingUrls;
    QSet<QString> existingNames;

    if (importing) {
        foreach (Item *i, children) {
            existingUrls.insert(i->url);
            existingNames.insert(i->name);
        }
    }

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement() && QLatin1String("stream")==doc.name()) {
            QString name=doc.attributes().value("name").toString();
            QString origName=name;
            QString url=doc.attributes().value("url").toString();

            if (!name.isEmpty() && !name.isEmpty() && (!importing || !existingUrls.contains(url))) {
                int i=1;
                if (importing) {
                    for (; i<100 && existingNames.contains(name); ++i) {
                        name=origName+QLatin1String(" (")+QString::number(i)+QLatin1Char(')');
                    }
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

void StreamsModel::DiCategoryItem::addHeaders(QNetworkRequest &req)
{
    DigitallyImported::self()->addAuthHeader(req);
}

StreamsModel::StreamsModel(QObject *parent)
    : ActionModel(parent)
    , root(new CategoryItem(QString(), "root"))
    , listenLive(0)
    , favouritesIsWriteable(true)
    , favouritesModified(false)
    , favouritesSaveTimer(0)
{
    tuneIn=new CategoryItem(constRadioTimeUrl+QLatin1String("?locale=")+QLocale::system().name(), i18n("TuneIn"), root, getIcon("tunein"), QString(), "tunein");
    tuneIn->supportsBookmarks=true;
    root->children.append(tuneIn);
    root->children.append(new IceCastCategoryItem(constIceCastUrl, i18n("IceCast"), root, getIcon("icecast"), "icecast"));
    shoutCast=new ShoutCastCategoryItem(constShoutCastUrl, i18n("ShoutCast"), root, getIcon("shoutcast"));
    root->children.append(shoutCast);
    root->children.append(new CategoryItem(constSomaFMUrl, i18n("SomaFM"), root, getIcon("somafm"), "somafm", QString(), true));
    root->children.append(new DiCategoryItem(constDigitallyImportedUrl, i18n("Digitally Imported"), root, getIcon("digitallyimported"), "di"));
    root->children.append(new DiCategoryItem(constJazzRadioUrl, i18n("JazzRadio.com"), root, getIcon("jazzradio"), "jazzradio"));
    root->children.append(new DiCategoryItem(constRockRadioUrl, i18n("RockRadio.com"), root, getIcon("rockradio"), "rockradio"));
    root->children.append(new DiCategoryItem(constSkyFmUrl, i18n("Sky.fm"), root, getIcon("skyfm"), "skyfm"));
    favourites=new FavouritesCategoryItem(constFavouritesUrl, i18n("Favorites"), root, getIcon("favourites"));
    root->children.append(favourites);
    buildListenLive();
    buildXml();
    addBookmarkAction = ActionCollection::get()->createAction("bookmarkcategory", i18n("Bookmark Category"), Icon("bookmark-new"));
    addToFavouritesAction = ActionCollection::get()->createAction("addtofavourites", i18n("Add Stream To Favorites"), favouritesIcon());
    configureAction = ActionCollection::get()->createAction("configurestreams", i18n("Configure Streams"), Icons::self()->configureIcon);
    reloadAction = ActionCollection::get()->createAction("reloadstreams", i18n("Reload"), Icon("view-refresh"));
}

StreamsModel::~StreamsModel()
{
    delete root;
}

QModelIndex StreamsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const CategoryItem * p = parent.isValid() ? static_cast<CategoryItem *>(parent.internalPointer()) : root;
    const Item * c = row<p->children.count() ? p->children.at(row) : 0;
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
    const Item *item = toItem(index);

    switch (role) {
    case Qt::DecorationRole:
        if (item->isCategory()) {
            const CategoryItem *cat=static_cast<const CategoryItem *>(item);
            return cat->icon.isNull() ? Icons::self()->streamCategoryIcon : cat->icon;
        } else {
            return Icons::self()->radioStreamIcon;
        }
    case Qt::DisplayRole:
        if (item==favourites && !favouritesIsWriteable) {
            return i18n("%1 (Read-Only)", item->name);
        }
        return item->name;
    case Qt::ToolTipRole:
        return item->isCategory() ? item->name : (item->name+QLatin1String("<br><small><i>")+item->url+QLatin1String("</i></small>"));
    case ItemView::Role_SubText:
        if (item->isCategory()) {
            const CategoryItem *cat=static_cast<const CategoryItem *>(item);
            switch (cat->state) {
            case CategoryItem::Initial:
                return i18n("Not Loaded");
            case CategoryItem::Fetching:
                return i18n("Loading...");
            default:
                #ifdef ENABLE_KDE_SUPPORT
                return i18np("1 Entry", "%1 Entries", cat->children.count());
                #else
                return QTP_ENTRIES_STR(cat->children.count());
                #endif
            }
        }
        break;
    case ItemView::Role_Actions: {
        QList<Action *> actions;
        if (item->isCategory()){
            if (static_cast<const CategoryItem *>(item)->canConfigure()) {
                actions << configureAction;
            }
            if (static_cast<const CategoryItem *>(item)->canReload()) {
                actions << reloadAction;
            }
            if (tuneIn==item || shoutCast==item) {
                actions << StdActions::self()->searchAction;
            }
            if (static_cast<const CategoryItem *>(item)->canBookmark) {
                actions << addBookmarkAction;
            }
        } else {
            actions << StdActions::self()->replacePlayQueueAction;
            if (favouritesIsWriteable && item->parent!=favourites) {
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
    return QVariant();
}

Qt::ItemFlags StreamsModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        if (toItem(index)->isCategory()) {
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        } else {
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
        }
    } else {
        return Qt::NoItemFlags;
    }
}

bool StreamsModel::hasChildren(const QModelIndex &index) const
{
    return index.isValid() ? toItem(index)->isCategory() : true;
}

bool StreamsModel::canFetchMore(const QModelIndex &index) const
{
    if (index.isValid()) {
        Item *item = toItem(index);
        return item->isCategory() && CategoryItem::Initial==static_cast<CategoryItem *>(item)->state && !item->url.isEmpty();
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
        if (item->url==constFavouritesUrl && !getInternalFile().startsWith(QLatin1String("http://"))) {
            cat->state=CategoryItem::Fetching;
            emit dataChanged(index, index);
            loadFavourites(getInternalFile(false), index);
            cat->state=CategoryItem::Fetched;
        } else if (!loadCache(cat)) {
            QNetworkRequest req;
            if (constDiUrls.contains(cat->url)) {
                req=QNetworkRequest(constDiChannelListUrl.arg(cat->url.split(".").at(1)));
            } else {
                req=QNetworkRequest(cat->url);
            }
            cat->addHeaders(req);

            QNetworkReply *job=NetworkAccessManager::self()->get(req);
            job->setProperty(constOrigUrlProperty, cat->url);
            if (jobs.isEmpty()) {
                emit loading();
            }
            jobs.insert(job, cat);
            connect(job, SIGNAL(finished()), SLOT(jobFinished()));
            cat->state=CategoryItem::Fetching;
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

        if (listenLive==cat) {
            QModelIndex index=createIndex(root->children.indexOf(listenLive), 0, (void *)listenLive);
            buildListenLive();
            beginInsertRows(index, 0, listenLive->children.count()-1);
            endInsertRows();
        }
    }

    fetchMore(index);
}

void StreamsModel::saveFavourites(bool force)
{
    if (force) {
        if (favouritesSaveTimer) {
            favouritesSaveTimer->stop();
        }
        persistFavourites();
    } else if (!QFile::exists(getInternalFile(false)) || !QFileInfo(getInternalFile(false)).isWritable()) {
        if (favouritesSaveTimer) {
            favouritesSaveTimer->stop();
        }
        persistFavourites(); // Call persist now so as to log errors immediately
    } else {
        if (!favouritesSaveTimer) {
            favouritesSaveTimer=new QTimer(this);
            connect(favouritesSaveTimer, SIGNAL(timeout()), this, SLOT(persistFavourites()));
        }
        favouritesSaveTimer->start(10*1000);
    }
}

bool StreamsModel::exportFavourites(const QString &fileName)
{
    return favourites->saveXml(fileName, true);
}

bool StreamsModel::checkFavouritesWritable()
{
    bool wasWriteable=favouritesIsWriteable;
    QString dirName=favouritesDir();
    bool isHttp=dirName.startsWith("http:/");
    favouritesIsWriteable=!isHttp && QFileInfo(dirName).isWritable();
    if (favouritesIsWriteable) {
        QString fileName=getInternalFile(false);
        if (QFile::exists(fileName) && !QFileInfo(fileName).isWritable()) {
            favouritesIsWriteable=false;
        }
    }
    if (favouritesIsWriteable!=wasWriteable) {
        QModelIndex index=favouritesIndex();
        emit dataChanged(index, index);
    }
    return favouritesIsWriteable;
}

void StreamsModel::removeFromFavourites(const QModelIndex &index)
{
    Item *item=static_cast<Item *>(index.internalPointer());
    int pos=favourites->children.indexOf(item);

    if (-1!=pos) {
        QModelIndex index=favouritesIndex();
        beginRemoveRows(index, pos, pos);
        delete favourites->children.takeAt(pos);
        endRemoveRows();
        favouritesModified=true;
        saveFavourites();
    }
}

bool StreamsModel::addToFavourites(const QString &url, const QString &name)
{
    QSet<QString> existingNames;

    foreach (Item *i, favourites->children) {
        if (i->url==url) {
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
        QModelIndex index=favouritesIndex();
        beginInsertRows(index, favourites->children.count(), favourites->children.count());
        favourites->children.append(new Item(url, n, favourites));
        endInsertRows();
        favouritesModified=true;
        saveFavourites();
        return true;
    }
    return false;
}

QString StreamsModel::favouritesNameForUrl(const QString &u)
{
    foreach (Item *i, favourites->children) {
        if (i->url==u) {
            return i->name;
        }
    }
    return QString();
}

bool StreamsModel::nameExistsInFavourites(const QString &n)
{
    foreach (Item *i, favourites->children) {
        if (i->name==n) {
            return true;
        }
    }
    return false;
}

void StreamsModel::updateFavouriteStream(Item *item)
{
    int pos=favourites->children.indexOf(item);

    if (-1==pos) {
        return;
    }
    QModelIndex index=createIndex(favourites->children.indexOf(item), 0, (void *)item);
    favouritesModified=true;
    saveFavourites();
    emit dataChanged(index, index);
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

        foreach (Item *i, bookmarkCat->children) {
            if (i->url==url) {
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

bool StreamsModel::validProtocol(const QString &file)
{
    QString scheme=QUrl(file).scheme();
    return scheme.isEmpty() || MPDConnection::self()->urlHandlers().contains(scheme);
}

QString StreamsModel::modifyUrl(const QString &u, bool addPrefix, const QString &name)
{
    return MPDParseUtils::addStreamName(!addPrefix || !u.startsWith("http:") ? u : (constPrefix+u), name);
}

static QString addDiHash(const StreamsModel::Item *item)
{
    return item->parent && constDiUrls.contains(item->parent->url)
            ? DigitallyImported::self()->modifyUrl(item->url) : item->url;
}

static void filenames(QStringList &fn, bool addPrefix, const StreamsModel::CategoryItem *cat)
{
    foreach (const StreamsModel::Item *i, cat->children) {
        if (i->isCategory()) {
            ::filenames(fn, addPrefix, static_cast<const StreamsModel::CategoryItem *>(i));
        } else if (!fn.contains(i->url) && StreamsModel::validProtocol(i->url)) {
            fn << StreamsModel::modifyUrl(addDiHash(i), addPrefix, addPrefix ? i->modifiedName() : i->name);
        }
    }
}

QStringList StreamsModel::filenames(const QModelIndexList &indexes, bool addPrefix) const
{
    QStringList fnames;
    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isCategory()) {
            ::filenames(fnames, addPrefix, static_cast<const StreamsModel::CategoryItem *>(item));
        } else if (!fnames.contains(item->url) && validProtocol(item->url)) {
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
    QNetworkReply *job=dynamic_cast<QNetworkReply *>(sender());

    if (!job) {
        return;
    }

    job->deleteLater();

    if (jobs.contains(job)) {
        CategoryItem *cat=jobs[job];
        cat->state=CategoryItem::Fetched;
        jobs.remove(job);

        QModelIndex index=createIndex(cat->parent->children.indexOf(cat), 0, (void *)cat);
        if (QNetworkReply::NoError==job->error()) {
            QList<Item *> newItems;
            if (cat==favourites) {
                newItems=favourites->loadXml(job);
            } else if (QLatin1String("http")==job->url().scheme()) {
                QString url=job->url().toString();
                if (constRadioTimeHost==job->url().host()) {
                    newItems=parseRadioTimeResponse(job, cat);
                } else if (constIceCastUrl==url) {
                    newItems=parseIceCastResponse(job, cat);
                } else if (constSomaFMUrl==url) {
                    newItems=parseSomaFmResponse(job, cat);
                } else if (constDiChannelListHost==job->url().host()) {
                    newItems=parseDigitallyImportedResponse(job, cat);
                } else if (constShoutCastHost==job->url().host()) {
                    newItems=parseShoutCastResponse(job, cat, job->property(constOrigUrlProperty).toString());
                } else {
                    newItems=parseListenLiveResponse(job, cat);
                }
            }

            if (cat && cat->parent==root && cat->supportsBookmarks) {
                QList<Item *> bookmarks=cat->loadBookmarks();
                if (bookmarks.count()) {
                    CategoryItem *bookmarksCat=cat->getBookmarksCategory();

                    if (bookmarksCat) {
                        QList<Item *> newBookmarks;
                        foreach (Item *bm, bookmarks) {
                            foreach (Item *ex, bookmarksCat->children) {
                                if (ex->url==bm->url) {
                                    delete bm;
                                    bm=0;
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
                        foreach (Item *i, bookmarks) {
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
        }
        emit dataChanged(index, index);
        if (jobs.isEmpty()) {
            emit loaded();
        }
    }
}

void StreamsModel::persistFavourites()
{
    if (favouritesModified) {
        QString fileName=getInternalFile(true);
        favouritesModified=false;
        if (favourites->saveXml(fileName)) {
            Utils::setFilePerms(fileName);
        } else {
            emit error(i18n("Failed to save stream list. Please check %1 is writable.", fileName));
            reloadFavourites();
        }
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

static QString fixSingleGenre(const QString &g)
{
    if (!g.isEmpty()) {
        QString genre=Song::capitalize(g);
        genre[0]=genre[0].toUpper();
        genre=genre.trimmed();
        genre=genre.replace(QLatin1String("Afrocaribbean"), QLatin1String("Afro-Caribbean"));
        genre=genre.replace(QLatin1String("Afro Caribbean"), QLatin1String("Afro-Caribbean"));
        if (genre.length() < 3 ||
                QLatin1String("The")==genre || QLatin1String("All")==genre ||
                QLatin1String("Various")==genre || QLatin1String("Unknown")==genre ||
                QLatin1String("Misc")==genre || QLatin1String("Mix")==genre || QLatin1String("100%")==genre ||
                genre.contains("ÃÂ") || // Broken unicode.
                genre.contains(QRegExp("^#x[0-9a-f][0-9a-f]"))) { // Broken XML entities.
            return QString();
        }

        if (genre==QLatin1String("R&B") || genre==QLatin1String("R B") || genre==QLatin1String("Rnb") || genre==QLatin1String("RnB")) {
            return QLatin1String("R&B");
        }
        if (genre==QLatin1String("Classic") || genre==QLatin1String("Classical")) {
            return QLatin1String("Classical");
        }
        if (genre==QLatin1String("Christian") || genre.startsWith(QLatin1String("Christian "))) {
            return QLatin1String("Christian");
        }
        if (genre==QLatin1String("Rock") || genre.startsWith(QLatin1String("Rock "))) {
            return QLatin1String("Rock");
        }
        if (genre==QLatin1String("Easy") || genre==QLatin1String("Easy Listening")) {
            return QLatin1String("Easy Listening");
        }
        if (genre==QLatin1String("Hit") || genre==QLatin1String("Hits") || genre==QLatin1String("Easy listening")) {
            return QLatin1String("Hits");
        }
        if (genre==QLatin1String("Hip") || genre==QLatin1String("Hiphop") || genre==QLatin1String("Hip Hop") || genre==QLatin1String("Hop Hip")) {
            return QLatin1String("Hip Hop");
        }
        if (genre==QLatin1String("News") || genre==QLatin1String("News talk")) {
            return QLatin1String("News");
        }
        if (genre==QLatin1String("Top40") || genre==QLatin1String("Top 40") || genre==QLatin1String("40Top") || genre==QLatin1String("40 Top")) {
            return QLatin1String("Top 40");
        }

        QStringList small=QStringList() << QLatin1String("Adult Contemporary") << QLatin1String("Alternative")
                                        << QLatin1String("Community Radio") << QLatin1String("Local Service")
                                        << QLatin1String("Multiultural") << QLatin1String("News")
                                        << QLatin1String("Student") << QLatin1String("Urban");

        foreach (const QString &s, small) {
            if (genre==s || genre.startsWith(s+" ") || genre.endsWith(" "+s)) {
                return s;
            }
        }

        // Convert XX's to XXs
        if (genre.contains(QRegExp("^[0-9]0's$"))) {
            genre=genre.remove('\'');
        }
        if (genre.length()>25 && (0==genre.indexOf(QRegExp("^[0-9]0s ")) || 0==genre.indexOf(QRegExp("^[0-9]0 ")))) {
            int pos=genre.indexOf(' ');
            if (pos>1) {
                genre=genre.left(pos);
            }
        }
        // Convert 80 -> 80s.
        return genre.contains(QRegExp("^[0-9]0$")) ? genre + 's' : genre;
    }
    return g;
}

static QStringList fixGenres(const QString &genre)
{
    QString g(genre);
    int pos=g.indexOf("<br");
    if (pos>3) {
        g=g.left(pos);
    }
    pos=g.indexOf("(");
    if (pos>3) {
        g=g.left(pos);
    }

    g=Song::capitalize(g);
    QStringList genres=g.split('|', QString::SkipEmptyParts);
    QStringList allGenres;

    foreach (const QString &genre, genres) {
        allGenres+=genre.split('/', QString::SkipEmptyParts);
    }

    QStringList fixed;
    foreach (const QString &genre, allGenres) {
        QString g=fixSingleGenre(genre).trimmed();
        if (!g.isEmpty()) {
            fixed.append(g);
        }
    }
    return fixed;
}

static void trimGenres(QMap<QString, QList<StreamsModel::Item *> > &genres)
{
    QString other=i18n("Other");
    QSet<QString> genreSet = genres.keys().toSet();
    foreach (const QString &genre, genreSet) {
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
                if (stationGenres.isEmpty()) {
                    stationGenres.append(i18n("Other"));
                }
                foreach (const QString &g, stationGenres) {
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
        foreach (Item *i, it.value()) {
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
    QJson::Parser parser;
    #ifdef Q_OS_WIN
    QVariantMap data = parser.parse(dev->readAll()).toMap();
    #else
    QVariantMap data = parser.parse(dev).toMap();
    #endif
    QString listenHost=QLatin1String("listen.")+QUrl(cat->url).host().remove("www.");

    if (data.contains("channel_filters")) {
        QVariantList filters = data["channel_filters"].toList();

        foreach (const QVariant &filter, filters) {
            // Find the filter called "All"
            QVariantMap filterMap = filter.toMap();
            if (filterMap.value("name", QString()).toString() != "All") {
                continue;
            }

            // Add all its stations to the result
            QVariantList channels = filterMap.value("channels", QVariantList()).toList();
            foreach (const QVariant &channel, channels) {
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

    int weight() const { return ((bitrate&0xff)<<8)+(format&0x0f); }

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
    void clear() { name=location=comment=QString(); streams.clear(); }
    QString name;
    QString location;
    QString comment;
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
    QSet<QString> names;
    QMap<QString, QList<Item *> > genres;

    if (dev) {
        ListenLiveStationEntry entry;

        while (!dev->atEnd()) {
            QString line=dev->readLine().trimmed().replace("> <", "><").replace("<td><b><a href", "<td><a href")
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
                } else {
                    entry.comment=getString(line, "<td>", "</td>");
                }
            } else if ("</tr>"==line) {
                if (entry.streams.count()) {
                    qSort(entry.streams);
                    QString name;
                    QString url=entry.streams.at(0).url;

                    if (QLatin1String("National")==entry.location || entry.name.endsWith("("+entry.location+")")) {
                        name=entry.name;
                    } else if (entry.name.endsWith(")")) {
                        name=entry.name.left(entry.name.length()-1)+", "+entry.location+")";
                    } else {
                        name=entry.name+" ("+entry.location+")";
                    }

                    if (!names.contains(name) && !name.isEmpty() && url.contains("://")) {
                        QStringList stationGenres=fixGenres(entry.comment);
                        if (stationGenres.isEmpty()) {
                            stationGenres.append(i18n("Other"));
                        }
                        foreach (const QString &g, stationGenres) {
                            genres[g].append(new Item(url, name, cat));
                        }
                        names.insert(name);
                    }
                }
            }
        }
    }

    QMap<QString, QList<Item *> >::ConstIterator it(genres.constBegin());
    QMap<QString, QList<Item *> >::ConstIterator end(genres.constEnd());
    CategoryItem *all=0;

    if (!genres.isEmpty()) {
        all=new CategoryItem(QString(), i18n("All"), cat);
        all->state=CategoryItem::Fetched;
        all->isAll=true;
        newItems.append(all);
    }
    QSet<QString> allUrls;
    for (; it!=end; ++it) {
        CategoryItem *genre=new CategoryItem(QString(), it.key(), cat);
        genre->state=CategoryItem::Fetched;
        foreach (Item *i, it.value()) {
            i->parent=genre;
            genre->children.append(i);
            if (!allUrls.contains(i->url)) {
                allUrls.insert(i->url);
                all->children.append(new Item(i->url, i->name, all));
            }
        }
        newItems.append(genre);
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

QList<StreamsModel::Item *> StreamsModel::parseShoutCastResponse(QIODevice *dev, CategoryItem *cat, const QString &origUrl)
{
    bool isRoot=origUrl==constShoutCastUrl;
    bool wasLinks=false;
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("genrelist")==doc.name()) {
            wasLinks=true;
            newItems+=parseShoutCastLinks(doc, cat);
        } else if (doc.isStartElement() && QLatin1String("stationlist")==doc.name()) {
            newItems+=parseShoutCastStations(doc, cat);
        }
    }

    if (!isRoot && wasLinks) {
        // Get stations...
        QUrl url(QLatin1String("http://")+constShoutCastHost+QLatin1String("/legacy/genresearch"));
        #if QT_VERSION < 0x050000
        QUrl &query=url;
        #else
        QUrlQuery query;
        #endif
        query.addQueryItem("k", constShoutCastApiKey);
        query.addQueryItem("genre", cat->name);
        #if QT_VERSION >= 0x050000
        url.setQuery(query);
        #endif

        QNetworkRequest req(url);
        cat->addHeaders(req);
        QNetworkReply *job=NetworkAccessManager::self()->get(req);
        job->setProperty(constOrigUrlProperty, url.toString());
        if (jobs.isEmpty()) {
            emit loading();
        }
        jobs.insert(job, cat);
        connect(job, SIGNAL(finished()), SLOT(jobFinished()));
    }
    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseShoutCastLinks(QXmlStreamReader &doc, CategoryItem *cat)
{
    QList<Item *> newItems;
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("genre")==doc.name()) {
            newItems.append(new ShoutCastCategoryItem(QLatin1String("http://")+constShoutCastHost+QLatin1String("/genre/secondary?parentid=")+
                                                      doc.attributes().value("id").toString()+QLatin1String("&f=xml&k=")+constShoutCastApiKey,
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
            newItems.append(new Item(QLatin1String("http://yp.shoutcast.com/sbin/tunein-station.pls?id=")+
                                        doc.attributes().value("id").toString(),
                                     doc.attributes().value("name").toString(), cat));
        } else if (doc.isEndElement() && QLatin1String("stationlist")==doc.name()) {
            return newItems;
        }
    }

    return newItems;
}

StreamsModel::Item * StreamsModel::parseRadioTimeEntry(QXmlStreamReader &doc, CategoryItem *parent, bool parseSubText)
{
    Item *item=0;
    CategoryItem *cat=0;
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

    return name.isEmpty() || url.isEmpty() ? 0 : new Item(url, name, parent);
}

bool StreamsModel::loadFavourites(const QString &fileName, const QModelIndex &index, bool importing)
{
    QList<Item *> newItems=favourites->loadXml(fileName, importing);

    if (!newItems.isEmpty()) {
        beginInsertRows(index, favourites->children.count(), (favourites->children.count()+newItems.count())-1);
        favourites->children+=newItems;
        endInsertRows();
        return true;
    }
    return false;
}

void StreamsModel::buildListenLive()
{
    QFile f(":listenlive.xml");
    if (f.open(QIODevice::ReadOnly)) {
        if (!listenLive) {
            listenLive=new ListenLiveCategoryItem(i18n("Listen Live"), root, getIcon("listenlive"));
            root->children.append(listenLive);
            listenLive->state=CategoryItem::Fetched;
        }
        CategoryItem *region=listenLive;
        CategoryItem *prevRegion=listenLive;
        QXmlStreamReader doc(&f);
        while (!doc.atEnd()) {
            doc.readNext();
            if (doc.isStartElement()) {
                if (QLatin1String("listing")==doc.name()) {
                    QString url=doc.attributes().value("url").toString();
                    QString cache=doc.attributes().value("cache").toString();
                    if (cache.isEmpty() && url.endsWith(".html")) {
                        QStringList parts=url.split("/", QString::SkipEmptyParts);
                        if (!parts.isEmpty()) {
                            cache=parts.last().remove((".html"));
                        }
                    }
                    if (!cache.isEmpty()) {
                        cache="ll-"+cache;
                    }
                    region->children.append(new CategoryItem(url,
                                                             doc.attributes().value("name").toString(),
                                                             region, QIcon(), cache));
                } else if (QLatin1String("region")==doc.name()) {
                    prevRegion=region;
                    region=new ListenLiveCategoryItem(doc.attributes().value("name").toString(), prevRegion);
                    region->state=CategoryItem::Fetched;
                    prevRegion->children.append(region);
                }
            } else if (QLatin1String("region")==doc.name()) {
                region=prevRegion;
            }
        }
    }
}

void StreamsModel::buildXml()
{
    #ifdef Q_OS_WIN
    QStringList dirs=QStringList() << QCoreApplication::applicationDirPath()+"/streams/";
    #else
    QStringList dirs=QStringList() << Utils::configDir("streams")
                                   << INSTALL_PREFIX "/share/cantata/streams/";
    #endif
    QSet<QString> added;

    foreach (const QString &dir, dirs) {
        if (dir.isEmpty()) {
            continue;
        }
        QDir d(dir);
        QStringList subDirs=d.entryList(QStringList() << "*", QDir::Dirs|QDir::Readable|QDir::NoDot|QDir::NoDotDot);
        foreach (const QString &sub, subDirs) {
            if (!added.contains(sub)) {
                foreach (const QString &streamFile, constExternalStreamFiles) {
                    if (QFile::exists(dir+sub+streamFile)) {
                        CategoryItem *cat=new XmlCategoryItem(sub, root, getExternalIcon(dir+sub), dir+sub+streamFile);
                        added.insert(sub);
                        root->children.append(cat);
                        break;
                    }
                }
            }
        }
    }
}
