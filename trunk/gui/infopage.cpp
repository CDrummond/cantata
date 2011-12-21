/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QtCore/QDir>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KWebView>
#include <KDE/KLocale>
#include <KDE/KXMLGUIClient>
#include <KDE/KActionCollection>
#include <KDE/KAction>
#else
#include <QtWebKit/QWebView>
#include <QtGui/QAction>
#endif
#include "infopage.h"
#include "network.h"
#include <QDebug>

InfoPage::InfoPage(QWidget *parent)
    : QWidget(parent)
{
#ifdef ENABLE_KDE_SUPPORT
    KXMLGUIClient *client=dynamic_cast<KXMLGUIClient *>(parent);
    if (client) {
        refreshAction = client->actionCollection()->addAction("refreshinfo");
    } else {
        refreshAction = new QAction(this);
    }
    refreshAction->setText(i18n("Refresh"));
#else
    refreshAction = new QAction(tr("Refresh"), this);
#endif
    QGridLayout *layout=new QGridLayout(this);
    view=new WebView(this);
    QToolButton *refreshBtn=new QToolButton(this);
    combo=new QComboBox(this);
#ifdef ENABLE_KDE_SUPPORT
    combo->insertItem(0, i18n("Artist Information"));
    combo->insertItem(1, i18n("Album Information"));
#else
    combo->insertItem(0, tr("Artist Information"));
    combo->insertItem(1, tr("Album Information"));
#endif
    layout->addWidget(view, 0, 0, 1, 3);
    layout->addWidget(refreshBtn, 1, 0, 1, 1);
    layout->addItem(new QSpacerItem(2, 2, QSizePolicy::Expanding, QSizePolicy::Minimum), 1, 1, 1, 1);
    layout->addWidget(combo, 1, 2, 1, 1);
    layout->setContentsMargins(0, 0, 0, 0);

    refreshAction->setIcon(QIcon::fromTheme("view-refresh"));
    refreshBtn->setAutoRaise(true);
    refreshBtn->setDefaultAction(refreshAction);
    connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(changeView()));
    connect(refreshAction, SIGNAL(triggered()), SLOT(refresh()));
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

void InfoPage::refresh()
{
    lastWikiQuestion.clear();
    view->setHtml(QString());
    fetchInfo();
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
        int end = answer.indexOf( "\"", start );
        answer = answer.mid( start, end - start );
    }
    else {
        answer.clear();
    }

    if ( !answer.contains("wikipedia.org") || answer.contains( QRegExp("/.*:") ) ) {
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
    song=s;
    fetchInfo();
}

void InfoPage::fetchInfo()
{
    QString question = 0==combo->currentIndex() ? song.artist : (song.artist.isEmpty() && song.album.isEmpty() ? song.title : song.artist + " " + song.album);
    if ( !question.isEmpty() && lastWikiQuestion != question ) {
        lastWikiQuestion = question;
        askGoogle( question );
//         fetchWiki( artist );
    }
}
