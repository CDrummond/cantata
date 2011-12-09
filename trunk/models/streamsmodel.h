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
#ifndef STREAMSMODEL_H
#define STREAMSMODEL_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QList>
#include <QtCore/QUrl>
#include <QtCore/QHash>

class StreamsModel : public QAbstractListModel
{
public:
    struct Stream
    {
        Stream(const QString &n, const QUrl &u) : name(n), url(u) { }
        QString name;
        QUrl url;
    };

    StreamsModel();
    ~StreamsModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &, int) const;
    void reload();
    void save();
    QString toXml();
    bool importXml(const QString &xml);
    bool add(const QString &name, const QString &url);
    void edit(const QModelIndex &index, const QString &name, const QString &url);
    void remove(const QModelIndex &index);
    QString name(const QString &url);
    bool entryExists(const QString &name, const QUrl &url=QUrl());
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;

private:
    QHash<QString, QString> itemMap;
    QList<Stream> items;
};

#endif
