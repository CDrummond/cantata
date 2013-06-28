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

#include "actionmodel.h"
#include <QList>
#include <QMap>
#include <QIcon>

class QNetworkReply;
class QXmlStreamReader;
class QIODevice;
class QTimer;

class StreamsModel : public ActionModel
{
    Q_OBJECT

public:
    struct CategoryItem;
    struct Item
    {
        Item(const QString &u, const QString &n=QString(), CategoryItem *p=0) : url(u), name(n), parent(p) { }
        virtual ~Item() { }
        QString url;
        QString name;
        CategoryItem *parent;
        virtual bool isCategory() const { return false; }
    };
    
    struct CategoryItem : public Item
    {
        enum State
        {
            Initial,
            Fetching,
            Fetched
        };

        CategoryItem(const QString &u, const QString &n=QString(), CategoryItem *p=0, const QIcon &i=QIcon())
            : Item(u, n, p), state(Initial), isFavourites(false), icon(i) { }
        virtual ~CategoryItem() { qDeleteAll(children); }
        virtual bool isCategory() const { return true; }
        State state;
        bool isFavourites;
        QList<Item *> children;
        QIcon icon;
    };
    
    static const QString constPrefix;

    static StreamsModel * self();
    static QString favouritesDir();
    static QString modifyUrl(const QString &u,  bool addPrefix=true, const QString &name=QString());
    static bool validProtocol(const QString &file);

    StreamsModel(QObject *parent = 0);
    ~StreamsModel();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool hasChildren(const QModelIndex &index) const;
    bool canFetchMore(const QModelIndex &index) const;
    void fetchMore(const QModelIndex &index);

    void saveFavourites(bool force=false);
    bool haveFavourites() const { return !favourites->children.isEmpty(); }
    bool isFavoritesWritable() { return favouritesIsWriteable; }
    bool checkFavouritesWritable();
    void reloadFavourites();
    void removeFromFavourites(const QModelIndex &index);
    void addToFavourites(const QString &url, const QString &name);
    QString favouritesNameForUrl(const QString &u);
    bool nameExistsInFavourites(const QString &n);
    void updateFavouriteStream(Item *item);

    bool importXml(const QString &fileName);
    bool saveXml(const QString &fileName, const QList<Item *> &items);

    QStringList filenames(const QModelIndexList &indexes, bool addPrefix) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    QStringList mimeTypes() const;

Q_SIGNALS:
    void loading();
    void loaded();
    void error(const QString &msg);

private Q_SLOTS:
    void jobFinished();
    void persistFavourites();

private:
    Item * toItem(const QModelIndex &index) const { return index.isValid() ? static_cast<Item*>(index.internalPointer()) : root; }
    QList<Item *> parseRadioTimeResponse(QIODevice *dev, CategoryItem *cat);
    QList<Item *> parseIceCastResponse(QIODevice *dev, CategoryItem *cat);
    QList<Item *> parseSomaFmResponse(QIODevice *dev, CategoryItem *cat);
    QList<Item *> parseDigitallyImportedResponse(QIODevice *dev, CategoryItem *cat, const QString &origUrl);
    Item * parseRadioTimeEntry(QXmlStreamReader &doc, CategoryItem *parent);
    Item * parseIceCastEntry(QXmlStreamReader &doc, CategoryItem *parent, QSet<QString> &names);
    Item * parseSomaFmEntry(QXmlStreamReader &doc, CategoryItem *parent);
    void loadFavourites(const QModelIndex &index);
    bool loadXml(const QString &fileName, const QModelIndex &index);
    QList<Item *> loadXml(QIODevice *dev, bool isInternal);
    bool saveXml(QIODevice *dev, const QList<Item *> &items, bool format) const;

private:
    QMap<QNetworkReply *, CategoryItem *> jobs;
    CategoryItem *root;
    CategoryItem *favourites;
    bool favouritesIsWriteable;
    bool favouritesModified;
    QTimer *favouritesSaveTimer;
};

#endif
