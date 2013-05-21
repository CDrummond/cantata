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

#include "wikipediaengine.h"
#include "networkaccessmanager.h"
#include "localize.h"
#include "settings.h"
#include <QNetworkReply>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QXmlStreamReader>
#include <QRegExp>
#include <QDebug>

static const char * constModeProperty="mode";
static const char * constLangProperty="lang";
static const char * constRedirectsProperty="redirects";
static const char * constQueryProperty="query";
static const int constMaxRedirects=3;

static QString strip(const QString &string, QString open, QString close, QString inner=QString())
{
    QString result;
    int next, /*lastLeft, */left = 0;
    int pos = string.indexOf(open);

    if (pos < 0) {
        return string;
    }

    if (inner.isEmpty()) {
        inner = open;
    }

    while (pos > -1) {
        result += string.mid(left, pos - left);
//         lastLeft = left;
        left = string.indexOf(close, pos);
        if (left < 0) { // opens, but doesn't close
            break;
        } else {
            next = pos;
            while (next > -1 && left > -1) {
                // search for inner iterations
                int count = 0;
                int lastNext = next;
                while ((next = string.indexOf(inner, next+inner.length())) < left && next > -1) {
                    // count inner section openers
                    lastNext = next;
                    ++count;
                }
                next = lastNext; // set back next to last inside opener for next iteration

                if (!count) { // no inner sections, skip
                    break;
                }

                for (int i = 0; i < count; ++i) { // shift section closers by inside section amount
                    left = string.indexOf(close, left+close.length());
                }
                // "continue" - search for next inner section
            }

            if (left < 0) { // section does not close, skip here
                break;
            }

            left += close.length(); // extend close to next search start
        }

        if (left < 0) { // section does not close, skip here
            break;
        }

        pos = string.indexOf(open, left); // search next 1st level section opener
    }

    if (left > -1) { // append last part
        result += string.mid(left);
    }

    return result;
}

static QString wikiToHtml(QString answer)
{
    int start = answer.indexOf('>', answer.indexOf("<text")) + 1;
    int end = answer.lastIndexOf(QRegExp("\\n[^\\n]*\\n\\{\\{reflist", Qt::CaseInsensitive));
    if (end < start) {
        end = INT_MAX;
    }
    int e = answer.lastIndexOf(QRegExp("\\n[^\\n]*\\n&lt;references", Qt::CaseInsensitive));
    if (e > start && e < end) {
        end = e;
    }
    e = answer.lastIndexOf(QRegExp("\n==\\s*Sources\\s*=="));
    if (e > start && e < end) {
        end = e;
    }
    e = answer.lastIndexOf(QRegExp("\n==\\s*Notes\\s*=="));
    if (e > start && e < end) {
        end = e;
    }
    e = answer.lastIndexOf(QRegExp("\n==\\s*References\\s*=="));
    if (e > start && e < end) {
        end = e;
    }
    e = answer.lastIndexOf(QRegExp("\n==\\s*External links\\s*=="));
    if (e > start && e < end) {
        end = e;
    }
    if (end < start) {
        end = answer.lastIndexOf("</text");
    }

    answer = answer.mid(start, end - start); // strip header/footer
    answer = strip(answer, "{{", "}}"); // strip wiki internal stuff
    answer.replace("&lt;", "<").replace("&gt;", ">");
    answer = strip(answer, "<!--", "-->"); // strip comments
    answer.remove(QRegExp("<ref[^>]*/>")); // strip inline refereces
    answer = strip(answer, "<ref", "</ref>", "<ref"); // strip refereces
//     answer = strip(answer, "<ref ", "</ref>", "<ref"); // strip argumented refereces
    answer = strip(answer, "[[File:", "]]", "[["); // strip images etc
    answer = strip(answer, "[[Image:", "]]", "[["); // strip images etc

    answer.replace(QRegExp("\\[\\[[^\\[\\]]*\\|([^\\[\\]\\|]*)\\]\\]"), "\\1"); // collapse commented links
    answer.replace("[['", "[["); // Fixes '74 (e.g. 1974) causing errors!
    answer.remove("[[").remove("]]"); // remove wiki link "tags"

    answer = answer.trimmed();

//     answer.replace(QRegExp("\\n\\{\\|[^\\n]*wikitable[^\\n]*\\n!"), "\n<table><th>");

    answer.replace("\n\n", "<br>");
//     answer.replace("\n\n", "</p><p align=\"justify\">");
    answer.replace(QRegExp("\\n'''([^\\n]*)'''\\n"), "<hr><b>\\1</b>\n");
    answer.replace(QRegExp("\\n\\{\\|[^\\n]*\\n"), "\n");
    answer.replace(QRegExp("\\n\\|[^\\n]*\\n"), "\n");
    answer.replace("\n*", "<br>");
    answer.replace("\n", "");
    answer.replace("'''", "¬").replace(QRegExp("¬([^¬]*)¬"), "<b>\\1</b>");
    answer.replace("''", "¬").replace(QRegExp("¬([^¬]*)¬"), "<i>\\1</i>");
    answer.replace("===", "¬").replace(QRegExp("¬([^¬]*)¬"), "<h3>\\1</h3>");
    answer.replace("==", "¬").replace(QRegExp("¬([^¬]*)¬"), "<h2>\\1</h2>");
    answer.replace("&amp;nbsp;", " ");
    answer.replace("<br><h", "<h");
    answer.replace("</h2><br>", "</h2>");
    answer.replace("</h3><br>", "</h3>");
    answer.replace("<h3>=", "<h4>");
    answer.replace("</h3>=", "</h4>");
    answer.replace("br>;", "br>");
    answer.replace("h2>;", "h2>");
    answer.replace("h3>;", "h3>");
    return answer;
}

