/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "ultimatelyricsprovider.h"
#include "networkaccessmanager.h"
#include <QTextCodec>
#include <QXmlStreamReader>
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "Lyrics" << __FUNCTION__
void UltimateLyricsProvider::enableDebug()
{
    debugEnabled=true;
}

static const QString constArtistArg=QLatin1String("{Artist}");
static const QString constArtistLowerArg=QLatin1String("{artist}");
static const QString constArtistLowerNoSpaceArg=QLatin1String("{artist2}");
static const QString constArtistFirstCharArg=QLatin1String("{a}");
static const QString constAlbumArg=QLatin1String("{Album}");
static const QString constAlbumLowerArg=QLatin1String("{album}");
static const QString constAlbumLowerNoSpaceArg=QLatin1String("{album2}");
static const QString constTitleLowerArg=QLatin1String("{title}");
static const QString constTitleArg=QLatin1String("{Title}");
static const QString constTitleCaseArg=QLatin1String("{Title2}");
static const QString constYearArg=QLatin1String("{year}");
static const QString constTrackNoArg=QLatin1String("{track}");

static QString noSpace(const QString &text)
{
    QString ret(text);
    ret.remove(' ');
    return ret;
}

static QString firstChar(const QString &text)
{
    return text.isEmpty() ? text : text[0].toLower();
}

static QString titleCase(const QString &text)
{
    if (0==text.length()) {
        return QString();
    }
    if (1==text.length()) {
        return text[0].toUpper();
    }
    return text[0].toUpper() + text.right(text.length() - 1).toLower();
}

static QString doTagReplace(QString str, const Song &song)
{
    if (str.contains(QLatin1Char('{'))) {
        QString artistFixed=song.basicArtist();
        str.replace(constArtistArg, artistFixed);
        str.replace(constArtistFirstCharArg, firstChar(artistFixed));
        str.replace(constAlbumArg, song.album);
        str.replace(constTitleArg, song.title);
        str.replace(constYearArg, QString::number(song.year));
        str.replace(constTrackNoArg, QString::number(song.track));
    }
    return str;
}

static QString extract(const QString &source, const QString &begin, const QString &end)
{
    DBUG << "Looking for" << begin << end;
    int beginIdx = source.indexOf(begin, 0, Qt::CaseInsensitive);
    if (-1==beginIdx) {
        DBUG << "Failed to find begin";
        return QString();
    }
    beginIdx += begin.length();

    int endIdx = source.indexOf(end, beginIdx, Qt::CaseInsensitive);
    if (-1==endIdx) {
        DBUG << "Failed to find end";
        return QString();
    }

    DBUG << "Found match";
    return source.mid(beginIdx, endIdx - beginIdx - 1);
}

static QString extractXmlTag(const QString &source, const QString &tag)
{
    DBUG << "Looking for" << tag;
    QRegExp re("<(\\w+).*>"); // ಠ_ಠ
    if (-1==re.indexIn(tag)) {
        DBUG << "Failed to find tag";
        return QString();
    }

    DBUG << "Found match";
    return extract(source, tag, "</" + re.cap(1) + ">");
}

static QString exclude(const QString &source, const QString &begin, const QString &end)
{
    int beginIdx = source.indexOf(begin, 0, Qt::CaseInsensitive);
    if (-1==beginIdx) {
        return source;
    }

    int endIdx = source.indexOf(end, beginIdx + begin.length(), Qt::CaseInsensitive);
    if (-1==endIdx) {
        return source;
    }

    return source.left(beginIdx) + source.right(source.length() - endIdx - end.length());
}

static QString excludeXmlTag(const QString &source, const QString &tag)
{
    QRegExp re("<(\\w+).*>"); // ಠ_ಠ
    if (-1==re.indexIn(tag)) {
        return source;
    }

    return exclude(source, tag, "</" + re.cap(1) + ">");
}

