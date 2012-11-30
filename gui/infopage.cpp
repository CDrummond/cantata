    /*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtGui/QGridLayout>
#include <QtGui/QBoxLayout>
#include <QtGui/QComboBox>
#include <QtGui/QToolButton>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#include <QtGui/QFrame>
#include <QtCore/QDir>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWebKit/QWebSettings>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KWebView>
#include <KDE/KFileDialog>
#include <KDE/KGlobalSettings>
#else
#include <QtGui/QFileDialog>
#include <QtWebKit/QWebView>
#endif
#include "localize.h"
#include "infopage.h"
#include "mainwindow.h"
#include "settings.h"
#include "icon.h"

#ifdef ENABLE_KDE_SUPPORT
#define WEBVIEW_BASE KWebView
#else
#define WEBVIEW_BASE QWebView
#endif

class WebView : public WEBVIEW_BASE
{
public:
    WebView(QWidget *p) : WEBVIEW_BASE(p) { }
    QSize sizeHint() const { return QSize(128, 128); }

    void contextMenuEvent(QContextMenuEvent *ev)
    {
        if (page()->swallowContextMenuEvent(ev)) {
            ev->accept();
        }
        QMenu *menu=page()->createStandardContextMenu();

        foreach (QAction *act, menu->actions()) {
            if (act==page()->action(QWebPage::OpenLinkInNewWindow) ||
                act==page()->action(QWebPage::OpenFrameInNewWindow) ||
                act==page()->action(QWebPage::OpenImageInNewWindow)) {
                act->setVisible(false);
            }
        }
        menu->exec(ev->globalPos());
        menu->deleteLater();
    }

    #ifndef ENABLE_KDE_SUPPORT
    void wheelEvent(QWheelEvent *event)
    {
        if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
            const int numDegrees = event->delta() / 8;
            const int numSteps = numDegrees / 15;
            setZoomFactor(zoomFactor() + numSteps * 0.1);
            event->accept();
            return;
        }
        QWebView::wheelEvent(event);
    }
    #endif
};

InfoPage::InfoPage(QWidget *parent)
    : QWidget(parent)
    , iHaveAskedForArtist(false)
{
    QGridLayout *layout=new QGridLayout(this);
    QFrame *frame=new QFrame(this);
    QBoxLayout *frameLayout=new QBoxLayout(QBoxLayout::TopToBottom, frame);
    view=new WebView(frame);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setFrameShadow(QFrame::Sunken);
    frameLayout->setMargin(0);
    frameLayout->addWidget(view);
    QToolButton *refreshBtn=new QToolButton(this);
    QToolButton *backBtn=new QToolButton(this);
    QToolButton *forwardBtn=new QToolButton(this);
    combo=new QComboBox(this);
    combo->insertItem(0, i18n("Artist Information"));
    combo->insertItem(1, i18n("Album Information"));
    layout->addWidget(frame, 0, 0, 1, 4);
    layout->addWidget(refreshBtn, 1, 0, 1, 1);
    layout->addWidget(backBtn, 1, 1, 1, 1);
    layout->addWidget(forwardBtn, 1, 2, 1, 1);
    layout->addWidget(combo, 1, 3, 1, 1);
    layout->setContentsMargins(0, 0, 0, 0);

    view->page()->action(QWebPage::Reload)->setShortcut(QKeySequence());
    Icon::init(refreshBtn);
    refreshBtn->setDefaultAction(view->page()->action(QWebPage::Reload));
    Icon::init(backBtn);
    backBtn->setDefaultAction(view->page()->action(QWebPage::Back));
    Icon::init(forwardBtn);
    forwardBtn->setDefaultAction(view->page()->action(QWebPage::Forward));
    connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(changeView()));
    connect(view->page(), SIGNAL(downloadRequested(const QNetworkRequest &)), SLOT(downloadRequested(const QNetworkRequest &)));
    view->setZoomFactor(Settings::self()->infoZoom()/100.0);
    view->settings()->setAttribute(QWebSettings::PrivateBrowsingEnabled, true);
    view->settings()->setAttribute(QWebSettings::JavascriptEnabled, false);
    view->settings()->setFontSize(QWebSettings::MinimumFontSize, 8);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DnsPrefetchEnabled, true);

    #ifdef ENABLE_KDE_SUPPORT
    connect(KGlobalSettings::self(), SIGNAL(appearanceChanged()), SLOT(updateFonts()));
    updateFonts();
    #endif
}

void InfoPage::save()
{
    Settings::self()->saveInfoZoom(view->zoomFactor()*100);
}

void InfoPage::askGoogle(const QString &query)
{
    if (query.isEmpty()) {
        return;
    }
    get("http://www.google.com/search?as_q=" + query + "&ft=i&num=1&as_sitesearch=wikipedia.org");
}

void InfoPage::changeView()
{
    fetchInfo();
}

void InfoPage::googleAnswer(const QString &ans)
{
    QString answer(ans);
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

    if (!iHaveAskedForArtist && (!answer.contains("wikipedia.org") || answer.contains(QRegExp("/.*:")))) {
        iHaveAskedForArtist=true;
        askGoogle(song.artist);
        return;
    }
    QUrl wikiInfo; //(answer.mid(start, end - start).replace("/wiki/", "/wiki/Special:Export/").toUtf8());
    if (!answer.contains("m.wikipedia.org")) {
        answer.replace("wikipedia.org", "m.wikipedia.org");
    }
    wikiInfo.setEncodedUrl(answer/*.replace("/wiki/", "/wiki/Special:Export/")*/.toUtf8());
    wikiInfo.addQueryItem("action", "view");
    wikiInfo.addQueryItem("useskin", "monobook");
    wikiInfo.addQueryItem("useformat", "mobile");

