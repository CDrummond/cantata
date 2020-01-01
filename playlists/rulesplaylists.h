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

#ifndef RULES_PLAYLISTS_H
#define RULES_PLAYLISTS_H

#include <QIcon>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include "models/actionmodel.h"

class RulesPlaylists : public ActionModel
{
    Q_OBJECT

public:
    enum Order {
        Order_AlbumArtist,
        Order_Artist,
        Order_Album,
        Order_Composer,
        Order_Date,
        Order_Genre,
        Order_Rating,
        Order_Title,
        Order_Age,
        Order_Random,

        Order_Count
    };

    static Order toOrder(const QString &str);
    static QString orderStr(Order order);
    static QString orderName(Order order);

    typedef QMap<QString, QString> Rule;
    struct Entry {
        Entry(const QString &n=QString()) : name(n) { }
        bool operator==(const Entry &o) const { return name==o.name; }
        bool haveRating() const { return ratingFrom>=0 && ratingTo>0; }
        QString name;
        QList<Rule> rules;
        bool includeUnrated = false;
        int ratingFrom = 0;
        int ratingTo = 0;
        int minDuration = 0;
        int maxDuration = 0;
        int maxAge = 0;
        int numTracks = 10;
        Order order = Order_Random;
        bool orderAscending = true;
    };

    static const QString constExtension;
    static const QString constRuleKey;
    static const QString constArtistKey;
    static const QString constSimilarArtistsKey;
    static const QString constAlbumArtistKey;
    static const QString constComposerKey;
    static const QString constCommentKey;
    static const QString constAlbumKey;
    static const QString constTitleKey;
    static const QString constGenreKey;
    static const QString constDateKey;
    static const QString constRatingKey;
    static const QString constIncludeUnratedKey;
    static const QString constDurationKey;
    static const QString constNumTracksKey;
    static const QString constMaxAgeKey;
    static const QString constFileKey;
    static const QString constExactKey;
    static const QString constExcludeKey;
    static const QString constOrderKey;
    static const QString constOrderAscendingKey;
    static const QChar constRangeSep;
    static const QChar constKeyValSep;

    RulesPlaylists(int icon, const QString &dir);
    ~RulesPlaylists() override { }

    virtual QString name() const =0;
    virtual QString title() const =0;
    virtual QString descr() const =0;
    virtual bool isDynamic() const { return false; }
    const QIcon & icon() const { return icn; }
    virtual bool isRemote() const { return false; }
    virtual int minTracks() const { return 10; }
    virtual int maxTracks() const { return 500; }
    virtual int defaultNumTracks() const { return 10; }
    virtual bool saveRemote(const QString &string, const Entry &e) { Q_UNUSED(string); Q_UNUSED(e); return false; }
    virtual void stop(bool sendClear=false) { Q_UNUSED(sendClear) }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex&) const override { return 1; }
    bool hasChildren(const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &, int) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Entry entry(const QString &e);
    Entry entry(int row) const { return row>=0 && row<entryList.count() ? entryList.at(row) : Entry(); }
    bool exists(const QString &e) { return entryList.end()!=find(e); }
    bool save(const Entry &e);
    virtual void del(const QString &name);
    QString current() const { return currentEntry; }
    const QList<Entry> & entries() const { return entryList; }

protected:
    QList<Entry>::Iterator find(const QString &e);
    void loadLocal();
    void updateEntry(const Entry &e);

protected:
    QIcon icn;
    QString rulesDir;
    QList<Entry> entryList;
    QString currentEntry;
};

#endif
