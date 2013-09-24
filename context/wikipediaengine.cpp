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
#include "config.h"
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QXmlStreamReader>
#include <QRegExp>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void WikipediaEngine::enableDebug()
{
    debugEnabled=true;
}

static const char * constModeProperty="mode";
static const char * constQueryProperty="query";

static QString strip(const QString &string, QString open, QString close, QString inner=QString())
{
    QString result;
    int next, /*lastLeft, */left = 0;
    int pos = string.indexOf(open, 0, Qt::CaseInsensitive);

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

static QString stripEmptySections(QString answer)
{
    QStringList headers=QStringList() << "h3" << "h2" << "b";
    foreach (const QString &h1, headers) {
        foreach (const QString &h2, headers) {
            int end=-1;
            do {
                end=answer.indexOf("</"+h1+"><"+h2+">");
                int realEnd=end+3+h1.length();
                if (-1==end) {
                    end=answer.indexOf("</"+h1+"><br><br><"+h2+">");
                    realEnd=end+11+h1.length();
                }
                if (-1!=end) {
                    int start=answer.lastIndexOf("<"+h1+">", end);
                    if (-1!=start) {
                        answer=answer.left(start)+answer.mid(realEnd);
                    }
                }
            } while(-1!=end);
        }
    }
    return answer;
}

static QString stripLastEmptySection(QString answer)
{
    QStringList headers=QStringList() << "h3" << "h2" << "b";
    bool modified=false;
    do {
        modified=false;
        foreach (const QString &h, headers) {
            if (answer.endsWith("</"+h+">") || answer.endsWith("</"+h+"> ") || answer.endsWith("</"+h+">  ")) {
                int start=answer.lastIndexOf("<"+h+">", answer.length()-4);
                if (-1!=start) {
                    answer=answer.left(start);
                    modified=true;
                }
            }
        }
    } while (modified);
    return answer;
}

static QString wikiToHtml(QString answer, bool introOnly, const QUrl &url)
{
    QString u=url.toString();
    u.replace("/wiki/Special:Export/", "/wiki/");
    int start = answer.indexOf('>', answer.indexOf("<text"))+1;
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
//    answer = strip(answer, "({{", "}})"); // strip wiki internal stuff
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
    answer = strip(answer, "{| class=&quot;wikitable&quot;", "|}");

    answer = answer.trimmed();

//     answer.replace(QRegExp("\\n\\{\\|[^\\n]*wikitable[^\\n]*\\n!"), "\n<table><th>");

    answer.replace("\n\n", "<br>");
    answer.replace("(  ; ", "(");
//     answer.replace("\n\n", "</p><p align=\"justify\">");
    answer.replace(QRegExp("\\n'''([^\\n]*)'''\\n"), "<hr><b>\\1</b>\n");
    answer.replace(QRegExp("\\n\\{\\|[^\\n]*\\n"), "\n");
    answer.replace(QRegExp("\\n\\|[^\\n]*\\n"), "\n");
    answer.replace("\n*", "<br>");
    answer.replace("\n", "");
    answer.replace("'''s ", "'s");
    answer.replace("'''", "¬").replace(QRegExp("¬([^¬]*)¬"), "<b>\\1</b>");
    answer.replace("''", "¬").replace(QRegExp("¬([^¬]*)¬"), "<i>\\1</i>");
    if (!introOnly) {
        answer.replace("===", "¬").replace(QRegExp("¬([^¬]*)¬"), "<h3>\\1</h3>");
        answer.replace("==", "¬").replace(QRegExp("¬([^¬]*)¬"), "<h2>\\1</h2>");
    }
    answer.replace("&amp;nbsp;", " ");
    answer.replace("<br><h", "<h");
    if (introOnly) {
        end=answer.indexOf("==", 3);
        if (-1==end) {
            return QString();
        } else {
            answer=answer.left(end).trimmed();
            if (!answer.endsWith("<br>")) {
                answer+="<br>";
            }
            answer+=QString("<br><a href=\"%1\">%2</a>").arg(u).arg(WikipediaEngine::constReadMorePlaceholder);
        }
    } else {
        answer.replace("</h2><br>", "</h2>");
        answer.replace("</h3><br>", "</h3>");
        answer.replace("<h3>=", "<h4>");
        answer.replace("</h3>=", "</h4>");
        answer.replace("br>;", "br>");
        answer.replace("h2>;", "h2>");
        answer.replace("h3>;", "h3>");
        answer.replace("<br><br><br><br><br>", "<br><br>");
        answer.replace("<br><br><br><br>", "<br><br>");
        answer.replace("<br><br><br>", "<br><br>");

        // Remove track listings - we take these from MPD...
        QString listingText="<h2>"+i18n("Track listing")+"</h2>";
        start=answer.indexOf(listingText, 0, Qt::CaseInsensitive);
        if (-1!=start) {
            int end=answer.indexOf("<h2>", start+listingText.length(), Qt::CaseInsensitive);
            if (start!=end) {
                answer=answer.left(start)+answer.mid(end);
            }
        }

        // Try to remove empty sections (that will have been reated because we removed tables!)
        answer=stripEmptySections(answer);
        answer=stripLastEmptySection(answer);
    }

    if (!introOnly) {
        if (!answer.endsWith("<br>")) {
            answer+="<br>";
        }
        answer+=QString("<br><a href='%1'>%2</a>").arg(u).arg(WikipediaEngine::constOpenInBrowserPlaceholder);
    }

    return answer;
}

static inline QString getLang(const QUrl &url)
{
    return url.host().remove(QLatin1String(".wikipedia.org"));
}

QStringList WikipediaEngine::preferredLangs;
bool WikipediaEngine::introOnly=true;
const QLatin1String WikipediaEngine::constReadMorePlaceholder("XXX_CONTEXT_READ_MORE_ON_WIKIPEDIA_XXX");
const QLatin1String WikipediaEngine::constOpenInBrowserPlaceholder("XXX_CONTEXT_OPEN_IN_BROWSER_WIKIPEDIA_XXX");

WikipediaEngine::WikipediaEngine(QObject *p)
    : ContextEngine(p)
{
    if (preferredLangs.isEmpty()) {
        setPreferedLangs(Settings::self()->wikipediaLangs());
        introOnly=Settings::self()->wikipediaIntroOnly();
    }
}

void WikipediaEngine::setPreferedLangs(const QStringList &l)
{
    preferredLangs=l;
    if (preferredLangs.isEmpty()) {
        preferredLangs.append("en");
    }
}

QString WikipediaEngine::translateLinks(QString text) const
{
    text=text.replace(constReadMorePlaceholder, i18n("Read more on wikipedia"));
    text=text.replace(constOpenInBrowserPlaceholder, i18n("Open in browser"));
    return text;
}

void WikipediaEngine::search(const QStringList &query, Mode mode)
{
    titles.clear();
    requestTitles(fixQuery(query), mode, getPrefix(preferredLangs.first()));
}

void WikipediaEngine::requestTitles(const QStringList &query, Mode mode, const QString &lang)
{
    cancel();
    #ifdef ENABLE_HTTPS_SUPPORT
    QUrl url("https://"+lang+".wikipedia.org/w/api.php");
    #else
    QUrl url("http://"+lang+".wikipedia.org/w/api.php");
    #endif
    #if QT_VERSION < 0x050000
    QUrl &q=url;
    #else
    QUrlQuery q;
    #endif

    q.addQueryItem(QLatin1String("action"), QLatin1String("query"));
    q.addQueryItem(QLatin1String("list"), QLatin1String("search"));
    q.addQueryItem(QLatin1String("srsearch"), query.join(" "));
    q.addQueryItem(QLatin1String("srprop"), QLatin1String("size"));
    q.addQueryItem(QLatin1String("srredirects"), QString::number(1));
    q.addQueryItem(QLatin1String("srlimit"), QString::number(20));
    q.addQueryItem(QLatin1String("format"), QLatin1String("xml"));
    #if QT_VERSION >= 0x050000
    url.setQuery(q);
    #endif

    job=NetworkAccessManager::self()->get(url);
    job->setProperty(constModeProperty, (int)mode);
    job->setProperty(constQueryProperty, query);
    DBUG <<  url.toString();
    connect(job, SIGNAL(finished()), this, SLOT(parseTitles()));
}

void WikipediaEngine::parseTitles()
{
    DBUG << __FUNCTION__;
    NetworkJob *reply = getReply(sender());
    if (!reply) {
        return;
    }

    QUrl url=reply->url();
    QString hostLang=getLang(url);
    QByteArray data=reply->readAll();
    if (!reply->ok() || data.isEmpty()) {
        DBUG <<  reply->errorString();
        emit searchResult(QString(), QString());
        return;
    }

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
        DBUG <<  "No titles";
        QRegExp regex(QLatin1Char('^') + hostLang + QLatin1String(".*$"));
        int index = preferredLangs.indexOf(regex);
        if (-1!=index && index < preferredLangs.count()-1) {
            // use next preferred language as base for fetching langlinks since
            // the current one did not get any results we want.
            requestTitles(query, mode, getPrefix(preferredLangs.value(index+1)));
        } else {
            DBUG <<  "No more langs";
            emit searchResult(QString(), QString());
        }
        return;
    }

    getPage(query, mode, hostLang);
}

