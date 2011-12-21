/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* BE::MPC Qt4 client for MPD
 * Copyright (C) 2010-2011 Thomas Luebking <thomas.luebking@web.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <QtGui/QTextBrowser>
#include <QtGui/QGridLayout>
#include <QtGui/QComboBox>
#include <QtGui/QToolButton>
#include <QtCore/QDir>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#include <KDE/KXMLGUIClient>
#include <KDE/KActionCollection>
#include <KDE/KAction>
#else
#include <QtGui/QAction>
#endif
#include "infopage.h"
#include "network.h"

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
    text=new QTextBrowser(this);
    QToolButton *refreshBtn=new QToolButton(this);
    combo=new QComboBox(this);
#ifdef ENABLE_KDE_SUPPORT
    combo->insertItem(0, i18n("Artist Information"));
    combo->insertItem(1, i18n("Album Information"));
#else
    combo->insertItem(0, tr("Artist Information"));
    combo->insertItem(1, tr("Album Information"));
#endif
    layout->addWidget(text, 0, 0, 1, 3);
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

void InfoPage::askGoogle(const QString &query, bool useCache)
{
    if (query.isEmpty()) {
        return;
    }

    if (useCache) {
        QString plaintEntry = lastWikiQuestion.toLower();
        plaintEntry.replace('/', '_');
        QFile cache( Network::cacheDir("info") + plaintEntry );
        if ( cache.exists() && cache.open( QIODevice::ReadOnly ) ) {
            QString artistWiki;
            QDataStream stream( &cache );
            stream >> artistWiki;
            cache.close();
            text->setHtml( artistWiki );
            return; // shortcut ;-)
        }
    }

    QUrl question( "http://www.google.com/search?as_q=" + query + "&ft=i&num=1&as_sitesearch=wikipedia.org" );
    QString lang = QLocale::languageToString(QLocale::system().language());
    Network::self()->get( question, this, "gg2wp", lang );
}

void InfoPage::fetchWiki(QString query)
{
    QUrl wikiArtist("http://en.wikipedia.org/wiki/Special:Export/" + query.replace(' ', '_') );
//     QUrl wikiArtist("http://en.wikipedia.org/wiki/" + query.replace(' ', '_') );
    Network::self()->get( wikiArtist, this, "setArtistWiki" );
}

void InfoPage::refresh()
{
    lastWikiQuestion.clear();
    text->setText(QString());
    fetchInfo(false);
}

void InfoPage::changeView()
{
    fetchInfo();
}

void InfoPage::gg2wp(const QString &ans)
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
    wikiInfo.setEncodedUrl( answer.replace("/wiki/", "/wiki/Special:Export/").toUtf8() );
    Network::self()->get( wikiInfo, this, "setArtistWiki" );
}

void InfoPage::update(const Song &s)
{
    song=s;
    fetchInfo();
}

void InfoPage::fetchInfo(bool useCache)
{
    QString question = 0==combo->currentIndex() ? song.artist : (song.artist.isEmpty() && song.album.isEmpty() ? song.title : song.artist + " " + song.album);
    if ( !question.isEmpty() && lastWikiQuestion != question ) {
        lastWikiQuestion = question;
        askGoogle( question, useCache );
//         fetchWiki( artist );
    }
}

static QString strip(const QString &string, const QString &open, const QString &close, QString inner = QString())
{
    QString result;
    int next, left = 0;
    int pos = string.indexOf( open );

    if ( pos < 0 ) {
        return string;
    }

    if ( inner.isEmpty() ) {
        inner = open;
    }

    while ( pos > -1 )
    {
        result += string.mid( left, pos - left );
        left = string.indexOf( close, pos );
        if ( left < 0 ) {// opens, but doesn't close
            break;
        } else {
            next = pos;
            while ( next > -1 && left > -1 ) {   // search for inner iterations
                int count = 0;
                int lastNext = next;
                while ( (next = string.indexOf( inner, next+inner.length() )) < left && next > -1 ) {   // count inner section openers
                    lastNext = next;
                    ++count;
                }
                next = lastNext; // set back next to last inside opener for next iteration

                if ( !count ) { // no inner sections, skip
                    break;
                }

                for ( int i = 0; i < count; ++i ) {// shift section closers by inside section amount
                    left = string.indexOf( close, left+close.length() );
                }
                // "continue" - search for next inner section
            }

            if ( left < 0 ) {// section does not close, skip here
                break;
            }

            left += close.length(); // extend close to next search start
        }

        if ( left < 0 ) {// section does not close, skip here
            break;
        }

        pos = string.indexOf( open, left ); // search next 1st level section opener
    }

    if ( left > -1 ) {// append last part
        result += string.mid( left );
    }

    return result;
}

