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

#include "streamsearchmodel.h"
#include "icons.h"
#include "itemview.h"
#include "localize.h"
#include "qtplural.h"
#include "playqueuemodel.h"
#include "networkaccessmanager.h"
#include "stdactions.h"
#include <QNetworkReply>
#include <QString>
#include <QVariant>
#include <QXmlStreamReader>
#include <QMimeData>
#include <QLocale>
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

static QString constRadioTimeSearchUrl=QLatin1String("http://opml.radiotime.com/Search.ashx");
static QString constShoutCastSearchUrl=QLatin1String("http://")+StreamsModel::constShoutCastHost+QLatin1String("/legacy/genrelist");

StreamSearchModel::StreamSearchModel(QObject *parent)
    : ActionModel(parent)
    , category(TuneIn)
    , root(new StreamsModel::CategoryItem(QString(), "root"))
{
}

StreamSearchModel::~StreamSearchModel()
{
    cancelAll();
    delete root;
}

QModelIndex StreamSearchModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const StreamsModel::CategoryItem * p = parent.isValid() ? static_cast<StreamsModel::CategoryItem *>(parent.internalPointer()) : root;
    const StreamsModel::Item * c = row<p->children.count() ? p->children.at(row) : 0;
    return c ? createIndex(row, column, (void *)c) : QModelIndex();
}

QModelIndex StreamSearchModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    StreamsModel::Item * parent = toItem(index)->parent;

    if (!parent || parent == root || !parent->parent) {
        return QModelIndex();
    }
    return createIndex(static_cast<const StreamsModel::CategoryItem *>(parent->parent)->children.indexOf(parent), 0, parent);
}

QVariant StreamSearchModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int StreamSearchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        StreamsModel::Item *item = toItem(parent);
        return item->isCategory() ? static_cast<StreamsModel::CategoryItem *>(item)->children.count() : 0;
    }
    return root->children.count();
}

int StreamSearchModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant StreamSearchModel::data(const QModelIndex &index, int role) const
{
    const StreamsModel::Item *item = toItem(index);

    switch (role) {
    case Qt::DecorationRole:
        return item->isCategory() ? Icons::self()->streamCategoryIcon : Icons::self()->radioStreamIcon;
    case Qt::DisplayRole:
        return item->name;
    case Qt::ToolTipRole:
        return item->isCategory() ? item->name : (item->name+QLatin1String("<br><small><i>")+item->url+QLatin1String("</i></small>"));
    case ItemView::Role_SubText:
        if (item->isCategory()) {
            const StreamsModel::CategoryItem *cat=static_cast<const StreamsModel::CategoryItem *>(item);
            switch (cat->state) {
            case StreamsModel::CategoryItem::Initial:
                return i18n("Not Loaded");
            case StreamsModel::CategoryItem::Fetching:
                return i18n("Loading...");
            default:
                #ifdef ENABLE_KDE_SUPPORT
                return i18np("1 Entry", "%1 Entries", cat->children.count());
                #else
                return QTP_ENTRIES_STR(cat->children.count());
                #endif
            }
        } else {
            return item->subText.isEmpty() ? QLatin1String("-") : item->subText;
        }
        break;
    case ItemView::Role_Actions:
        if (item->isCategory()){
            if (static_cast<const StreamsModel::CategoryItem *>(item)->canBookmark) {
                QVariant v;
                v.setValue<QList<Action *> >(QList<Action *>() << StreamsModel::self()->addBookmarkAct());
                return v;
            }
        } else {
            QVariant v;
            if (StreamsModel::self()->isFavoritesWritable()) {
                v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction << StreamsModel::self()->addToFavouritesAct());
            } else {
                v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction);
            }
            return v;
        }
        break;
    default:
        break;
    }
    return QVariant();
}

Qt::ItemFlags StreamSearchModel::flags(const QModelIndex &index) const
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

bool StreamSearchModel::hasChildren(const QModelIndex &index) const
{
    return index.isValid() ? toItem(index)->isCategory() : true;
}

bool StreamSearchModel::canFetchMore(const QModelIndex &index) const
{
    if (index.isValid()) {
        StreamsModel::Item *item = toItem(index);
        return item->isCategory() && StreamsModel::CategoryItem::Initial==static_cast<StreamsModel::CategoryItem *>(item)->state && !item->url.isEmpty();
    } else {
        return false;
    }
}

void StreamSearchModel::fetchMore(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    StreamsModel::Item *item = toItem(index);
    if (item->isCategory() && !item->url.isEmpty()) {
        StreamsModel::CategoryItem *cat=static_cast<StreamsModel::CategoryItem *>(item);
        QNetworkReply *job=NetworkAccessManager::self()->get(cat->url);
        if (jobs.isEmpty()) {
            emit loading();
        }
        jobs.insert(job, cat);
        connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        cat->state=StreamsModel::CategoryItem::Fetching;
        emit dataChanged(index, index);
    }
}

