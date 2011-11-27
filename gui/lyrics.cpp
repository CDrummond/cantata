/**************************************************************************
* Converted to C++ from the Amarok lyrics script...
*
*   Amarok 2 lyrics script to fetch lyrics from lyrics.wikia.com          *
*   (formerly lyricwiki.org)                                              *
*                                                                         *
*   Copyright                                                             *
*   (C) 2008 Aaron Reichman <reldruh@gmail.com>                           *
*   (C) 2008 Leo Franchi <lfranchi@kde.org>                               *
*   (C) 2008 Mark Kretschmann <kretschmann@kde.org>                       *
*   (C) 2008 Peter ZHOU <peterzhoulei@gmail.org>                          *
*   (C) 2009 Jakob Kummerow <jakob.kummerow@gmail.com>                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
**************************************************************************/

#include "lyrics.h"
#include "musiclibrarymodel.h"
#include "lib/song.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KIO/AccessManager>
#endif
#include <QtXml/QDomDocument>
#include <QtGui/QTextDocument>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QTextStream>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

// url to get lyrics using mediawiki API
static const QLatin1String constApiUrl("http://lyrics.wikia.com/api.php?action=query&prop=revisions&rvprop=content&format=xml&titles=");

static const int constMaxRedirects=3;

static QString changeExt(const QString &f, const QString &newExt)
{
    QString newStr(f);
    int     dotPos(newStr.lastIndexOf('.'));

    if (-1==dotPos) {
        newStr+=newExt;
    } else {
        newStr.remove(dotPos, newStr.length());
        newStr+=newExt;
    }
    return newStr;
}

struct Decoded
{
    enum Type
    {
        Empty,
        Lyric,
        Redirect
    };

    Type type;
    QString value;

    Decoded() : type(Empty) { }
    Decoded(Type t, const QString &v) : type(t), value(v) { }
};

static Decoded decode(const QByteArray &data)
{
    QDomDocument doc;
    doc.setContent(data);
    QString response = doc.elementsByTagName("rev").at(0).toElement().text();
    qDebug() << "DECODE:" << response;

    int open=response.indexOf("<lyric");
    int close=-1==open ? -1 : response.indexOf("</lyric", open);

    if (-1!=close) {
        if(response[open+6]=='s') {
            open+=8;
        } else {
            open+=7;
        }

        if(response[open]=='\n') {
            open++;
        }
        if(response[close]=='\n') {
            close--;
        }
        return Decoded(Decoded::Lyric, response.mid(open, close-open));
    }

    if (response.startsWith("#REDIRECT [[", Qt::CaseInsensitive)) {
        open=QString("#REDIRECT [[").length();
        close=response.indexOf("]]", open);
        return Decoded(Decoded::Redirect, constApiUrl+QUrl::toPercentEncoding(response.mid(open, close-open)));
    }

    return Decoded();
}

// build a URL component out of a string containing an artist or a song title
static QString convertToUrl(const QString &str)
{
    // replace (erroneously used) accent ` with a proper apostrophe '
    QString string(str);
    string.replace( "`", "'" );
    // split into words, then treat each word separately
    QStringList words = string.split( " " );
    for ( int i = 0; i < words.length(); i++ ) {
        int upper = 1; // normally, convert first character only to uppercase, but:
        // if we have a Roman numeral (well, at least either of "ii", "iii"), convert all "i"s
        if ( words[i].at(0).toUpper() == QChar('I') ) {
            // count "i" letters
            while ( words[i].length() > upper && words[i].at(upper).toUpper() == QChar('I') ) {
                upper++;
            }
        }
        // if the word starts with an apostrophe or parenthesis, the next character has to be uppercase
        if ( words[i].at(0) == QChar('\'') || words[i].at(0) == QChar('(') ) {
            upper++;
        }
        // finally, perform the capitalization
        if ( upper < words[i].length() ) {
            words[i] = words[i].mid( 0, upper ).toUpper() + words[i].mid( upper );
        } else {
            words[i] = words[i].toUpper();
        }
        // now take care of more special cases
        // names like "McSomething"
        if ( words[i].mid( 0, 2 ) == QLatin1String("Mc") ) {
            words[i] = QLatin1String("Mc") + words[i][2].toUpper() + words[i].mid( 3 );
        }
        // URI-encode the word
        words[i] = QUrl::toPercentEncoding(words[i]);
    }
    // join the words back together and return the result
    return words.join( "_" );
}

// convert all HTML entities to their applicable characters
static QString entityDecode(const QString &string)
{
    QString convertXml = QLatin1String("<?xml version=\"1.0\" encoding=\"UTF-8\" ?><body><entity>") + string + QLatin1String("</entity></body>");
    QDomDocument doc;
    if (doc.setContent(convertXml))
    { // xml is valid
        return doc.elementsByTagName( "entity" ).at( 0 ).toElement().text();
    }

    return string;
}

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(Lyrics, instance)
#endif

