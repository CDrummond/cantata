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
#include "icon.h"
#include "icons.h"
#include "networkaccessmanager.h"
#include "localize.h"
#include "utils.h"
#include "mpdconnection.h"
#include "mpdparseutils.h"
#include "settings.h"
#include "playqueuemodel.h"
#include "itemview.h"
#include "action.h"
#include "stdactions.h"
#include "qjson/parser.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <QMimeData>
#include <QXmlStreamReader>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

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

static const char * constOrigUrlProperty = "orig-url";

static QString constRadioTimeHost=QLatin1String("opml.radiotime.com");
static QString constRadioTimeUrl=QLatin1String("http://")+constRadioTimeHost+QLatin1String("/Browse.ashx");
static QString constFavouritesUrl=QLatin1String("cantata://internal");
static QString constIceCastUrl=QLatin1String("http://dir.xiph.org/yp.xml");
static QString constSomaFMUrl=QLatin1String("http://somafm.com/channels.xml");

static QString constDigitiallyImportedUrl=QLatin1String("http://www.di.fm");
static QString constJazzRadioUrl=QLatin1String("http://www.jazzradio.com");
static QString constSkyFmUrl=QLatin1String("http://www.sky.fm");
static QStringList constDiUrls=QStringList() << constDigitiallyImportedUrl << constJazzRadioUrl << constSkyFmUrl;
static const char * constDiApiUsername="ephemeron";
static const char * constDiApiPassword="dayeiph0ne@pp";
//static const QString constDiAuthUrl=QLatin1String("http://api.audioaddict.com/v1/%1/members/authenticate");
static const QString constDiChannelListHost=QLatin1String("api.v2.audioaddict.com");
static const QString constDiChannelListUrl=QLatin1String("http://")+constDiChannelListHost+("/v1/%1/mobile/batch_update?asset_group_key=mobile_icons&stream_set_key=");
static const QString constDiStdUrl=QLatin1String("http://%1/public3/%2.pls");

static const QLatin1String constFavouritesFileName("streams.xml.gz");

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

StreamsModel::StreamsModel(QObject *parent)
    : ActionModel(parent)
    , root(new CategoryItem(QString(), "root"))
    , favouritesIsWriteable(true)
    , favouritesModified(false)
    , favouritesSaveTimer(0)
{
    root->children.append(new CategoryItem(constRadioTimeUrl, i18n("TuneIn"), root, QIcon(":streams-tunein")));
    root->children.append(new CategoryItem(constIceCastUrl, i18n("IceCast"), root, QIcon(":streams-icecast")));
    root->children.append(new CategoryItem(constSomaFMUrl, i18n("SomaFM"), root, QIcon(":streams-somafm")));
    root->children.append(new CategoryItem(constDigitiallyImportedUrl, i18n("Digitally Imported"), root, QIcon(":streams-digitallyimported")));
    root->children.append(new CategoryItem(constJazzRadioUrl, i18n("JazzRadio.com"), root, QIcon(":streams-jazzradio")));
    root->children.append(new CategoryItem(constSkyFmUrl, i18n("Sky.fm"), root, QIcon(":streams-skyfm")));
    favourites=new CategoryItem(constFavouritesUrl, i18n("Favourites"), root, QIcon(":streams-favourites"));
    favourites->isFavourites=true;
    root->children.append(favourites);
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

    return createIndex(static_cast<CategoryItem *>(parent->parent)->children.indexOf(parent), 0, parent);
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
            return Icon("audio-x-generic");
        }
    case Qt::DisplayRole:
        return item->name;
    case Qt::ToolTipRole:
        return item->isCategory() ? item->name : item->url;
    case ItemView::Role_SubText:
        if (item->isCategory()) {
            const CategoryItem *cat=static_cast<const CategoryItem *>(item);
            switch (cat->state) {
            case CategoryItem::Initial:
                return i18n("No Loaded");
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
    case ItemView::Role_Actions:
        if (!item->isCategory()){
            QVariant v;
            v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction);
            return v;
        }
        break;
    default:
        break;
    }
    return QVariant();
}