QStringList WikipediaEngine::preferredLangs;

WikipediaEngine::WikipediaEngine(QObject *p)
    : ContextEngine(p)
{
    if (preferredLangs.isEmpty()) {
        setPreferedLangs(Settings::self()->wikipediaLangs());
    }
}

void WikipediaEngine::setPreferedLangs(const QStringList &l)
{
    preferredLangs=l;
    if (preferredLangs.isEmpty()) {
        preferredLangs.append("en");
    }
}

void WikipediaEngine::search(const QStringList &query, Mode mode)
{
    titles.clear();
    requestTitles(query, mode, preferredLangs.first());
}

void WikipediaEngine::requestTitles(const QStringList &query, Mode mode, const QString &lang)
{
    cancel();
    QUrl url("https://"+lang+".wikipedia.org/w/api.php");
    #if QT_VERSION < 0x050000
    QUrl &q=url;
    #else
    QUrlQuery q;
    #endif

    q.addQueryItem(QLatin1String("action"), QLatin1String("query"));
    q.addQueryItem(QLatin1String("list"), QLatin1String("search"));
    q.addQueryItem(QLatin1String("srsearch"), query.join(" "));
    q.addQueryItem(QLatin1String("srprop"), QLatin1String("size") );
    q.addQueryItem(QLatin1String("srredirects"), QString::number(1));
    q.addQueryItem(QLatin1String("srlimit"), QString::number(20));
    q.addQueryItem(QLatin1String("format"), QLatin1String("xml"));
    #if QT_VERSION >= 0x050000
    url.setQuery(q);
    #endif

    job=NetworkAccessManager::self()->get(url);
    job->setProperty(constModeProperty, (int)mode);
    job->setProperty(constLangProperty, lang);
    job->setProperty(constRedirectsProperty, 0);
    job->setProperty(constQueryProperty, query);
    qWarning() << "XXX requestTitles:" << url.toString();
    connect(job, SIGNAL(finished()), this, SLOT(parseTitles()));
}

void WikipediaEngine::parseTitles()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply) {
        return;
    }

    QByteArray data=reply->readAll();
    if (QNetworkReply::NoError!=reply->error() || data.isEmpty()) {
        emit searchResult(QString(), QString());
        return;
    }

    QString hostLang = reply->property(constLangProperty).toString();
    QStringList query = reply->property(constQueryProperty).toStringList();
    Mode mode=(Mode)reply->property(constModeProperty).toInt();
    QXmlStreamReader xml(data);

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement() && QLatin1String("search")==xml.name()) {
            while (xml.readNextStartElement()) {
                if (QLatin1String("p")==xml.name()) {
                    if (xml.attributes().hasAttribute(QLatin1String("title"))) {
                        titles << xml.attributes().value(QLatin1String("title")).toString();
                    }
                    xml.skipCurrentElement();
                } else {
                    xml.skipCurrentElement();
                }
            }
        }
    }

    if (titles.isEmpty()) {
        QStringList refinePossibleLangs = preferredLangs.filter(QRegExp("^(en|fr|de|pl).*$") );
        int index = refinePossibleLangs.indexOf(hostLang);
        if (-1!=index && index<refinePossibleLangs.count()-1) {
            // Try next language!
            requestTitles(query, mode, refinePossibleLangs.value(index + 1).split(QLatin1Char(':')).back());
        } else {
            emit searchResult(QString(), QString());
        }
        return;
    }

    getPage(query, mode, hostLang);
}

static int indexOf(const QStringList &l, const QString &s)
{
    for (int i=0; i<l.length(); ++i){
        qWarning() << "COMPARE" << l.at(i) << s << l.at(i).compare(s, Qt::CaseInsensitive);
        if (0==l.at(i).compare(s, Qt::CaseInsensitive)) {
            return i;
        }
    }
    return -1;
}

