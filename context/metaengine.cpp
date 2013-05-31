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

#include "metaengine.h"
#include "wikipediaengine.h"
#include "lastfmengine.h"
#include <QDebug>

//#define DBUG qWarning() << "MetaEngine"

#ifndef DBUG
#define DBUG qDebug()
#endif

static const QLatin1String constBlankResp("-");

MetaEngine::MetaEngine(QObject *p)
    : ContextEngine(p)
{
    wiki=new WikipediaEngine(this);
    lastfm=new LastFmEngine(this);
    connect(wiki, SIGNAL(searchResult(QString,QString)), SLOT(wikiResponse(QString,QString)));
    connect(lastfm, SIGNAL(searchResult(QString,QString)), SLOT(lastFmResponse(QString,QString)));
}

QStringList MetaEngine::getLangs() const
{
    QStringList langs=wiki->getLangs();
    langs.append(LastFmEngine::constLang);
    return langs;
}

QString MetaEngine::getPrefix(const QString &key) const
{
    return key==LastFmEngine::constLang ? LastFmEngine::constLang : wiki->getPrefix(key);
}

QString MetaEngine::translateLinks(QString text) const
{
    return lastfm->translateLinks(wiki->translateLinks(text));
}

void MetaEngine::search(const QStringList &query, Mode mode)
{
    DBUG << __FUNCTION__ << query << (int)mode;
    responses.clear();
    wiki->cancel();
    lastfm->cancel();
    wiki->search(query, mode);
    lastfm->search(query, mode);
}

void MetaEngine::wikiResponse(const QString &html, const QString &lang)
{
    DBUG << __FUNCTION__ << html.isEmpty() << lang.isEmpty();
    if (!html.isEmpty()) {
        // Got a wikipedia reponse, use it!
        DBUG << __FUNCTION__ << "Got wiki response!";
        lastfm->cancel();
        emit searchResult(html, lang);
        responses.clear();
    } else if (responses[LastFm].html.isEmpty()) {
        DBUG << __FUNCTION__ << "Wiki empty, but not received last.fm";
        // Wikipedia response is empty, but have not received last.fm reply yet.
        // So, indicate that Wikipedia was empty - and wait for last.fm
        responses[Wiki]=Response(constBlankResp, lang);
    } else if (constBlankResp==responses[LastFm].html) {
        DBUG << __FUNCTION__ << "Both responses empty";
        // Last.fm is empty as well :-(
        emit searchResult(QString(), QString());
        responses.clear();
    } else {
        // Got a last.fm response, use that!
        DBUG << __FUNCTION__ << "Wiki empty, last.fm not - so use last.fm";
        emit searchResult(responses[LastFm].html, responses[LastFm].lang);
        responses.clear();
    }
}

void MetaEngine::lastFmResponse(const QString &html, const QString &lang)
{
    DBUG << __FUNCTION__ << html.isEmpty() << lang.isEmpty();
    if (constBlankResp==responses[Wiki].html) {
        // Wikipedia failed, so use last.fm response...
        DBUG << __FUNCTION__ << "Wiki failed, so use last.fm";
        emit searchResult(html, lang);
        responses.clear();
    } else if (responses[Wiki].html.isEmpty()) {
        // No Wikipedia response yet, so save last.fm response...
        DBUG << __FUNCTION__ << "No wiki response, save last.fm";
        responses[LastFm]=Response(html.isEmpty() ? constBlankResp : html, lang);
    }
}
