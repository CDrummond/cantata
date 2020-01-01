/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "config.h"
#include <QString>

#include "gui/apikeys.h"
#include "actionmodel.h"
#include "mpd-interface/stream.h"
#include "mpd-interface/playlist.h"
#include "support/icon.h"
#include <QList>
#include <QMap>
#include <QSet>
#include <QDateTime>

class NetworkJob;
class QNetworkRequest;
class QXmlStreamReader;
class QIODevice;

class StreamsModel : public ActionModel
{
    Q_OBJECT

public:
    struct CategoryItem;

    struct Item : public Stream
    {
        Item(const QString &u, const QString &n=QString(), CategoryItem *p=nullptr, const QString &sub=QString()) : Stream(u, n), subText(sub), parent(p) { }
        ~Item() override { }
        QString modifiedName() const;
        QString subText;
        CategoryItem *parent;
        virtual bool isCategory() const { return false; }
        CategoryItem *getTopLevelCategory() const;
        virtual QString fullUrl() const { return url; }
    };
    
    struct CategoryItem : public Item
    {
        enum State
        {
            Initial,
            Fetching,
            Fetched
        };

        CategoryItem(const QString &u, const QString &n=QString(), CategoryItem *p=nullptr, const QIcon &i=QIcon(),
                     const QString &cn=QString(), const QString &bn=QString(), bool modName=false)
            : Item(u, n, p), state(Initial), isAll(false), isBookmarks(false), supportsBookmarks(false),
              canBookmark(false), addCatToModifiedName(modName), icon(i), cacheName(cn),
              bookmarksName(bn), configName(cn.isEmpty() ? bn : cn) { }

        ~CategoryItem() override { qDeleteAll(children); }
        bool isCategory() const override { return true; }
        virtual bool isFavourites() const { return false; }
        virtual bool isBuiltIn() const { return true; }
        virtual bool isDi() const { return false; }
        virtual bool isSoma() const { return false; }
        virtual bool isListenLive() const { return false; }
        virtual void removeCache();
        bool isTopLevel() const { return parent && nullptr==parent->parent; }
        virtual bool canReload() const { return !cacheName.isEmpty() || isTopLevel() || !url.isEmpty(); }
        void removeBookmarks();
        void saveBookmarks();
        QList<Item *> loadBookmarks();
        CategoryItem * getBookmarksCategory();
        CategoryItem * createBookmarksCategory();
        void saveCache() const;
        virtual QList<Item *> loadCache();
        bool saveXml(const QString &fileName, bool format=false) const;
        bool saveXml(QIODevice *dev, bool format=false) const;
        QList<Item *> loadXml(const QString &fileName);
        virtual QList<Item *> loadXml(QIODevice *dev);
        virtual void addHeaders(QNetworkRequest &) { }
        virtual NetworkJob * fetchSecondardyUrl() { return nullptr; }

        State state;
        bool isAll : 1;
        bool isBookmarks : 1; // 'Virtual' bookmarks category...
        bool supportsBookmarks : 1; // Intended for top-level items, indicates if bookmarks can be added
        bool canBookmark : 1; // Can this category be bookmark'ed in top-level parent? can have the cache
        bool addCatToModifiedName : 1; // When adding to playqueue/favourites, should name contian category?
        QList<Item *> children;
        QIcon icon;
        QString cacheName;
        QString bookmarksName;
        QString configName;
    };

    struct FavouritesCategoryItem : public CategoryItem
    {
        FavouritesCategoryItem(const QString &u, const QString &n, CategoryItem *p, const QIcon &i)
            : CategoryItem(u, n, p, i), importedOld(false) { }
        QList<Item *> loadXml(QIODevice *dev) override;
        bool isFavourites() const override { return true; }
        bool canReload() const override { return false; }
        bool importedOld;
    };

    struct IceCastCategoryItem : public CategoryItem
    {
        IceCastCategoryItem(const QString &u, const QString &n=QString(), CategoryItem *p=nullptr, const QIcon &i=QIcon(),
                            const QString &cn=QString(), const QString &bn=QString())
            : CategoryItem(u, n, p, i, cn, bn) { }
        void addHeaders(QNetworkRequest &req) override;
    };

