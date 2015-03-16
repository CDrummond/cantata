/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ULTIMATELYRICSPROVIDER_H
#define ULTIMATELYRICSPROVIDER_H

#include "mpd-interface/song.h"
#include <QObject>
#include <QPair>
#include <QStringList>
#include <QHash>
#include <QMap>

class NetworkJob;

class UltimateLyricsProvider : public QObject {
    Q_OBJECT

public:
    static void enableDebug();

    UltimateLyricsProvider();
    virtual ~UltimateLyricsProvider();

    typedef QPair<QString, QString> RuleItem;
    typedef QList<RuleItem> Rule;
    typedef QPair<QString, QString> UrlFormat;

    void setName(const QString &n) { name = n; }
    void setUrl(const QString &u) { url = u; }
    void setCharset(const QString &c) { charset = c; }
    void setRelevance(int r) { relevance = r; }
    void addUrlFormat(const QString &replace, const QString &with) { urlFormats << UrlFormat(replace, with); }
    void addExtractRule(const Rule &rule) { extractRules << rule; }
    void addExcludeRule(const Rule &rule) { excludeRules << rule; }
    void addInvalidIndicator(const QString &indicator) { invalidIndicators << indicator; }
    QString getName() const { return name; }
    QString displayName() const;
    int getRelevance() const { return relevance; }
    void fetchInfo(int id, const Song &metadata);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e) { enabled = e; }
    void abort();

Q_SIGNALS:
    void lyricsReady(int id, const QString &data);

private Q_SLOTS:
    void wikiMediaSearchResponse();
    void wikiMediaLyricsFetched();
    void lyricsFetched();

private:
    QString doTagReplace(QString str, const Song &song, bool doAll=true);
    void doUrlReplace(const QString &tag, const QString &value, QString &u) const;

private:
    bool enabled;
    QHash<NetworkJob *, int> requests;
    QMap<int, Song> songs;
    QString name;
    QString url;
    QString charset;
    int relevance;
    QList<UrlFormat> urlFormats;
    QList<Rule> extractRules;
    QList<Rule> excludeRules;
    QStringList invalidIndicators;
};

#endif // ULTIMATELYRICSPROVIDER_H
