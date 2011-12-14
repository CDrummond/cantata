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

class QTimer;

class StreamsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Stream
    {
        Stream(const QString &n, const QUrl &u, bool fav) : name(n), url(u), favorite(fav) { }
        QString name;
        QUrl url;
        bool favorite;
    };

    StreamsModel();
    ~StreamsModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    void reload();
    void save(bool force=false);
    bool save(const QString &filename);
    bool import(const QString &filename) { return load(filename, false); }
    bool add(const QString &name, const QString &url, bool fav);
    void edit(const QModelIndex &index, const QString &name, const QString &url, bool fav);
    void remove(const QModelIndex &index);
    QString name(const QString &url);
    bool entryExists(const QString &name, const QUrl &url=QUrl());
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    void mark(const QList<int> &rows, bool f);

private:
    bool load(const QString &filename, bool isInternal);

private Q_SLOTS:
    void persist();

private:
    QHash<QString, QString> itemMap;
    QList<Stream> items;
    QTimer *timer;
};

#endif
