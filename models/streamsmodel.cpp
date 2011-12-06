/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "streamsmodel.h"
#include "settings.h"
#include <QDebug>

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#include <KDE/KGlobal>
K_GLOBAL_STATIC(StreamsModel, instance)
#else
#endif

StreamsModel * StreamsModel::self()
{
#ifdef ENABLE_KDE_SUPPORT
    return instance;
#else
    static StreamsModel *instance=0;;
    if(!instance) {
        instance=new StreamsModel;
    }
    return instance;
#endif
}


StreamsModel::StreamsModel()
    : QAbstractListModel(0)
{
    reload();
}

StreamsModel::~StreamsModel()
{
}

QVariant StreamsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int StreamsModel::rowCount(const QModelIndex &) const
{
    return items.size();
}

QVariant StreamsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= items.size()) {
        return QVariant();
    }

    if (Qt::DisplayRole == role) {
        return items.at(index.row()).name;
    } else if(Qt::ToolTipRole == role) {
        return items.at(index.row()).url;
    }

    return QVariant();
}

static const QLatin1String constNameQuery("CantataStreamName");

void StreamsModel::reload()
{
    beginResetModel();
    QList<QUrl> urls=Settings::self()->streamUrls();
    items.clear();
    itemMap.clear();
    foreach (const QUrl &u, urls) {
        QUrl url(u);
        QString name=url.queryItemValue(constNameQuery);
        if (!name.isEmpty()) {
            url.removeQueryItem(constNameQuery);
            items.append(Stream(name, url));
            itemMap.insert(url.toString(), name);
        }
    }
    endResetModel();
}

void StreamsModel::save()
{
    QList<QUrl> urls;

    foreach (const Stream &s, items) {
        QUrl u(s.url);
        u.addQueryItem(constNameQuery, s.name);
        urls.append(u);
    }
    Settings::self()->saveStreamUrls(urls);
}

bool StreamsModel::add(const QString &name, const QString &url)
{
    QUrl u(url);

    foreach (const Stream &s, items) {
        if (s.name==name || s.url==url) {
            return false;
        }
    }

    int row=items.count();
    beginInsertRows(createIndex(0, 0), row, row); // ????
    items.append(Stream(name, u));
    endInsertRows();
    emit layoutChanged();
    return true;
}

QString StreamsModel::name(const QString &url)
{
    QHash<QString, QString>::ConstIterator it=itemMap.find(url);

    return it==itemMap.end() ? QString() : it.value();
}
