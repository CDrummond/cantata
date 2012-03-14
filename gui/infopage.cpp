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
#include <QtGui/QComboBox>
#include <QtGui/QToolButton>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#include <QtCore/QDir>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KWebView>
#include <KDE/KLocale>
#include <KDE/KFileDialog>
#else
#include <QtGui/QFileDialog>
#include <QtWebKit/QWebView>
#endif
#include "infopage.h"
#include "network.h"
#include "mainwindow.h"
#include "settings.h"

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
                act==page()->action(QWebPage::OpenImageInNewWindow) ||
                act==page()->action(QWebPage::OpenFrameInNewWindow)) {
                act->setVisible(false);
            }
        }
        menu->exec(ev->globalPos());
        menu->deleteLater();
    }
};

InfoPage::InfoPage(QWidget *parent)
    : QWidget(parent)
    , iHaveAskedForArtist(false)
{
    QGridLayout *layout=new QGridLayout(this);
    view=new WebView(this);
    QToolButton *refreshBtn=new QToolButton(this);
    QToolButton *backBtn=new QToolButton(this);
    QToolButton *forwardBtn=new QToolButton(this);
    combo=new QComboBox(this);
    #ifdef ENABLE_KDE_SUPPORT
    combo->insertItem(0, i18n("Artist Information"));
    combo->insertItem(1, i18n("Album Information"));
    #else
    combo->insertItem(0, tr("Artist Information"));
    combo->insertItem(1, tr("Album Information"));
    #endif
    layout->addWidget(view, 0, 0, 1, 4);
    layout->addWidget(refreshBtn, 1, 0, 1, 1);
    layout->addWidget(backBtn, 1, 1, 1, 1);
    layout->addWidget(forwardBtn, 1, 2, 1, 1);
    layout->addWidget(combo, 1, 3, 1, 1);
    layout->setContentsMargins(0, 0, 0, 0);

    view->page()->action(QWebPage::Reload)->setShortcut(QKeySequence());
    MainWindow::initButton(refreshBtn);
    refreshBtn->setDefaultAction(view->page()->action(QWebPage::Reload));
    MainWindow::initButton(backBtn);
    backBtn->setDefaultAction(view->page()->action(QWebPage::Back));
    MainWindow::initButton(forwardBtn);
    forwardBtn->setDefaultAction(view->page()->action(QWebPage::Forward));
    connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(changeView()));
    connect(view->page(), SIGNAL(downloadRequested(const QNetworkRequest &)), SLOT(downloadRequested(const QNetworkRequest &)));
    view->setZoomFactor(Settings::self()->infoZoom()/100.0);
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

    QUrl question( "http://www.google.com/search?as_q=" + query + "&ft=i&num=1&as_sitesearch=wikipedia.org" );
    QString lang = QLocale::languageToString(QLocale::system().language());
    Network::self()->get( question, this, "googleAnswer", lang );
}

void InfoPage::fetchWiki(QString query)
{
    QUrl wikiArtist("http://en.wikipedia.org/wiki/" + query.replace(' ', '_') );
//     QUrl wikiArtist("http://en.wikipedia.org/wiki/" + query.replace(' ', '_') );
    Network::self()->get( wikiArtist, this, "setArtistWiki" );
}

void InfoPage::changeView()
{
    fetchInfo();
}

void InfoPage::googleAnswer(const QString &ans)
{
    QString answer(ans);
    int start = answer.indexOf( "<h3" );
    if ( start > -1 ) {
        start = answer.indexOf( "href=\"", start ) + 6;
        start = answer.indexOf( "http", start );
        int end = answer.indexOf( "\"", start );
        end = qMin(end, answer.indexOf( "&amp", start ));
        answer = answer.mid( start, end - start );
    }
    else {
        answer.clear();
    }

    if ( !iHaveAskedForArtist && (!answer.contains("wikipedia.org") || answer.contains( QRegExp("/.*:") ) )) {
        iHaveAskedForArtist=true;
        askGoogle( song.artist );
        return;
    }
    QUrl wikiInfo; //( answer.mid( start, end - start ).replace("/wiki/", "/wiki/Special:Export/").toUtf8() );
    if (!answer.contains("mobile.wikipedia.org")) {
        answer.replace("wikipedia.org", "mobile.wikipedia.org");
    }
    wikiInfo.setEncodedUrl( answer/*.replace("/wiki/", "/wiki/Special:Export/")*/.toUtf8() );
    wikiInfo.addQueryItem("action", "view");
    view->setUrl(wikiInfo);
}

void InfoPage::update(const Song &s)
{
    iHaveAskedForArtist=false;
    song=s;
    fetchInfo();
}

void InfoPage::fetchInfo()
{
    QString question = 0==combo->currentIndex() ? song.artist : (song.artist.isEmpty() && song.album.isEmpty() ? song.title : song.artist + " " + song.album);
    if ( !question.isEmpty() && lastWikiQuestion != question ) {
        lastWikiQuestion = question;
        #ifdef ENABLE_KDE_SUPPORT
        view->setHtml(i18n("<h3><i>Loading...</i></h3>"));
        #else
        view->setHtml(tr("<h3><i>Loading...</i></h3>"));
        #endif

        askGoogle( question );
//         fetchWiki( artist );
    }
}

void InfoPage::downloadRequested(const QNetworkRequest &request)
{
    QString defaultFileName=QFileInfo(request.url().toString()).fileName();
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getSaveFileName(KUrl(), QString(), this);
    #else
    QString fileName=QFileDialog::getSaveFileName(this, tr("Save File"), defaultFileName);
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
}
