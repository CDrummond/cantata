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

#include "lastfmengine.h"
#include "networkaccessmanager.h"
#include "localize.h"
#include "covers.h"
#include <QNetworkReply>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QXmlStreamReader>
#include <QRegExp>
#include <QDebug>

//#define DBUG qWarning() << metaObject()->className() << __FUNCTION__

#ifndef DBUG
#define DBUG qDebug()
#endif

const QLatin1String LastFmEngine::constLang("lastfm");
const QLatin1String LastFmEngine::constLinkPlaceholder("XXX_CONTEXT_READ_MORE_ON_LASTFM_XXX");

static const char * constModeProperty="mode";
static const char * constQuery="query";
static const char * constRedirectsProperty="redirects";
static const int constMaxRedirects=3;

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
    text=text.replace(constLinkPlaceholder, i18n("Read more on last.fm"));
    return text;
}

void LastFmEngine::search(const QStringList &query, Mode mode)
{
    QStringList fixedQuery=fixQuery(query);
    QUrl url("https://ws.audioscrobbler.com/2.0/");
    #if QT_VERSION < 0x050000
    QUrl &urlQuery=url;
    #else
    QUrlQuery urlQuery;
    #endif

    urlQuery.addQueryItem("method", Artist==mode ? "artist.getInfo" : "album.getInfo");
    urlQuery.addQueryItem("api_key", Covers::constLastFmApiKey);
    urlQuery.addQueryItem("autocorrect", "1");
    urlQuery.addQueryItem("artist", Covers::fixArtist(fixedQuery.at(0)));
    if (Album==mode) {
        urlQuery.addQueryItem("album", fixedQuery.at(1));
    }
    #if QT_VERSION >= 0x050000
    url.setQuery(urlQuery);
    #endif

    job=NetworkAccessManager::self()->get(url);
    job->setProperty(constModeProperty, (int)mode);
    job->setProperty(constRedirectsProperty, 0);
    
    QStringList queryString;
    foreach (QString q, fixedQuery) {
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
    QNetworkReply *reply = getReply(sender());
    if (!reply) {
        return;
    }

    QVariant redirect = reply->header(QNetworkRequest::LocationHeader);
    int numRirects=reply->property(constRedirectsProperty).toInt();
    if (redirect.isValid() && ++numRirects<constMaxRedirects) {
        job=NetworkAccessManager::self()->get(redirect.toString());
        job->setProperty(constRedirectsProperty, numRirects);
        job->setProperty(constModeProperty, reply->property(constModeProperty));
        DBUG <<  "Redirect" << redirect.toString();
        connect(job, SIGNAL(finished()), this, SLOT(parseResponse()));
        return;
    }

    QByteArray data=reply->readAll();
    if (QNetworkReply::NoError!=reply->error() || data.isEmpty()) {
        DBUG <<  "Empty/error";
        emit searchResult(QString(), QString());
        return;
    }

    Mode mode=(Mode)reply->property(constModeProperty).toInt();
    QString text=Artist==mode ? parseArtistResponse(data) : parseAlbumResponse(data);
    if (!text.isEmpty()) {
        static const QRegExp constLicense("User-contributed text is available.*");
        text.remove(constLicense);
        text.replace("\n", "<br>");
        text=text.simplified();
        text.replace(" <br>", "<br>");

        // Remove last.fm read more link (as we add our own!!)
        int end=text.lastIndexOf(QLatin1String("on Last.fm</a>"));
        if (-1!=end) {
            int start=text.lastIndexOf(QLatin1String("<a href=\"http://www.last.fm/music/"), end);
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

QString LastFmEngine::parseArtistResponse(const QByteArray &data)
{
    QXmlStreamReader xml(data);
    while (xml.readNextStartElement()) {
        if (QLatin1String("artist")==xml.name()) {
            while (xml.readNextStartElement()) {
                if (QLatin1String("bio")==xml.name()) {
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

QString LastFmEngine::parseAlbumResponse(const QByteArray &data)
{
    QXmlStreamReader xml(data);
    while (xml.readNextStartElement()) {
        if (QLatin1String("album")==xml.name()) {
            while (xml.readNextStartElement()) {
                if (QLatin1String("wiki")==xml.name()) {
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