    struct ShoutCastCategoryItem : public CategoryItem
    {
        ShoutCastCategoryItem(const QString &u, const QString &n=QString(), CategoryItem *p=nullptr, const QIcon &i=QIcon(),
                              const QString &cn=QString(), const QString &bn=QString())
            : CategoryItem(u, n, p, i, cn, bn) { }
        void addHeaders(QNetworkRequest &req) override;
        NetworkJob * fetchSecondardyUrl() override;
        QString fullUrl() const override { return ApiKeys::self()->addKey(url, ApiKeys::ShoutCast); }
    };

    struct DirbleCategoryItem : public CategoryItem
    {
        DirbleCategoryItem(const QString &u, const QString &n=QString(), CategoryItem *p=nullptr, const QIcon &i=QIcon(),
                              const QString &cn=QString(), const QString &bn=QString())
            : CategoryItem(u, n, p, i, cn, bn) { }
        QString fullUrl() const override { return ApiKeys::self()->addKey(url, ApiKeys::Dirble); }
    };

    struct ListenLiveCategoryItem : public CategoryItem
    {
        ListenLiveCategoryItem(const QString &u, const QString &n, CategoryItem *p, const QIcon &i, const QString &cn)
            : CategoryItem(u, n, p, i, cn) { }
        bool isListenLive() const override { return true; }
        bool isBuiltIn() const override { return false; }
    };

    struct DiCategoryItem : public CategoryItem
    {
        DiCategoryItem(const QString &u, const QString &n, CategoryItem *p, const QIcon &i, const QString &cn)
            : CategoryItem(u, n, p, i, cn, QString(), true) { }
        void addHeaders(QNetworkRequest &req) override;
        bool isDi() const override { return true; }
        bool isBuiltIn() const override { return false; }
    };

    struct SomaCategoryItem : public CategoryItem
    {
        SomaCategoryItem(const QString &u, const QString &n, CategoryItem *p, const QIcon &i, const QString &cn, bool mod)
            : CategoryItem(u, n, p, i, cn, QString(), mod) { }
        bool isSoma() const override { return true; }
        bool isBuiltIn() const override { return false; }
    };

    struct XmlCategoryItem : public CategoryItem
    {
        XmlCategoryItem(const QString &n, CategoryItem *p, const QIcon &i, const QString &cn)
            : CategoryItem(QLatin1String("-"), n, p, i, cn, QString(), true) { }
        QList<Item *> loadCache() override;
        bool canReload() const override { return false; }
        void removeCache() override { }
        bool isBuiltIn() const override { return false; }
    };

    struct Category
    {
        Category(const QString &n, const QIcon &i, const QString &k, bool b, bool h, bool c)
            : name(n), icon(i), key(k), builtin(b), hidden(h), configurable(c) { }
        QString name;
        QIcon icon;
        QString key;
        bool builtin;
        bool hidden;
        bool configurable;
    };

    static const QString constPrefix;
    static const QString constSubDir;
    static const QString constCacheExt;
    static const QString constShoutCastHost;
    static const QString constDirbleHost;
    static const QString constCommunityHost;
    static const QString constCompressedXmlFile;
    static const QString constXmlFile;
    static const QString constSettingsFile;
    static const QString constPngIcon;
    static const QString constSvgIcon;
    static const QStringList constInstallFiles;

    static StreamsModel * self();
    static QString favouritesDir();
    static QString modifyUrl(const QString &u,  bool addPrefix=true, const QString &name=QString());
//    static bool validProtocol(const QString &file);