static int indexOf(const QStringList &l, const QString &s)
{
    QString search=s.simplified();
    for (int i=0; i<l.length(); ++i){
        if (0==l.at(i).compare(search, Qt::CaseInsensitive)) {
            return i;
        }
    }
    return -1;
}

void WikipediaEngine::getPage(const QStringList &query, Mode mode, const QString &lang)
{
    DBUG << __FUNCTION__;
    QStringList queryCopy(query);
    QStringList queries;
    QStringList simplifiedTitles;
    foreach (QString t, titles) {
        simplifiedTitles.append(t.simplified());
    }

    while(!queryCopy.isEmpty()) {
        QString q=queryCopy.join(" ");
        QString q2=q;
        q2.remove("."); // A.S.A.P. -> ASAP
        queries.append(q);
        if (q2!=q) {
            queries.append(q2);
        }

        q2=q;
        q2.replace("-", "/"); // AC-DC -> AC/DC
        queries.append(q);
        if (q2!=q) {
            queries.append(q2);
        }

        queryCopy.takeFirst();
    }

    QStringList patterns;
    QStringList englishPatterns;
    
    switch (mode) {
    default:
    case Artist:
        patterns=i18nc("Search pattern for an artist or band, separated by |", "artist|band|singer|vocalist|musician").split("|", QString::SkipEmptyParts);
        englishPatterns=QString(QLatin1String("artist|band|singer|vocalist|musician")).split("|");
        break;
    case Album:
        patterns=i18nc("Search pattern for an album, separated by |", "album|score|soundtrack").split("|", QString::SkipEmptyParts);
        englishPatterns=QString(QLatin1String("album|score|soundtrack")).split("|");
        break;
    }

    foreach (const QString &eng, englishPatterns) {
        if (!patterns.contains(eng)) {
            patterns.append(eng);
        }
    }

    DBUG <<  "Titles" << titles;

    int index=-1;
    if (mode==Album && 2==query.count()) {
        DBUG <<  "Check album";
        foreach (const QString &pattern, patterns) {
            QString q=query.at(1)+" ("+query.at(0)+" "+pattern+")";
            DBUG <<  "Try" << q;
            index=indexOf(simplifiedTitles, q);
            if (-1!=index) {
                DBUG <<  "Matched with '$album ($artist pattern)" << index << q;
                break;
            }
        }
    }
    if (-1==index) {
        foreach (const QString &q, queries) {
            DBUG <<  "Query" << q;
            // First check original query with one of the patterns...
            foreach (const QString &pattern, patterns) {
                index=indexOf(simplifiedTitles, q+" ("+pattern+")");
                if (-1!=index) {
                    DBUG <<  "Matched with pattern" << index << QString(q+" ("+pattern+")");
                    break;
                }
            }

            if (-1==index) {
                // Try without any pattern...
                index=indexOf(simplifiedTitles, q);
                if (-1!=index) {
                    DBUG <<  "Matched without pattern" << index << q;
                }
            }

            if (-1!=index) {
                break;
            }
        }
    }

    // TODO: If we fail to find a match, prompt user???
    if (-1==index) {
        DBUG <<  "Failed to find match";
        emit searchResult(QString(), QString());
        return;
    }
    const QString title=titles.takeAt(index);

    if (QLatin1String("List of CJK Unified Ideographs")==title) {
        DBUG <<  "Unicode list?";
        emit searchResult(QString(), QString());
        return;
    }

    QUrl url;
    #ifdef ENABLE_HTTPS_SUPPORT
    url.setScheme(QLatin1String("https"));
    #else
    url.setScheme(QLatin1String("http"));
    #endif
    url.setHost(lang+".wikipedia.org");
    url.setPath("/wiki/Special:Export/"+title);
    job=NetworkAccessManager::self()->get(url);
    job->setProperty(constModeProperty, (int)mode);
    job->setProperty(constQueryProperty, query);
    DBUG <<  url.toString();
    connect(job, SIGNAL(finished()), this, SLOT(parsePage()));
}

void WikipediaEngine::parsePage()
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

    QString answer(QString::fromUtf8(data));
    //DBUG <<  "Anser" << answer;
    QUrl url=reply->url();
    QString hostLang=getLang(url);

    if (answer.contains(QLatin1String("{{disambiguation}}")) || answer.contains(QLatin1String("{{disambig}}"))) { // i18n???
        getPage(reply->property(constQueryProperty).toStringList(), (Mode)reply->property(constModeProperty).toInt(),
                hostLang);
        return;
    }

    if (answer.isEmpty()) {
        emit searchResult(QString(), QString());
        return;
    }
    QString resp=wikiToHtml(answer, introOnly, reply->url());
    if (introOnly && resp.isEmpty()) {
        resp=wikiToHtml(answer, false, reply->url());
    }
    emit searchResult(resp, hostLang);
}