void WikipediaEngine::getPage(const QStringList &query, Mode mode, const QString &lang)
{
    QStringList queryCopy(query);
    QStringList queries;

    while(!queryCopy.isEmpty()) {
        queries.append(queryCopy.join(" "));
        queryCopy.takeFirst();
    }

    #if 0
    // Amarok original - not working for me :-(
    qWarning() << "XXX Titles:" << titles;
    QString basePattern;
    switch (mode)
    {
    default:
    case Artist:
        basePattern=i18nc("Search pattern for an artist or band", ".*\\(.*(artist|band).*\\))").toLatin1();
        break;
    case Album:
        basePattern=i18nc("Search pattern for an album", ".*\\(.*(album|score|soundtrack).*\\)").toLatin1();
        break;
    }

    int index=-1;
    foreach (const QString &q, queries) {
        QString pattern=q+basePattern;
        index = titles.indexOf(QRegExp(pattern, Qt::CaseInsensitive));
        qWarning() << "XXX Query[a]:" << q << pattern << index;
        if (-1==index) {
            QString q2=q;
            q2.remove(".");
            pattern=q2+basePattern;
            index = titles.indexOf(QRegExp(pattern, Qt::CaseInsensitive));
            qWarning() << "XXX Query[b]:" << q2 << pattern << index;
        }
        if (-1!=index) {
            break;
        }
    }
    #else
    QStringList patterns;
    switch (mode)
    {
    default:
    case Artist:
        patterns=i18nc("Search pattern for an artist or band, separated by |", "artist|band").split("|", QString::SkipEmptyParts);
        break;
    case Album:
        patterns=i18nc("Search pattern for an album, separated by |", "album|score|soundtrack").split("|", QString::SkipEmptyParts);
        break;
    }

    qWarning() << "XXX Titles:" << titles;
    int index=-1;
    foreach (const QString &q, queries) {
        qWarning() << "XXX Query:" << q;
        qWarning() << "XXX - patterns";
        // First check original query with one of the patterns...
        foreach (const QString &pattern, patterns) {
            index=indexOf(titles, q+" ("+pattern+")");
            if (-1!=index) {
                qWarning() << "XXX match[a]" << index << q;
                break;
            }
        }

        if (-1==index && q.contains(".")) {
            qWarning() << "XXX - patterns (no dots)";
            // Now try by removing all dots (A.S.A.P. -> ASAP)
            QString query2=q;
            query2.remove(".");
            foreach (const QString &pattern, patterns) {
                index=indexOf(titles, query2+" ("+pattern+")");
                if (-1!=index) {
                    qWarning() << "XXX match[b]" << index << q;
                    break;
                }
            }
        }

        if (-1==index) {
            // Try without any pattern...
            qWarning() << "XXX - no pattern";
            index=indexOf(titles, q);
            if (-1!=index) {
                qWarning() << "XXX match[c]" << index << q;
            }
        }

        if (-1==index && q.contains(".")) {
            // Try without any pattern, and no dots..
            qWarning() << "XXX - no pattern no dots";
            QString query2=q;
            query2.remove(".");
            index=indexOf(titles, query2);
            if (-1!=index) {
                qWarning() << "XXX match[d]" << index << q;
            }
        }
        if (-1!=index) {
            break;
        }
    }
    #endif
    // TODO: If we fail to find a match, prompt user???
    const QString title=titles.takeAt(-1==index ? 0 : index);
    QUrl url;
    url.setScheme(QLatin1String("https"));
    url.setHost(lang+".wikipedia.org");
    url.setPath("/wiki/Special:Export/"+title);
    job=NetworkAccessManager::self()->get(url);
    job->setProperty(constModeProperty, (int)mode);
    job->setProperty(constLangProperty, lang);
    job->setProperty(constQueryProperty, query);
    job->setProperty(constRedirectsProperty, 0);
    qWarning() << "XXX getPage:" << url.toString();
    connect(job, SIGNAL(finished()), this, SLOT(parsePage()));
}

void WikipediaEngine::parsePage()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply) {
        return;
    }

    QVariant redirect = reply->header(QNetworkRequest::LocationHeader);
    int numRirects=reply->property(constRedirectsProperty).toInt();
    if (redirect.isValid() && ++numRirects<constMaxRedirects) {
        job=NetworkAccessManager::self()->get(redirect.toString());
        job->setProperty(constRedirectsProperty, numRirects);
        job->setProperty(constLangProperty, reply->property(constLangProperty));
        job->setProperty(constModeProperty, reply->property(constModeProperty));
        job->setProperty(constQueryProperty, reply->property(constQueryProperty));
        connect(job, SIGNAL(finished()), this, SLOT(parsePage()));
        return;
    }

    QByteArray data=reply->readAll();
    if (QNetworkReply::NoError!=reply->error() || data.isEmpty()) {
        emit searchResult(QString(), QString());
        return;
    }

    QString answer(QString::fromUtf8(data));
    qWarning() << "XXX ANS:" << answer;
    if (answer.contains(QLatin1String("{{disambiguation}}")) || answer.contains(QLatin1String("{{disambig}}"))) { // i18n???
        getPage(reply->property(constQueryProperty).toStringList(), (Mode)reply->property(constModeProperty).toInt(),
                reply->property(constLangProperty).toString());
        return;
    }

    emit searchResult(wikiToHtml(answer), reply->property(constLangProperty).toString());
}
