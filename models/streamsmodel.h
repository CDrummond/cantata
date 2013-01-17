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
#ifndef STREAMSMODEL_H
#define STREAMSMODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtCore/QUrl>
#include <QtCore/QHash>
#include <QtCore/QSet>

class QTimer;

class StreamsModel : public QAbstractItemModel
{
    Q_OBJECT

public:

    static const QLatin1String constDefaultCategoryIcon;

    struct Item
    {
        Item(const QString &n, const QString &i) : name(n), icon(i) { }
        virtual bool isCategory() = 0;
        virtual ~Item() { }
        QString name;
        QString icon;
    };

    struct CategoryItem;
    struct StreamItem : public Item
    {
        StreamItem(const QString &n, const QString &g, const QString &i, const QUrl &u, CategoryItem *p=0) : Item(n, i), genre(g), url(u), parent(p) { }
        bool isCategory() { return false; }
        QString genre;
        QUrl url;
        CategoryItem *parent;
    };

    struct CategoryItem : public Item
    {
        CategoryItem(const QString &n, const QString &i=QString()) : Item(n, i) { }
        virtual ~CategoryItem() { clearStreams(); }
        bool isCategory() { return true; }
        void clearStreams();
        QHash<QString, StreamItem *> itemMap;
        QList<StreamItem *> streams;
        QSet<QString> genres;
    };

    static QString prefixUrl(const QString &n, bool addPrefix=true);

    StreamsModel();
    ~StreamsModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex&) const { return 1; }
    bool hasChildren(const QModelIndex &parent) const;
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    void reload();
    void save(bool force=false);
    bool save(const QString &filename, const QSet<StreamsModel::Item *> &selection=QSet<StreamsModel::Item *>());
    bool import(const QString &filename) { return load(filename, false); }
    bool add(const QString &cat, const QString &name, const QString &genre, const QString &icon, const QString &url);
    void add(const QString &cat, const QList<StreamsModel::StreamItem *> &streams);
    void editCategory(const QModelIndex &index, const QString &name, const QString &icon);
    void editStream(const QModelIndex &index, const QString &oldCat, const QString &newCat, const QString &name, const QString &genre, const QString &icon, const QString &url);
    void remove(const QModelIndex &index);
    void removeCategory(CategoryItem *cat);
    void removeStream(StreamItem *stream);
    QString name(const QString &cat, const QString &url) { return name(getCategory(cat), url); }
    bool entryExists(const QString &cat, const QString &name, const QUrl &url=QUrl()) { return entryExists(getCategory(cat), name, url); }
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool validProtocol(const QString &file) const;
    QStringList filenames(const QModelIndexList &indexes, bool addPrefix) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    void mark(const QList<int> &rows, bool f);
    void updateGenres();

Q_SIGNALS:
    void updateGenres(const QSet<QString> &genres);

private:
    void clearCategories();
    bool load(const QString &filename, bool isInternal);
    CategoryItem * getCategory(const QString &name, bool create=false, bool signal=false);
    QString name(CategoryItem *cat, const QString &url);
    bool entryExists(CategoryItem *cat, const QString &name, const QUrl &url=QUrl());

private Q_SLOTS:
    void persist();

private:
    QList<CategoryItem *> items;
    bool modified;
    QTimer *timer;
};

#endif
