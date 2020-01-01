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

#include "lastfmengine.h"
#include "network/networkaccessmanager.h"
#include "gui/covers.h"
#include "gui/apikeys.h"
#include "config.h"
#include <QUrlQuery>
#include <QXmlStreamReader>
#include <QRegExp>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void LastFmEngine::enableDebug()
{
    debugEnabled=true;
}

const QLatin1String LastFmEngine::constLang("lastfm");
const QLatin1String LastFmEngine::constLinkPlaceholder("XXX_CONTEXT_READ_MORE_ON_LASTFM_XXX");

static const char * constModeProperty="mode";
static const char * constQuery="query";

LastFmEngine::LastFmEngine(QObject *p)
    : ContextEngine(p)
{
}

QStringList LastFmEngine::getLangs() const
{
    QStringList langs;
    langs.append(constLang);
    return langs;
}

QString LastFmEngine::translateLinks(QString text) const
{
    text=text.replace(constLinkPlaceholder, tr("Read more on last.fm"));
    return text;
}

void LastFmEngine::search(const QStringList &query, Mode mode)
{
    QStringList fixedQuery=fixQuery(query);
    QUrl url("https://ws.audioscrobbler.com/2.0/");
    QUrlQuery urlQuery;

    switch (mode) {
    case Artist:
        urlQuery.addQueryItem("method", "artist.getInfo");
        break;
    case Album:
        urlQuery.addQueryItem("method", "album.getInfo");
        urlQuery.addQueryItem("album", fixedQuery.at(1));
        break;
    case Track:
        urlQuery.addQueryItem("method", "track.getInfo");
        urlQuery.addQueryItem("track", fixedQuery.at(1));
        break;
    }

    ApiKeys::self()->addKey(urlQuery, ApiKeys::LastFm);
    urlQuery.addQueryItem("autocorrect", "1");
    urlQuery.addQueryItem("artist", Covers::fixArtist(fixedQuery.at(0)));

    url.setQuery(urlQuery);

    job=NetworkAccessManager::self()->get(url);
    job->setProperty(constModeProperty, (int)mode);
    
    QStringList queryString;
    for (QString q: fixedQuery) {
        q=q.replace("/", "%2F");
        q=q.replace(" ", "+");
        queryString.append(q);
    }
    job->setProperty(constQuery, queryString.join("/"));
    DBUG <<  url.toString();
    connect(job, SIGNAL(finished()), this, SLOT(parseResponse()));
}

void LastFmEngine::parseResponse()
{
    DBUG << __FUNCTION__;
    NetworkJob *reply = getReply(sender());
    if (!reply) {
        return;
    }

    QByteArray data=reply->readAll();
    if (!reply->ok() || data.isEmpty()) {
        DBUG <<  "Empty/error";
        emit searchResult(QString(), QString());
        return;
    }

    Mode mode=(Mode)reply->property(constModeProperty).toInt();
    QString text;

    switch (mode) {
    case Artist:
        text=parseResponse(data, QLatin1String("artist"), QLatin1String("bio"));
        break;
    case Album:
        text=parseResponse(data, QLatin1String("album"), QLatin1String("wiki"));
        break;
    case Track:
        text=parseResponse(data, QLatin1String("track"), QLatin1String("wiki"));
        break;
    }

    if (!text.isEmpty()) {
        static const QRegExp constLicense("User-contributed text is available.*");
        text.remove(constLicense);
        text.replace("\n", "<br>");
        text=text.simplified();
        text.replace(" <br>", "<br>");

        // Remove last.fm read more link (as we add our own!!)
        int end=text.lastIndexOf(QLatin1String("on Last.fm</a>"));
        if (-1!=end) {
            int start=text.lastIndexOf(QLatin1String("<a href=\"https://www.last.fm/music/"), end);
            if (-1==start) {
                start=text.lastIndexOf(QLatin1String("<a href=\"http://www.last.fm/music/"), end);
            }
            if (-1!=start) {
                if (text.indexOf(QLatin1String("Read more about"), start)<end) {
                    text=text.left(start);
                }
            }
        }
        text += QLatin1String("<br><br><a href='http://www.last.fm/music/")+reply->property(constQuery).toString()+QLatin1String("/+wiki'>")+constLinkPlaceholder+QLatin1String("</a>");
        text.replace("<br><br><br><br><br>", "<br><br>");
        text.replace("<br><br><br><br>", "<br><br>");
        text.replace("<br><br><br>", "<br><br>");
    }
    emit searchResult(text, text.isEmpty() ? QString() : constLang);
}

QString LastFmEngine::parseResponse(const QByteArray &data, const QString &firstTag, const QString &secondTag)
{
    DBUG << __FUNCTION__ << data;
    QXmlStreamReader xml(data);
    xml.setNamespaceProcessing(false);
    while (xml.readNextStartElement()) {
        if (firstTag==xml.name()) {
            while (xml.readNextStartElement()) {
                if (secondTag==xml.name()) {
                    while (xml.readNextStartElement()) {
                        if (QLatin1String("content")==xml.name()) {
                            return xml.readElementText().trimmed();
                        } else {
                            xml.skipCurrentElement();
                        }
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        }
    }

    return QString();
}

#include "moc_lastfmengine.cpp"