Qt::ItemFlags StreamsModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
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
            loadFavourites(index);
            cat->state=CategoryItem::Fetched;
        } else {
            QNetworkRequest req;
            if (constDiUrls.contains(cat->url)) {
                req=QNetworkRequest(constDiChannelListUrl.arg(cat->url.split(".").at(1)));
                #if QT_VERSION < 0x050000
                req.setRawHeader("Authorization", "Basic "+QString("%1:%2").arg(constDiApiUsername, constDiApiPassword).toAscii().toBase64());
                #else
                req.setRawHeader("Authorization", "Basic "+QString("%1:%2").arg(constDiApiUsername, constDiApiPassword).toLatin1().toBase64());
                #endif
            } else {
                req=QNetworkRequest(cat->url);
            }

            if (cat==favourites && !favourites->children.isEmpty()) {
                beginRemoveRows(index, 0, favourites->children.count()-1);
                qDeleteAll(favourites->children);
                favourites->children.clear();
                endRemoveRows();
            }

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

bool StreamsModel::checkFavouritesWritable()
{
    QString dirName=favouritesDir();
    bool isHttp=dirName.startsWith("http:/");
    favouritesIsWriteable=!isHttp && QFileInfo(dirName).isWritable();
    if (favouritesIsWriteable) {
        QString fileName=getInternalFile(false);
        if (QFile::exists(fileName) && !QFileInfo(fileName).isWritable()) {
            favouritesIsWriteable=false;
        }
    }
    return favouritesIsWriteable;
}

void StreamsModel::reloadFavourites()
{
    fetchMore(createIndex(root->children.indexOf(favourites), 0, favourites));
}

void StreamsModel::removeFromFavourites(const QModelIndex &index)
{
    Item *item=static_cast<Item *>(index.internalPointer());
    int pos=favourites->children.indexOf(item);

    if (-1!=pos) {
        QModelIndex index=createIndex(root->children.indexOf(favourites), 0, (void *)favourites);
        beginRemoveRows(index, pos, pos);
        delete favourites->children.takeAt(pos);
        endRemoveRows();
        favouritesModified=true;
        saveFavourites();
    }
}

void StreamsModel::addToFavourites(const QString &url, const QString &name)
{
    QSet<QString> existingNames;

    foreach (Item *i, favourites->children) {
        if (i->url==url) {
            return;
        }
        existingNames.insert(i->name);
    }

    QString n=name;
    int i=1;
    for (; i<100 && existingNames.contains(n); ++i) {
        n=name+QLatin1String("_")+QString::number(i);
    }

    if (i<100) {
        QModelIndex index=createIndex(root->children.indexOf(favourites), 0, (void *)favourites);
        beginInsertRows(index, favourites->children.count(), favourites->children.count());
        favourites->children.append(new Item(url, n, favourites));
        endInsertRows();
        favouritesModified=true;
        saveFavourites();
    }
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

bool StreamsModel::importXml(const QString &fileName)
{
    return loadXml(fileName, createIndex(root->children.indexOf(favourites), 0, favourites));
}

bool StreamsModel::saveXml(const QString &fileName, const QList<Item *> &items)
{
    QFile file(fileName);

    if (fileName.endsWith(".xml")) {
        return file.open(QIODevice::WriteOnly) && saveXml(&file, items.isEmpty() ? favourites->children : items, true);
    } else {
        QtIOCompressor compressor(&file);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        return compressor.open(QIODevice::WriteOnly) && saveXml(&compressor, items.isEmpty() ? favourites->children : items, false);
    }
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

static void filenames(QStringList &fn, bool addPrefix, const StreamsModel::CategoryItem *cat)
{
    foreach (const StreamsModel::Item *i, cat->children) {
        if (i->isCategory()) {
            filenames(fn, addPrefix, static_cast<const StreamsModel::CategoryItem *>(i));
        } else if (!fn.contains(i->url) && StreamsModel::validProtocol(i->url)) {
            fn << StreamsModel::modifyUrl(i->url, addPrefix, i->name);
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
            fnames << modifyUrl(item->url, addPrefix, item->name);
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
                newItems=loadXml(job, true);
            } else if (QLatin1String("http")==job->url().scheme()) {
                QString url=job->url().toString();
                if (constRadioTimeHost==job->url().host()) {
                    newItems=parseRadioTimeResponse(job, cat);
                } else if (constIceCastUrl==url) {
                    newItems=parseIceCastResponse(job, cat);
                } else if (constSomaFMUrl==url) {
                    newItems=parseSomaFmResponse(job, cat);
                } else if (constDiChannelListHost==job->url().host()) {
                    newItems=parseDigitallyImportedResponse(job, cat, job->property(constOrigUrlProperty).toString());
                }
            }

            if (!newItems.isEmpty()) {
                beginInsertRows(index, 0, newItems.count()-1);
                cat->children=newItems;
                endInsertRows();
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
        if (favourites->children.isEmpty()) {
            // No entries, so remove file...
            if (QFile::exists(fileName) && !QFile::remove(fileName)) {
                emit error(i18n("Failed to save stream list. Please check %1 is writable.").arg(fileName));
                reloadFavourites();
            }
        } else if (saveXml(fileName, favourites->children)) {
            Utils::setFilePerms(fileName);
        } else {
            emit error(i18n("Failed to save stream list. Please check %1 is writable.").arg(fileName));
            reloadFavourites();
        }
    }
}

QList<StreamsModel::Item *> StreamsModel::parseRadioTimeResponse(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("outline")==doc.name()) {
            Item *item = parseRadioTimeEntry(doc, cat);
            if (item) {
                newItems.append(item);
            }
        }
    }
    return newItems;
}

QList<StreamsModel::Item *> StreamsModel::parseIceCastResponse(QIODevice *dev, CategoryItem *cat)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("entry")==doc.name()) {
            Item *item = parseIceCastEntry(doc, cat);
            if (item) {
                newItems.append(item);
            }
        }
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

QList<StreamsModel::Item *> StreamsModel::parseDigitallyImportedResponse(QIODevice *dev, CategoryItem *cat, const QString &origUrl)
{
    QList<Item *> newItems;
    QJson::Parser parser;
    QVariantMap data = parser.parse(dev).toMap();
    QString listenHost=QLatin1String("listen.")+QUrl(origUrl).host().remove("www.");

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

StreamsModel::Item * StreamsModel::parseRadioTimeEntry(QXmlStreamReader &doc, CategoryItem *parent)
{
    Item *item=0;
    CategoryItem *cat=0;
    while (!doc.atEnd()) {
        if (doc.isStartElement()) {
            QString text=doc.attributes().value("text").toString();
            if (!text.isEmpty()) {
                QString url=doc.attributes().value("URL").toString();
                bool isStation=QLatin1String("audio")==doc.attributes().value("type").toString();
                if (isStation) {
                    item=new Item(url, text, parent);
                } else {
                    item=cat=new CategoryItem(url, text, parent);
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

StreamsModel::Item * StreamsModel::parseIceCastEntry(QXmlStreamReader &doc, CategoryItem *parent)
{
    QString name;
    QString url;
    while (!doc.atEnd()) {
        doc.readNext();

        if (QXmlStreamReader::StartElement==doc.tokenType()) {
            QStringRef elem = doc.name();

            if (QLatin1String("server_name")==elem) {
                name=doc.readElementText().trimmed();
            } else if (QLatin1String("listen_url")==elem) {
                url=doc.readElementText().toFloat();
            }
        } else if (doc.isEndElement() && QLatin1String("entry")==doc.name()) {
            break;
        }
    }

    return name.isEmpty() || url.isEmpty() ? 0 : new Item(url, name, parent);
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

void StreamsModel::loadFavourites(const QModelIndex &index)
{
    if (!favourites->children.isEmpty()) {
        beginRemoveRows(index, 0, favourites->children.count()-1);
        qDeleteAll(favourites->children);
        favourites->children.clear();
        endRemoveRows();
    }
    loadXml(getInternalFile(false), index);
}

bool StreamsModel::loadXml(const QString &fileName, const QModelIndex &index)
{
    QFile file(fileName);
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

    QList<Item *> newItems=loadXml(isCompressed ? (QIODevice *)&compressor : (QIODevice *)&file, true);

    if (!newItems.isEmpty()) {
        beginInsertRows(index, favourites->children.count(), (favourites->children.count()+newItems.count())-1);
        favourites->children+=newItems;
        endInsertRows();
        return true;
    }
    return false;
}

QList<StreamsModel::Item *> StreamsModel::loadXml(QIODevice *dev, bool isInternal)
{
    QList<Item *> newItems;
    QXmlStreamReader doc(dev);
    QSet<QString> existingUrls;
    QSet<QString> existingNames;

    if (!isInternal) {
        foreach (Item *i, favourites->children) {
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

            if (!name.isEmpty() && !name.isEmpty() && (isInternal || !existingUrls.contains(url))) {
                int i=1;
                if (!isInternal) {
                    for (; i<100 && existingNames.contains(name); ++i) {
                        name=origName+QLatin1String("_")+QString::number(i);
                    }
                }

                if (i<100) {
                    existingNames.insert(name);
                    existingUrls.insert(url);
                    newItems.append(new Item(url, name, favourites));
                }
            }
        }
    }
    return newItems;
}

bool StreamsModel::saveXml(QIODevice *dev, const QList<Item *> &items, bool format) const
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

    foreach (const Item *i, items) {
        doc.writeStartElement("stream");
        doc.writeAttribute("name", i->name);
        doc.writeAttribute("url", i->url);
        doc.writeEndElement();
    }
    doc.writeEndElement();
    doc.writeEndDocument();
    return true;
}
