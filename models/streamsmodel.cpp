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
#include <QtCore/QDataStream>
#include <QtCore/QMimeData>
#include <QtCore/QStringList>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include "streamsmodel.h"
#include "settings.h"

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(StreamsModel, instance)
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
            QString str=url.toString();
            if (str.endsWith("&")) {
                str=str.left(str.length()-1);
                url=QUrl(str);
            }
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

bool StreamsModel::importXml(const QString &xml)
{
    QDomDocument doc;
    bool valid=false;
    doc.setContent(xml);
    QDomElement root = doc.documentElement();
    if ("cantata" == root.tagName() && root.hasAttribute("version") && "1.0" == root.attribute("version")) {
        QDomElement stream = root.firstChildElement("stream");
        bool firstInsert=true;
        while (!stream.isNull()) {
            if (stream.hasAttribute("name") && stream.hasAttribute("url")) {
                valid=true;
                QString name=stream.attribute("name");
                QString origName=name;
                QUrl url=QUrl(stream.attribute("url"));

                if (!entryExists(QString(), url)) {
                    int i=1;
                    for (; i<100 && entryExists(name); ++i) {
                        name=origName+QLatin1String("_")+QString::number(i);
                    }

                    if (i<100) {
                        if (firstInsert) {
                            beginResetModel();
                            firstInsert=false;
                        }

                        items.append(Stream(name, url));
                        itemMap.insert(url.toString(), name);
                    }
                }
            }
            stream=stream.nextSiblingElement("stream");
        }
        if (!firstInsert) {
            endResetModel();
        }
    }

    return valid;
}

QString StreamsModel::toXml()
{
    QDomDocument doc;
    QDomElement root = doc.createElement("cantata");
    root.setAttribute("version", "1.0");
    doc.appendChild(root);
    foreach (const Stream &s, items) {
        QDomElement stream = doc.createElement("stream");
        stream.setAttribute("name", s.name);
        stream.setAttribute("url", s.url.toString());
        root.appendChild(stream);
    }

    return doc.toString();
}

bool StreamsModel::add(const QString &name, const QString &url)
{
    QUrl u(url);

    if (entryExists(name, url)) {
        return false;
    }

//     int row=items.count();
    beginResetModel();
//     beginInsertRows(createIndex(0, 0), row, row); // ????
    items.append(Stream(name, u));
    itemMap.insert(url, name);
//     endInsertRows();
//     emit layoutChanged();
    endResetModel();
    return true;
}

void StreamsModel::edit(const QModelIndex &index, const QString &name, const QString &url)
{
    QUrl u(url);
    int row=index.row();

    if (row<items.count()) {
        Stream old=items.at(row);
        beginResetModel();
        items.removeAt(index.row());
        itemMap.remove(old.url.toString());
        row=items.count();
        items.append(Stream(name, u));
        itemMap.insert(url, name);
        endResetModel();
    }
}

void StreamsModel::remove(const QModelIndex &index)
{
    int row=index.row();

    if (row<items.count()) {
        Stream old=items.at(row);
        beginResetModel();
        items.removeAt(index.row());
        itemMap.remove(old.url.toString());
        endResetModel();
    }
}

QString StreamsModel::name(const QString &url)
{
    QHash<QString, QString>::ConstIterator it=itemMap.find(url);

    return it==itemMap.end() ? QString() : it.value();
}

bool StreamsModel::entryExists(const QString &name, const QUrl &url)
{
    foreach (const Stream &s, items) {
        if ( (!name.isEmpty() && s.name==name) || (!url.isEmpty() && s.url==url)) {
            return true;
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

QMimeData * StreamsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    QStringList filenames;

    foreach(QModelIndex index, indexes) {
        if (index.row()<items.count()) {
            filenames << items.at(index.row()).url.toString();
        }
    }

    for (int i = filenames.size() - 1; i >= 0; i--) {
        stream << filenames.at(i);
    }

    mimeData->setData("application/cantata_songs_filename_text", encodedData);
    return mimeData;
}