    StreamsModel(QObject *parent = nullptr);
    ~StreamsModel() override;
    QString name() const;
    QString title() const;
    QString descr() const;
    const QIcon & icon() const { return icn; }
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &, int) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool hasChildren(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &index) const override;
    void fetchMore(const QModelIndex &index) override;
    void reload(const QModelIndex &index);

    bool exportFavourites(const QString &fileName);
    void importIntoFavourites(const QString &fileName);
    bool haveFavourites() const { return !favourites->children.isEmpty(); }
    void removeFromFavourites(const QModelIndexList &indexes);
    bool addToFavourites(const QString &url, const QString &name);
    QString favouritesNameForUrl(const QString &u);
    bool nameExistsInFavourites(const QString &n);
    void updateFavouriteStream(const QString &url, const QString &name, const QModelIndex &idx);
    
    bool addBookmark(const QString &url, const QString &name, CategoryItem *bookmarkParentCat);
    void removeBookmark(const QModelIndex &index);
    void removeAllBookmarks(const QModelIndex &index);
    
    QModelIndex favouritesIndex() const;
    const QIcon & favouritesIcon() const { return favourites->icon; }
    bool isTuneIn(const CategoryItem *cat) const { return tuneIn==cat; }
    bool isShoutCast(const CategoryItem *cat) const { return shoutCast==cat; }
    bool isDirble(const CategoryItem *cat) const { return dirble==cat; }
    CategoryItem * tuneInCat() const { return tuneIn; }

    QStringList filenames(const QModelIndexList &indexes, bool addPrefix) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;

    Action *addBookmarkAct() { return addBookmarkAction; }
    Action *addToFavouritesAct() { return addToFavouritesAction; }
    Action *configureDiAct() { return configureDiAction; }
    Action *reloadAct() { return reloadAction; }

    void save();
    QList<Category> getCategories() const;
    void setHiddenCategories(const QSet<QString> &cats);
    CategoryItem * addInstalledProvider(const QString &name, const QIcon &icon, const QString &streamsFileName, bool replace);
    void removeInstalledProvider(const QString &key);

    QModelIndex categoryIndex(const CategoryItem *cat) const;

Q_SIGNALS:
    void loading();
    void loaded();
    void error(const QString &msg);
    void categoriesChanged();

    // Streams stored on MPD...
    void listFavouriteStreams();
    void saveFavouriteStream(const QString &url, const QString &name);
    void removeFavouriteStreams(const QList<quint32> &positions);
    void editFavouriteStream(const QString &url, const QString &name, quint32 position);
    void favouritesLoaded();
    void addedToFavourites(const QString &name);

public:
    static QList<Item *> parseRadioTimeResponse(QIODevice *dev, CategoryItem *cat, bool parseSubText=false);
    static QList<Item *> parseIceCastResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseSomaFmResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseDigitallyImportedResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseListenLiveResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseShoutCastSearchResponse(QIODevice *dev, CategoryItem *cat);
    QList<Item *> parseShoutCastResponse(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseShoutCastLinks(QXmlStreamReader &doc, CategoryItem *cat);
    static QList<Item *> parseShoutCastStations(QXmlStreamReader &doc, CategoryItem *cat);
    QList<Item *> parseDirbleResponse(QIODevice *dev, CategoryItem *cat, const QString &origUrl);
    static QList<Item *> parseDirbleStations(QIODevice *dev, CategoryItem *cat);
    static QList<Item *> parseCommunityStations(QIODevice *dev, CategoryItem *cat);
    static Item * parseRadioTimeEntry(QXmlStreamReader &doc, CategoryItem *parent, bool parseSubText=false);
    static Item * parseSomaFmEntry(QXmlStreamReader &doc, CategoryItem *parent);

private Q_SLOTS:
    void jobFinished();

    // Responses from MPD...
    void savedFavouriteStream(const QString &url, const QString &name);
    void removedFavouriteStreams(const QList<quint32> &removed);
//    void editedFavouriteStream(const QString &url, const QString &name, quint32 position);
    void favouriteStreams(const QList<Stream> &streams);
    void mpdConnectionState(bool c);

private:
    bool loadCache(CategoryItem *cat);
    Item * toItem(const QModelIndex &index) const { return index.isValid() ? static_cast<Item*>(index.internalPointer()) : root; }
    void loadInstalledProviders();

private:
    QMap<NetworkJob *, CategoryItem *> jobs;
    CategoryItem *root;
    FavouritesCategoryItem *favourites;
    CategoryItem *tuneIn;
    CategoryItem *shoutCast;
    CategoryItem *dirble;
    Action *addBookmarkAction;
    Action *addToFavouritesAction;
    Action *configureDiAction;
    Action *reloadAction;
    QList<Item *> hiddenCategories;
    QIcon icn;
};

#endif