static void applyExtractRule(const UltimateLyricsProvider::Rule &rule, QString &content, const Song &song)
{
    foreach (const UltimateLyricsProvider::RuleItem &item, rule) {
        if (item.second.isNull()) {
            content = extractXmlTag(content, doTagReplace(item.first, song));
        } else {
            content = extract(content, doTagReplace(item.first, song), doTagReplace(item.second, song));
        }
    }
}

static void applyExcludeRule(const UltimateLyricsProvider::Rule &rule, QString &content, const Song &song)
{
    foreach (const UltimateLyricsProvider::RuleItem &item, rule) {
        if (item.second.isNull()) {
            content = excludeXmlTag(content, doTagReplace(item.first, song));
        } else {
            content = exclude(content, doTagReplace(item.first, song), doTagReplace(item.second, song));
        }
    }
}

UltimateLyricsProvider::UltimateLyricsProvider()
    : enabled(true)
    , relevance(0)
{
}

UltimateLyricsProvider::~UltimateLyricsProvider()
{
    abort();
}

void UltimateLyricsProvider::fetchInfo(int id, const Song &metadata)
{
    #if QT_VERSION < 0x050000
    const QTextCodec *codec = QTextCodec::codecForName(charset.toAscii().constData());
    #else
    const QTextCodec *codec = QTextCodec::codecForName(charset.toLatin1().constData());
    #endif
    if (!codec) {
        emit lyricsReady(id, QString());
        return;
    }

    QString artistFixed=metadata.basicArtist();
    QString urlText(url);

    songs.insert(id, metadata);
    if (QLatin1String("lyrics.wikia.com")==name) {
        QUrl url(urlText);
        #if QT_VERSION < 0x050000
        QUrl &query=url;
        #else
        QUrlQuery query;
        #endif

        query.addQueryItem(QLatin1String("artist"), artistFixed);
        query.addQueryItem(QLatin1String("song"), metadata.title);
        query.addQueryItem(QLatin1String("func"), QLatin1String("getSong"));
        query.addQueryItem(QLatin1String("fmt"), QLatin1String("xml"));
        #if QT_VERSION >= 0x050000
        url.setQuery(query);
        #endif

        NetworkJob *reply = NetworkAccessManager::self()->get(url);
        requests[reply] = id;
        connect(reply, SIGNAL(finished()), this, SLOT(wikiMediaSearchResponse()));
        return;
    }

    // Fill in fields in the URL
    bool urlContainsDetails=urlText.contains(QLatin1Char('{'));
    if (urlContainsDetails) {
        doUrlReplace(constArtistArg, artistFixed, urlText);
        doUrlReplace(constArtistLowerArg, artistFixed.toLower(), urlText);
        doUrlReplace(constArtistLowerNoSpaceArg, noSpace(artistFixed.toLower()), urlText);
        doUrlReplace(constArtistFirstCharArg, firstChar(artistFixed), urlText);
        doUrlReplace(constAlbumArg, metadata.album, urlText);
        doUrlReplace(constAlbumLowerArg, metadata.album.toLower(), urlText);
        doUrlReplace(constAlbumLowerNoSpaceArg, noSpace(metadata.album.toLower()), urlText);
        doUrlReplace(constTitleArg, metadata.title, urlText);
        doUrlReplace(constTitleLowerArg, metadata.title.toLower(), urlText);
        doUrlReplace(constTitleCaseArg, titleCase(metadata.title), urlText);
        doUrlReplace(constYearArg, QString::number(metadata.year), urlText);
        doUrlReplace(constTrackNoArg, QString::number(metadata.track), urlText);
    }

    // For some reason Qt messes up the ? -> %3F and & -> %26 conversions - by placing 25 after the %
    // So, try and revert this...
    QUrl url(urlText);

    if (urlContainsDetails) {
        QByteArray data=url.toEncoded();
        data.replace("%253F", "%3F");
        data.replace("%253f", "%3f");
        data.replace("%2526", "%26");
        url=QUrl::fromEncoded(data, QUrl::StrictMode);
    }

    NetworkJob *reply = NetworkAccessManager::self()->get(url);
    requests[reply] = id;
    connect(reply, SIGNAL(finished()), this, SLOT(lyricsFetched()));
}