QStringList StreamSearchModel::filenames(const QModelIndexList &indexes, bool addPrefix) const
{
    QStringList fnames;
    foreach(QModelIndex index, indexes) {
        StreamsModel::Item *item=static_cast<StreamsModel::Item *>(index.internalPointer());
        if (!item->isCategory() && !fnames.contains(item->url)) {
            fnames << StreamsModel::modifyUrl(item->url, addPrefix, item->name);
        }
    }

    return fnames;
}

QMimeData * StreamSearchModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames(indexes, true));
    return mimeData;
}

QStringList StreamSearchModel::mimeTypes() const
{
    QStringList types;
    types << PlayQueueModel::constFileNameMimeType;
    return types;
}

void StreamSearchModel::clear()
{
    cancelAll();
    beginRemoveRows(QModelIndex(), 0, root->children.count()-1);
    qDeleteAll(root->children);
    root->children.clear();
    currentSearch=QString();
    endRemoveRows();
}

static int getParam(const QString &key, QString &query)
{
    int index=query.indexOf(" "+key+"=");
    int val=0;
    if (-1!=index) {
        int endPos=query.indexOf(" ", index+3);
        int end=endPos;
        if (-1==end) {
            end=query.length()-1;
        }
        int start=index+key.length()+2;
        val=query.mid(start, (end+1)-start).toInt();
        if (endPos>start) {
            query=query.left(index)+query.mid(endPos);
            query=query.trimmed();
        } else {
            query=query.left(index);
        }
    }
    return val;
}

void StreamSearchModel::search(const QString &searchTerm, bool stationsOnly)
{
    if (searchTerm==currentSearch) {
        return;
    }

    clear();
    
    QUrl searchUrl(TuneIn==category ? constRadioTimeSearchUrl : constShoutCastSearchUrl);
    #if QT_VERSION < 0x050000
    QUrl &query=searchUrl;
    #else
    QUrlQuery query;
    #endif

    if (TuneIn==category) {
        if (stationsOnly) {
            query.addQueryItem("types", "station");
        }
        query.addQueryItem("query", searchTerm);
        QString locale=QLocale::system().name();
        if (!locale.isEmpty()) {
            query.addQueryItem("locale", locale);
        }
    } else {
        QString search=searchTerm;
        int limit=getParam("limit", search);
        int bitrate=getParam("br", search);
        if (0==bitrate) {
            bitrate=getParam("bitrate", search);
        }
        query.addQueryItem("k", StreamsModel::constShoutCastApiKey);
        query.addQueryItem("search", search);
        query.addQueryItem("limit", QString::number(limit<1 ? 100 : limit));
        if (bitrate>=32 && bitrate<=512) {
            query.addQueryItem("br", QString::number(bitrate));
        }
    }
    #if QT_VERSION >= 0x050000
    searchUrl.setQuery(query);
    #endif
    QNetworkReply *job=NetworkAccessManager::self()->get(searchUrl);
    if (jobs.isEmpty()) {
        emit loading();
    }
    jobs.insert(job, root);
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
}

void StreamSearchModel::cancelAll()
{
    if (!jobs.isEmpty()) {
        QList<QNetworkReply *> jobList=jobs.keys();
        foreach (QNetworkReply *j, jobList) {
            j->abort();
            j->deleteLater();
            disconnect(j, SIGNAL(finished()), this, SLOT(jobFinished()));
        }
        jobs.clear();
        emit loaded();
    }
}
    
void StreamSearchModel::jobFinished()
{
    QNetworkReply *job=dynamic_cast<QNetworkReply *>(sender());

    if (!job) {
        return;
    }

    job->deleteLater();
    if (jobs.contains(job)) {
        StreamsModel::CategoryItem *cat=jobs[job];
        cat->state=StreamsModel::CategoryItem::Fetched;
        jobs.remove(job);

        QModelIndex index=cat==root ? QModelIndex() : createIndex(cat->parent->children.indexOf(cat), 0, (void *)cat);
        if (QNetworkReply::NoError==job->error()) {
            QList<StreamsModel::Item *> newItems=TuneIn==category
                                                    ? StreamsModel::parseRadioTimeResponse(job, cat, true)
                                                    : StreamsModel::parseShoutCastSearchResponse(job, cat);
            if (!newItems.isEmpty()) {
                beginInsertRows(index, cat->children.count(), (cat->children.count()+newItems.count())-1);
                cat->children+=newItems;
                endInsertRows();
            }
        }
        emit dataChanged(index, index);
        if (jobs.isEmpty()) {
            emit loaded();
        }
    }
}
