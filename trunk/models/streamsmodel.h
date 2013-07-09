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
#include <QIcon>
#include <QList>
#include <QMap>
#include <QDateTime>

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

        CategoryItem(const QString &u, const QString &n=QString(), CategoryItem *p=0, const QIcon &i=QIcon(), const QString &cn=QString())
            : Item(u, n, p), state(Initial), isFavourites(false), isAll(false), childrenHaveCache(false), icon(i), cacheName(cn) { }
        virtual ~CategoryItem() { qDeleteAll(children); }
        virtual bool isCategory() const { return true; }
        void removeCache();
        void saveCache() const;
        QList<Item *> loadCache();
        bool saveXml(const QString &fileName, bool format=false) const;
        bool saveXml(QIODevice *dev, bool format=false) const;
        QList<Item *> loadXml(const QString &fileName, bool importing=false);
        QList<Item *> loadXml(QIODevice *dev, bool importing=false);

        State state;
        bool isFavourites : 1;
        bool isAll : 1;
        bool childrenHaveCache : 1; // Only used for ListenLive - as each sub-category can have the cache
        QList<Item *> children;
        QIcon icon;
        QString cacheName;
    };

    static const QString constPrefix;
    static const QString constCacheDir;
    static const QString constCacheExt;

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
    void reload(const QModelIndex &index);

    void saveFavourites(bool force=false);
    bool exportFavourites(const QString &fileName);
    bool importIntoFavourites(const QString &fileName) { return loadFavourites(fileName, favouritesIndex(), true); }
    bool haveFavourites() const { return !favourites->children.isEmpty(); }
    bool isFavoritesWritable() { return favouritesIsWriteable; }
    bool checkFavouritesWritable();
    void reloadFavourites() { reload(favouritesIndex()); }
    void removeFromFavourites(const QModelIndex &index);
    void addToFavourites(const QString &url, const QString &name);
    QString favouritesNameForUrl(const QString &u);
    bool nameExistsInFavourites(const QString &n);
    void updateFavouriteStream(Item *item);
    QModelIndex favouritesIndex() const;
    const QIcon & favouritesIcon() const { return favourites->icon; }
    const QIcon & tuneInIcon() const { return tuneIn->icon; }

    QStringList filenames(const QModelIndexList &indexes, bool addPrefix) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    QStringList mimeTypes() const;

    bool isTopLevel(const CategoryItem *cat) const { return cat && root==cat->parent; }

Q_SIGNALS:
    void loading();
    void loaded();
    void error(const QString &msg);

public:
    static QList<Item *> parseRadioTimeResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseIceCastResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseSomaFmResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseDigitallyImportedResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseListenLiveResponse(QIODevice *dev, CategoryItem *cat);
    QList<Item *> parseShoutCastResponse(QIODevice *dev, CategoryItem *cat, const QString &origUrl);
    QList<Item *> parseShoutCastLinks(QXmlStreamReader &doc, CategoryItem *cat);
    QList<Item *> parseShoutCastStations(QXmlStreamReader &doc, CategoryItem *cat);
    static Item * parseRadioTimeEntry(QXmlStreamReader &doc, CategoryItem *parent);
    static Item * parseSomaFmEntry(QXmlStreamReader &doc, CategoryItem *parent);

private Q_SLOTS:
    void jobFinished();
    void persistFavourites();

private:
    bool loadCache(CategoryItem *cat);
    Item * toItem(const QModelIndex &index) const { return index.isValid() ? static_cast<Item*>(index.internalPointer()) : root; }
    bool loadFavourites(const QString &fileName, const QModelIndex &index, bool importing=false);
    void buildListenLive();

private:
    QMap<QNetworkReply *, CategoryItem *> jobs;
    CategoryItem *root;
    CategoryItem *favourites;
    CategoryItem *tuneIn;
    CategoryItem *listenLive;
    bool favouritesIsWriteable;
    bool favouritesModified;
    QTimer *favouritesSaveTimer;
};

#endif