//     view->settings()->resetFontSize(QWebSettings::MinimumFontSize);
//     view->settings()->resetFontSize(QWebSettings::MinimumLogicalFontSize);
//     view->settings()->resetFontSize(QWebSettings::DefaultFontSize);
//     view->settings()->resetFontSize(QWebSettings::DefaultFixedFontSize);
//     view->settings()->resetFontFamily(QWebSettings::StandardFont);
    view->setUrl(wikiInfo);
}

void InfoPage::update(const Song &s)
{
    iHaveAskedForArtist=false;
    song=s;
    if (song.isVariousArtists()) {
        song.revertVariousArtists();
    }
    fetchInfo();
}

void InfoPage::fetchInfo()
{
    QString question = 0==combo->currentIndex() ? song.artist : (song.artist.isEmpty() && song.album.isEmpty() ? song.title : song.artist + " " + song.album);
    if (!question.isEmpty() && lastWikiQuestion != question) {
        lastWikiQuestion = question;
        view->setHtml(i18n("<h3><i>Loading...</i></h3>"));
        askGoogle(question);
//         fetchWiki(artist);
    }
}

void InfoPage::downloadRequested(const QNetworkRequest &request)
{
    QString defaultFileName=QFileInfo(request.url().toString()).fileName();
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getSaveFileName(KUrl(), QString(), this);
    #else
    QString fileName=QFileDialog::getSaveFileName(this, i18nc("Qt-only", "Save File"), defaultFileName);
    #endif
    if (fileName.isEmpty()) {
        return;
    }

    QNetworkRequest newRequest = request;
    newRequest.setAttribute(QNetworkRequest::User, fileName);

    QNetworkAccessManager *networkManager = view->page()->networkAccessManager();
    QNetworkReply *reply = networkManager->get(newRequest);
    connect(reply, SIGNAL(finished()), this, SLOT(downloadingFinished()));
}

void InfoPage::downloadingFinished()
{
    QNetworkReply *reply = ((QNetworkReply*)sender());
    QNetworkRequest request = reply->request();
    QFile file(request.attribute(QNetworkRequest::User).toString());
    if (file.open(QFile::ReadWrite)) {
        file.write(reply->readAll());
    }
    reply->deleteLater();
}

#ifdef ENABLE_KDE_SUPPORT
void InfoPage::updateFonts()
{
    view->settings()->setFontFamily(QWebSettings::StandardFont, KGlobalSettings::generalFont().family());
    view->settings()->setFontFamily(QWebSettings::SerifFont, KGlobalSettings::generalFont().family());
    view->settings()->setFontFamily(QWebSettings::SansSerifFont, KGlobalSettings::generalFont().family());
    view->settings()->setFontFamily(QWebSettings::FixedFont, KGlobalSettings::fixedFont().family());

}
#endif

void InfoPage::handleReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    QVariant redirect = reply->header(QNetworkRequest::LocationHeader);
    if (redirect.isValid()) {
        get(redirect.toUrl());
        reply->deleteLater();
        return;
    }

    reply->open(QIODevice::ReadOnly | QIODevice::Text);
    QString answer = QString::fromUtf8(reply->readAll());
    reply->close();
    googleAnswer(answer);
    reply->deleteLater();
}

void InfoPage::get(const QUrl &url)
{
    QNetworkRequest request(url);
    QString lang(QLocale::languageToString(QLocale::system().language()));
    // lie to prevent google etc. from believing we'd be some automated tool, abusing their ... errr ;-P
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux i686; rv:6.0) Gecko/20100101 Firefox/6.0");
    if (!lang.isEmpty()) {
        request.setRawHeader("Accept-Language", lang.toAscii());
    }

    QNetworkReply *reply = view->page()->networkAccessManager()->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(handleReply()));
}