Lyrics * Lyrics::self()
{
#ifdef ENABLE_KDE_SUPPORT
    return instance;
#else
    static Lyrics *instance=0;;
    if(!instance) {
        instance=new Lyrics;
    }
    return instance;
#endif
}

Lyrics::Lyrics()
    : manager(0)
{
}

static const QLatin1String constKeySep("<<##>>");
static const QLatin1String constLyricsDir("lyrics/");
static const QLatin1String constExtension(".lyrics");

QString cacheFile(QString artist, QString title, bool createDir=false)
{
    title.replace("/", "_");
    artist.replace("/", "_");
    return QDir::toNativeSeparators(MusicLibraryModel::cacheDir(constLyricsDir+artist+'/', createDir))+title+constExtension;
}

void Lyrics::get(const Song &song)
{
    if (song.artist.isEmpty() || song.title.isEmpty()) {
        return;
    }

    if (!mpdDir.isEmpty()) {
        // Check for MPD file...
        QString mpdLyrics=changeExt(mpdDir+song.file, constExtension);

        if (QFile::exists(mpdLyrics)) {
            QFile f(mpdLyrics);

            if (f.open(QIODevice::ReadOnly)) {
                emit lyrics(song.artist, song.title, f.readAll());
                f.close();
                return;
            }
        }
    }

    // Check for cached file...
    QString file=cacheFile(song.artist, song.title);

    if (QFile::exists(file)) {
        QFile f(file);

        if (f.open(QIODevice::ReadOnly)) {
            emit lyrics(song.artist, song.title, f.readAll());
            f.close();
            return;
        }
    }

    // Check we are not aleady downloading...
    QString key=song.artist+constKeySep+song.title;
    if (jobs.contains(key)) {
        return;
    }

    // strip "featuring <someone else>" from the song.artist
    QString artistFixed=song.artist;
    QStringList toStrip;
    toStrip << QLatin1String(" ft. ") << QLatin1String(" feat. ") << QLatin1String(" featuring ");
    foreach (const QString s, toStrip) {
        int strip = artistFixed.toLower().indexOf( " ft. ");
        if ( strip != -1 ) {
            artistFixed = artistFixed.mid( 0, strip );
        }
    }

    if (!manager) {
//  QNetworkProxy proxy;
//  proxy.setType(QNetworkProxy::HttpProxy);
//  proxy.setHostName("proxy.fr.nds.com");
//  proxy.setPort(8080);
//  QNetworkProxy::setApplicationProxy(proxy);
#ifdef ENABLE_KDE_SUPPORT
        manager = new KIO::Integration::AccessManager(this);
#else
        manager=new QNetworkAccessManager(this);
#endif
        connect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(jobFinished(QNetworkReply *)));
    }

    QUrl url;
    url.setEncodedUrl(QString(constApiUrl + convertToUrl(entityDecode(artistFixed))+":"+convertToUrl(entityDecode(song.title))).toLatin1());
    qDebug() << "URL" << url.toString();
    QNetworkReply *reply=manager->get(QNetworkRequest(url));
    jobs.insert(key, Job(reply));
}

void Lyrics::jobFinished(QNetworkReply *reply)
{
    // TODO: Handle redirects: QNetworkRequest::RedirectionTargetAttribute
    QMap<QString, Job>::Iterator it(jobs.begin());
    QMap<QString, Job>::Iterator end(jobs.end());

    for (; it!=end; ++it) {
        if (it.value().reply==reply) {
            QStringList parts=it.key().split(constKeySep);
            Decoded decoded=QNetworkReply::NoError==reply->error() ? decode(reply->readAll()) : Decoded();

            switch(decoded.type) {
            case Decoded::Empty:
#ifdef ENABLE_KDE_SUPPORT
                emit lyrics(parts[0], parts[1], i18n("No lyrics found"));
#else
                emit lyrics(parts[0], parts[1], tr("No lyrics found"));
#endif
                jobs.remove(it.key());
                break;
            case Decoded::Lyric: {
                QFile f(cacheFile(parts[0], parts[1], true));

                if (f.open(QIODevice::WriteOnly)) {
                    QTextStream(&f) << decoded.value;
                    f.close();
                }

                emit lyrics(parts[0], parts[1], decoded.value);
                jobs.remove(it.key());
                break;
            }
            case Decoded::Redirect: {
                if(++(it.value().redirects) < constMaxRedirects) {
                    qDebug() << "Redirect URL" << decoded.value;
                    QNetworkReply *reply=manager->get(QNetworkRequest(QUrl(decoded.value)));
                    it.value().reply=reply;
                } else {
#ifdef ENABLE_KDE_SUPPORT
                    emit lyrics(parts[0], parts[1], i18n("No lyrics found"));
#else
                    emit lyrics(parts[0], parts[1], tr("No lyrics found"));
#endif
                    jobs.remove(it.key());
                }
                break;
            }
            }
        }
    }

    reply->deleteLater();
}