void InfoPage::setArtistWiki(const QString &aw)
{
//     // check for redirects
//     int redirect = artistWiki.indexOf( "#REDIRECT" );
//     if ( redirect > -1 )
//     {
//         int end = artistWiki.indexOf( ']', redirect );
//         int begin = artistWiki.indexOf( '[', -end ) + 2;
//         QString s = artistWiki.mid( begin, end - begin ).trimmed();
//         wiki->deleteLater();
//         fetchWiki( s );
//         return;
//     }

    QString artistWiki(aw);
    int start = artistWiki.indexOf('>', artistWiki.indexOf("<text") ) + 1;
    int end = artistWiki.lastIndexOf( QRegExp("\\n[^\\n]*\\n\\{\\{reflist", Qt::CaseInsensitive) );
    if ( end < start ) {
        end = INT_MAX;
    }
    int e = artistWiki.lastIndexOf( QRegExp("\\n[^\\n]*\\n&lt;references", Qt::CaseInsensitive) );
    if ( e > start && e < end ) {
        end = e;
    }
    e = artistWiki.lastIndexOf( QRegExp("\n==\\s*Sources\\s*==") );
    if ( e > start && e < end ) {
        end = e;
    }
    e = artistWiki.lastIndexOf( QRegExp("\n==\\s*Notes\\s*==") );
    if ( e > start && e < end ) {
        end = e;
    }
    e = artistWiki.lastIndexOf( QRegExp("\n==\\s*References\\s*==") );
    if ( e > start && e < end ) {
        end = e;
    }
    e = artistWiki.lastIndexOf( QRegExp("\n==\\s*External links\\s*==") );
    if ( e > start && e < end ) {
        end = e;
    }
    if ( end < start ) {
        end = artistWiki.lastIndexOf("</text");
    }

    artistWiki = artistWiki.mid( start, end - start ); // strip header/footer
    artistWiki = strip( artistWiki, "{{", "}}" ); // strip wiki internal stuff
    artistWiki.replace("&lt;", "<").replace("&gt;", ">");
    artistWiki = strip( artistWiki, "<!--", "-->" ); // strip comments
    artistWiki.remove( QRegExp( "<ref[^>]*/>" ) ); // strip inline refereces
    artistWiki = strip( artistWiki, "<ref", "</ref>", "<ref" ); // strip refereces
//     artistWiki = strip( artistWiki, "<ref ", "</ref>", "<ref" ); // strip argumented refereces
    artistWiki = strip( artistWiki, "[[File:", "]]", "[[" ); // strip images etc
    artistWiki = strip( artistWiki, "[[Image:", "]]", "[[" ); // strip images etc

    artistWiki.replace( QRegExp("\\[\\[[^\\[\\]]*\\|([^\\[\\]\\|]*)\\]\\]"), "\\1"); // collapse commented links
    artistWiki.remove("[[").remove("]]"); // remove wiki link "tags"

    artistWiki = artistWiki.trimmed();

//     artistWiki.replace( QRegExp("\\n\\{\\|[^\\n]*wikitable[^\\n]*\\n!"), "\n<table><th>" );

    artistWiki.replace("\n\n", "<br>");
//     artistWiki.replace("\n\n", "</p><p align=\"justify\">");
    artistWiki.replace( QRegExp("\\n'''([^\\n]*)'''\\n"), "<hr><b>\\1</b>\n" );
    artistWiki.replace( QRegExp("\\n\\{\\|[^\\n]*\\n"), "\n" );
    artistWiki.replace( QRegExp("\\n\\|[^\\n]*\\n"), "\n" );
    artistWiki.replace("\n*", "<br>");
    artistWiki.replace("\n", "");
    artistWiki.replace("'''", "¬").replace( QRegExp("¬([^¬]*)¬"), "<b>\\1</b>" );
    artistWiki.replace("''", "¬").replace( QRegExp("¬([^¬]*)¬"), "<i>\\1</i>" );
    artistWiki.replace("===", "¬").replace( QRegExp("¬([^¬]*)¬"), "<h3>\\1</h3>" );
    artistWiki.replace("==", "¬").replace( QRegExp("¬([^¬]*)¬"), "<h2>\\1</h2>" );
    artistWiki.replace("&amp;nbsp;", " ");
    artistWiki.replace("<br><h", "<h");
    artistWiki.replace("</h2><br>", "</h2>");
    artistWiki.replace("</h3><br>", "</h3>");

    text->setHtml( artistWiki );

    QString plaintEntry = lastWikiQuestion.toLower();
    plaintEntry.replace('/', '_');
    QFile cache( Network::cacheDir("info") + plaintEntry );
    if ( cache.open(  QIODevice::WriteOnly ) ) {
        QDataStream stream( &cache );
        stream << artistWiki;
        cache.close();
    }
}