void UltimateLyricsProvider::abort()
{
    QHash<NetworkJob *, int>::ConstIterator it(requests.constBegin());
    QHash<NetworkJob *, int>::ConstIterator end(requests.constEnd());

    for (; it!=end; ++it) {
        disconnect(it.key(), SIGNAL(finished()), this, SLOT(lyricsFetched()));
        disconnect(it.key(), SIGNAL(finished()), this, SLOT(wikiMediaSearchResponse()));
        it.key()->deleteLater();
        it.key()->close();
    }
    requests.clear();
    songs.clear();
}

void UltimateLyricsProvider::wikiMediaSearchResponse()
{
    NetworkJob *reply = qobject_cast<NetworkJob*>(sender());
    if (!reply) {
        return;
    }

    int id = requests.take(reply);
    reply->deleteLater();

    if (!reply->ok()) {
        songs.remove(id);
        emit lyricsReady(id, QString());
        return;
    }

    QUrl url;
    QXmlStreamReader doc(reply->actualJob());
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("url")==doc.name()) {
            QString lyricsUrl=doc.readElementText();
            if (!lyricsUrl.contains(QLatin1String("action=edit"))) {
                url=QUrl::fromEncoded(lyricsUrl.toUtf8()).toString();
            }
            break;
        }
    }

    if (url.isValid()) {
        NetworkJob *reply = NetworkAccessManager::self()->get(url);
        requests[reply] = id;
        connect(reply, SIGNAL(finished()), this, SLOT(lyricsFetched()));
    } else {
        songs.remove(id);
        emit lyricsReady(id, QString());
    }
}

void UltimateLyricsProvider::lyricsFetched()
{
    NetworkJob *reply = qobject_cast<NetworkJob*>(sender());
    if (!reply) {
        return;
    }

    int id = requests.take(reply);
    reply->deleteLater();
    Song song=songs.take(id);

    if (!reply->ok()) {
        //emit Finished(id);
        emit lyricsReady(id, QString());
        return;
    }

    #if QT_VERSION < 0x050000
    const QTextCodec *codec = QTextCodec::codecForName(charset.toAscii().constData());
    #else
    const QTextCodec *codec = QTextCodec::codecForName(charset.toLatin1().constData());
    #endif
    const QString originalContent = codec->toUnicode(reply->readAll()).replace("<br />", "<br/>");
    DBUG << name << "response" << originalContent;
    // Check for invalid indicators
    foreach (const QString &indicator, invalidIndicators) {
        if (originalContent.contains(indicator)) {
            //emit Finished(id);
            emit lyricsReady(id, QString());
            return;
        }
    }

    QString lyrics;

    // Apply extract rules
    foreach (const Rule& rule, extractRules) {
        QString content = originalContent;
        applyExtractRule(rule, content, song);

        if (!content.isEmpty()) {
            lyrics = content;
        }
    }

    // Apply exclude rules
    foreach (const Rule& rule, excludeRules) {
        applyExcludeRule(rule, lyrics, song);
    }

    lyrics=lyrics.trimmed();
    lyrics.replace("<br/>\n", "<br/>");
    lyrics.replace("<br>\n", "<br/>");
    emit lyricsReady(id, lyrics);
}

void UltimateLyricsProvider::doUrlReplace(const QString &tag, const QString &value, QString &u) const
{
    if (!u.contains(tag)) {
        return;
    }

    // Apply URL character replacement
    QString valueCopy(value);
    foreach (const UltimateLyricsProvider::UrlFormat& format, urlFormats) {
        QRegExp re("[" + QRegExp::escape(format.first) + "]");
        valueCopy.replace(re, format.second);
    }
    // ? and & should ony be used in URL queries...
    valueCopy.replace(QLatin1Char('&'), QLatin1String("%26"));
    valueCopy.replace(QLatin1Char('?'), QLatin1String("%3f"));
    u.replace(tag, valueCopy, Qt::CaseInsensitive);
}
