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
 
#include "view.h"
#include "spinner.h"
#include "networkaccessmanager.h"
#include <QLabel>
#include <QTextBrowser>
#include <QImage>
#include <QPixmap>
#include <QBoxLayout>
#include <QNetworkReply>
#include <QLocale>

const QLatin1String View::constAmbiguous("-");

View::View(QWidget *parent)
    : QWidget(parent)
    , needToUpdate(false)
    , spinner(0)
    , job(0)
{
    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    header=new QLabel(this);
    pic=new QLabel(this);
    text=new QTextBrowser(this);

    layout->setMargin(0);

    header->setWordWrap(true);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    text->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    pic->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    layout->addWidget(header);
    layout->addWidget(pic);
    layout->addWidget(text);
    layout->addItem(new QSpacerItem(1, fontMetrics().height()/4, QSizePolicy::Fixed, QSizePolicy::Fixed));
    text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setEditable(false);
    locale=qgetenv("CANTATA_LOCALE");
    if (locale.isEmpty()) {
        locale="en";
    }
}

void View::clear()
{
    setHeader(stdHeader);
    setPic(QImage());
    text->clear();
}

void View::setHeader(const QString &str)
{
    header->setText("<h1>"+str+"</h1>");
}

void View::setPicSize(const QSize &sz)
{
    picSize=sz;
    if (pic && (picSize.width()<1 || picSize.height()<1)) {
        pic->deleteLater();
        pic=0;
    }
}

void View::setPic(const QImage &img)
{
    if (!pic) {
        return;
    }

    if (img.isNull()) {
        pic->clear();
        pic->setVisible(false);
    } else {
        pic->setPixmap(QPixmap::fromImage(img.scaled(picSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        pic->setVisible(true);
    }
}


void View::showEvent(QShowEvent *e)
{
    if (needToUpdate) {
        update(currentSong, true);
    }
    needToUpdate=false;
    QWidget::showEvent(e);
}

void View::showSpinner()
{
    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(this);
    }
    if (!spinner->isActive()) {
        spinner->start();
    }
}

void View::hideSpinner()
{
    if (spinner) {
        spinner->stop();
    }
}

void View::setEditable(bool e)
{
    text->setReadOnly(!e);
    text->setFrameShape(e ? QFrame::StyledPanel : QFrame::NoFrame);
    text->viewport()->setAutoFillBackground(e);
}

void View::searchResponse(const QString &r)
{
    text->setText(r);
}

void View::cancel()
{
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(googleAnswer()));
        job->deleteLater();
        job=0;
    }
}

void View::search(const QString &query)
{
    cancel();
    if (query.isEmpty()) {
        return;
    }
    QUrl url("http://www.google.com/search?as_q="+query+"&ft=i&num=1&as_qdr=all&pws=0&as_sitesearch="+locale+".wikipedia.org");

    QNetworkRequest request(url);
    QString lang(QLocale::languageToString(QLocale::system().language()));
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux i686; rv:6.0) Gecko/20100101 Firefox/6.0");
    if (!lang.isEmpty()) {
        request.setRawHeader("Accept-Language", lang.toAscii());
    }
    job = NetworkAccessManager::self()->get(request);
    connect(job, SIGNAL(finished()), this, SLOT(googleAnswer()));
}

void View::googleAnswer()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());

    if (!reply) {
        return;
    }
    reply->deleteLater();
    if (reply!=job) {
        return;
    }
    job=0;
    if (reply->error()) {
        searchResponse(QString());
        return;
    }

    QString answer(QString::fromUtf8(reply->readAll()));
    int start = answer.indexOf("<h3");
    if (start > -1) {
        start = answer.indexOf("href=\"", start) + 6;
        start = answer.indexOf("http", start);
        int end = answer.indexOf("\"", start);
        end = qMin(end, answer.indexOf("&amp", start));
        answer = answer.mid(start, end - start);
        // For "Queensrÿche" google returns http://en.wikipedia.org/wiki/Queensr%25C3%25BFche
        // ...but (I think) this should be http://en.wikipedia.org/wiki/Queensr%C3%BFche
        // ...so, replace %25 with % :-)
        answer.replace("%25", "%");
    }
    else {
        answer.clear();
    }

    if (!answer.contains("wikipedia.org")) {
        searchResponse(QString());
        cancel();
        return;
    }
    QUrl wiki;
    wiki.setEncodedUrl(answer.replace("/wiki/", "/wiki/Special:Export/").toUtf8());
    job = NetworkAccessManager::self()->get(wiki);
    job->setProperty("redirect", 0);
    connect(job, SIGNAL(finished()), this, SLOT(wikiAnswer()));
}

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

void View::wikiAnswer()
{
    static const int constMaxRedirects=3;
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());

    if (!reply) {
        return;
    }
    reply->deleteLater();
    if (reply!=job) {
        return;
    }
    job=0;

    QVariant redirect = reply->header(QNetworkRequest::LocationHeader);
    int numRirects=reply->property("redirect").toInt();
    if (redirect.isValid() && ++numRirects<constMaxRedirects) {
        job=NetworkAccessManager::self()->get(redirect.toString());
        job->setProperty("redirect", numRirects);
        connect(job, SIGNAL(finished()), this, SLOT(wikiAnswer()));
        return;
    }

    if (reply->error()) {
        searchResponse(QString());
        return;
    }

    QString answer(QString::fromUtf8(reply->readAll()));
    if (answer.contains(QLatin1String("{{disambiguation}}"))) {
        searchResponse(constAmbiguous);
        return;
    }

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
    searchResponse(answer);
}
