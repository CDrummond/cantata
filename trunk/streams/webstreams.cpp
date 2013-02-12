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

#include "webstreams.h"
#include "networkaccessmanager.h"
#include "streamsmodel.h"
#include "localize.h"
#include "song.h"
#include <QXmlStreamReader>
#include <QFile>

enum Type {
    WS_IceCast,
    WS_SomaFm,
    WS_Radio,

    WS_Count
};

static QList<WebStream *> providers;

QList<WebStream *> WebStream::getAll()
{
    if (providers.isEmpty()) {
        QFile f(":streams.xml");
        if (f.open(QIODevice::ReadOnly)) {
            QXmlStreamReader doc(&f);
            while (!doc.atEnd()) {
                doc.readNext();

                if (doc.isStartElement() && QLatin1String("stream")==doc.name()) {
                    QString name=doc.attributes().value("name").toString();
                    unsigned int type=doc.attributes().value("type").toString().toUInt();
                    QUrl url=QUrl(doc.attributes().value("url").toString());
                    switch (type) {
                    case WS_IceCast: providers.append(new IceCastWebStream(name, doc.attributes().value("region").toString(), url)); break;
                    case WS_SomaFm:  providers.append(new SomaFmWebStream(name, doc.attributes().value("region").toString(), url)); break;
                    case WS_Radio:   providers.append(new RadioWebStream(name, doc.attributes().value("region").toString(), url)); break;
                    default: break;
                    }
                }
            }
        }
    }
    return providers;
}

WebStream * WebStream::get(const QUrl &url)
{
    foreach (WebStream *p, providers) {
        if (p->url==url) {
            return p;
        }
    }

    return 0;
}

void WebStream::download()
{
    if (job) {
        return;
    }
    job=NetworkAccessManager::self()->get(QNetworkRequest(url));
    connect(job, SIGNAL(finished()), this, SLOT(downloadFinished()));
}

void WebStream::cancelDownload()
{
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(downloadFinished()));
        job->deleteLater();
        job=0;
    }
}

void WebStream::downloadFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    if(QNetworkReply::NoError==reply->error()) {
        QList<StreamsModel::StreamItem *> streams=parse(reply);

        if (streams.isEmpty()) {
            emit error(i18nc("message \n url", "No streams downloaded from %1\n(%2)").arg(name).arg(url.toString()));
        } else {
            StreamsModel::self()->add(name, streams);
        }
    } else {
        emit error(i18nc("message \n url", "Failed to download streams from %1\n(%2)").arg(name).arg(url.toString()));
    }
    job=0;

    reply->deleteLater();
    emit finished();
}

static QString fixSingleGenre(const QString &g)
{
    if (g.length()) {
        QString genre=Song::capitalize(g);
        genre[0]=genre[0].toUpper();
        genre=genre.trimmed();
        genre=genre.replace(QLatin1String("Afrocaribbean"), QLatin1String("Afro-Caribbean"));
        genre=genre.replace(QLatin1String("Afro Caribbean"), QLatin1String("Afro-Caribbean"));
        if (genre.length() < 3 ||
                QLatin1String("The")==genre || QLatin1String("All")==genre ||
                QLatin1String("Various")==genre || QLatin1String("Unknown")==genre ||
                QLatin1String("Misc")==genre || QLatin1String("Mix")==genre || QLatin1String("100%")==genre ||
                genre.contains("ÃÂ") || // Broken unicode.
                genre.contains(QRegExp("^#x[0-9a-f][0-9a-f]"))) { // Broken XML entities.
            return QString();
        }

        if (genre==QLatin1String("R&B") || genre==QLatin1String("R B") || genre==QLatin1String("Rnb") || genre==QLatin1String("RnB")) {
            return QLatin1String("R&B");
        }
        if (genre==QLatin1String("Classic") || genre==QLatin1String("Classical")) {
            return QLatin1String("Classical");
        }
        if (genre==QLatin1String("Christian") || genre.startsWith(QLatin1String("Christian "))) {
            return QLatin1String("Christian");
        }
        if (genre==QLatin1String("Rock") || genre.startsWith(QLatin1String("Rock "))) {
            return QLatin1String("Rock");
        }
        if (genre==QLatin1String("Electronic") || genre==QLatin1String("Electronica") || genre==QLatin1String("Electric")) {
            return QLatin1String("Electronic");
        }
        if (genre==QLatin1String("Easy") || genre==QLatin1String("Easy Listening")) {
            return QLatin1String("Easy Listening");
        }
        if (genre==QLatin1String("Hit") || genre==QLatin1String("Hits") || genre==QLatin1String("Easy listening")) {
            return QLatin1String("Hits");
        }
        if (genre==QLatin1String("Hip") || genre==QLatin1String("Hiphop") || genre==QLatin1String("Hip Hop") || genre==QLatin1String("Hop Hip")) {
            return QLatin1String("Hip Hop");
        }
        if (genre==QLatin1String("News") || genre==QLatin1String("News talk")) {
            return QLatin1String("News");
        }
        if (genre==QLatin1String("Top40") || genre==QLatin1String("Top 40") || genre==QLatin1String("40Top") || genre==QLatin1String("40 Top")) {
            return QLatin1String("Top 40");
        }

        QStringList small=QStringList() << QLatin1String("Adult Contemporary") << QLatin1String("Alternative")
                                        << QLatin1String("Community Radio") << QLatin1String("Local Service")
                                        << QLatin1String("Multiultural") << QLatin1String("News")
                                        << QLatin1String("Student") << QLatin1String("Urban");

        foreach (const QString &s, small) {
            if (genre==s || genre.startsWith(s+" ") || genre.endsWith(" "+s)) {
                return s;
            }
        }

        // Convert XX's to XXs
        if (genre.contains(QRegExp("^[0-9]0's$"))) {
            genre=genre.remove('\'');
        }
        if (genre.length()>25 && (0==genre.indexOf(QRegExp("^[0-9]0s ")) || 0==genre.indexOf(QRegExp("^[0-9]0 ")))) {
            int pos=genre.indexOf(' ');
            if (pos>1) {
                genre=genre.left(pos);
            }
        }
        // Convert 80 -> 80s.
        return genre.contains(QRegExp("^[0-9]0$")) ? genre + 's' : genre;
    }
    return g;
}

static QString fixGenres(const QString &genre)
{
    QString g(genre);
    int pos=g.indexOf("<br");
    if (pos>3) {
        g=g.left(pos);
    }
    pos=g.indexOf("(");
    if (pos>3) {
        g=g.left(pos);
    }

    g=Song::capitalize(g);
    QStringList genres=g.split('|', QString::SkipEmptyParts);
    QStringList allGenres;

    foreach (const QString &genre, genres) {
        allGenres+=genre.split('/', QString::SkipEmptyParts);
    }

    QStringList fixed;
    foreach (const QString &genre, allGenres) {
        fixed.append(fixSingleGenre(genre));
    }
    return fixed.join(StreamsModel::constGenreSeparator);
}

static void trimGenres(QMultiHash<QString, StreamsModel::StreamItem *> &genres)
{
    QSet<QString> genreSet = genres.keys().toSet();
    foreach (const QString &genre, genreSet) {
        if (genres.count(genre) < 3) {
            const QList<StreamsModel::StreamItem *> &smallGnre = genres.values(genre);
            foreach (StreamsModel::StreamItem* s, smallGnre) {
                s->genres.remove(genre);
            }
        }
    }
}

QList<StreamsModel::StreamItem *> IceCastWebStream::parse(QIODevice *dev)
{
    QList<StreamsModel::StreamItem *> streams;
    QString name;
    QUrl url;
    QString genre;
    QSet<QString> names;
    QMultiHash<QString, StreamsModel::StreamItem *> genres;
    int level=0;
    QXmlStreamReader doc(dev);

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            ++level;
            if (2==level && QLatin1String("entry")==doc.name()) {
                name=QString();
                url=QUrl();
                genre=QString();
            } else if (3==level) {
                if (QLatin1String("server_name")==doc.name()) {
                    name=doc.readElementText();
                    --level;
                } else if (QLatin1String("genre")==doc.name()) {
                    genre=fixGenres(doc.readElementText());
                    --level;
                } else if (QLatin1String("listen_url")==doc.name()) {
                    url=QUrl(doc.readElementText());
                    --level;
                }
            }
        } else if (doc.isEndElement()) {
            if (2==level && QLatin1String("entry")==doc.name() && !name.isEmpty() && url.isValid() && !names.contains(name)) {
                StreamsModel::StreamItem *item=new StreamsModel::StreamItem(name, genre, QString(), url);
                streams.append(item);
                foreach (const QString &genre, item->genres) {
                    genres.insert(genre, item);
                }
                names.insert(item->name);
            }
            --level;
        }
    }
    trimGenres(genres);
    return streams;
}

QList<StreamsModel::StreamItem *> SomaFmWebStream::parse(QIODevice *dev)
{
    QList<StreamsModel::StreamItem *> streams;
    QSet<QString> names;
    QString streamFormat;
    QMultiHash<QString, StreamsModel::StreamItem *> genres;
    QString name;
    QUrl url;
    QString genre;
    int level=0;
    QXmlStreamReader doc(dev);

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            ++level;
            if (2==level && QLatin1String("channel")==doc.name()) {
                name=QString();
                url=QUrl();
                genre=QString();
                streamFormat=QString();
            } else if (3==level) {
                if (QLatin1String("title")==doc.name()) {
                    name=doc.readElementText();
                    --level;
                } else if (QLatin1String("genre")==doc.name()) {
                    genre=fixGenres(doc.readElementText());
                    --level;
                } else if (QLatin1String("fastpls")==doc.name()) {
                    if (streamFormat.isEmpty() || QLatin1String("mp3")!=streamFormat) {
                        streamFormat=doc.attributes().value("format").toString();
                        url=QUrl(doc.readElementText());
                        --level;
                    }
                }
            }
        } else if (doc.isEndElement()) {
            if (2==level && QLatin1String("channel")==doc.name() && !name.isEmpty() && url.isValid() && !names.contains(name)) {
                StreamsModel::StreamItem *item=new StreamsModel::StreamItem(name, genre, QString(), url);
                streams.append(item);
                foreach (const QString &genre, item->genres) {
                    genres.insert(genre, item);
                }
                names.insert(item->name);
            }
            --level;
        }
    }

    trimGenres(genres);
    return streams;
}

struct Stream {
    enum Format {
        AAC,
        MP3,
        OGG,
        WMA,
        Unknown
    };

    Stream() : format(Unknown), bitrate(0) { }
    bool operator<(const Stream &o) const {
        return weight()>o.weight();
    }

    int weight() const {
        return ((format&0x0f)<<8)+(bitrate&0xff);
    }

    void setFormat(const QString &f) {
        if (QLatin1String("mp3")==f.toLower()) {
            format=MP3;
        } else if (QLatin1String("aacplus")==f.toLower()) {
            format=AAC;
        } else if (QLatin1String("ogg vorbis")==f.toLower()) {
            format=OGG;
        } else if (QLatin1String("windows media")==f.toLower()) {
            format=WMA;
        } else {
            format=Unknown;
        }
    }

    QUrl url;
    Format format;
    unsigned int bitrate;
};

struct StationEntry {
    StationEntry() { clear(); }
    void clear() { name=location=comment=QString(); streams.clear(); }
    QString name;
    QString location;
    QString comment;
    QList<Stream> streams;
};

static QString getString(QString &str, const QString &start, const QString &end)
{
    QString rv;
    int b=str.indexOf(start);
    int e=-1==b ? -1 : str.indexOf(end, b+start.length());
    if (-1!=e) {
        rv=str.mid(b+start.length(), e-(b+start.length())).trimmed();
        str=str.mid(e+end.length());
    }
    return rv;
}

QList<StreamsModel::StreamItem *> RadioWebStream::parse(QIODevice *dev)
{
    QList<StreamsModel::StreamItem *> streams;
//    QMultiHash<QString, StreamsModel::StreamItem *> genres;
    QSet<QString> names;

    if (dev) {
        StationEntry entry;

        while (!dev->atEnd()) {
            QString line=dev->readLine().trimmed().replace("> <", "><").replace("<td><b><a href", "<td><a href").replace("</b></a></b>", "</b></a>");
            if ("<tr>"==line) {
                entry.clear();
            } else if (line.startsWith("<td><a href=")) {
                if (entry.name.isEmpty()) {
                    entry.name=getString(line, "<b>", "</b>");
                    QString extra=getString(line, "</a>", "</td>");
                    if (!extra.isEmpty()) {
                        entry.name+=" "+extra;
                    }
                } else {
                    // Station URLs...
                    QString url;
                    QString bitrate;
                    int idx=0;
                    do {
                        url=getString(line, "href=\"", "\"");
                        bitrate=getString(line, ">", " Kbps");
                        if (!url.isEmpty() && !bitrate.isEmpty() && !url.startsWith(QLatin1String("javascript"))) {
                            if (idx<entry.streams.count()) {
                                entry.streams[idx].url=url;
                                entry.streams[idx].bitrate=bitrate.toUInt();
                                idx++;
                            }
                        }
                    } while (!url.isEmpty() && !bitrate.isEmpty());
                }
            } else if (line.startsWith("<td><img src")) {
                // Station formats...
                QString format;
                do {
                    format=getString(line, "alt=\"", "\"");
                    if (!format.isEmpty()) {
                        Stream stream;
                        stream.setFormat(format);
                        entry.streams.append(stream);
                    }
                } while (!format.isEmpty());
            } else if (line.startsWith("<td>")) {
                if (entry.location.isEmpty()) {
                    entry.location=getString(line, "<td>", "</td>");
                } else {
                    entry.comment=getString(line, "<td>", "</td>");
                }
            } else if ("</tr>"==line) {
                if (entry.streams.count()) {
                    QString name=QLatin1String("National")==entry.location ? entry.name : (entry.name+" ("+entry.location+")");
                    QUrl url=entry.streams.at(0).url;

                    if (!names.contains(name) && !name.isEmpty() && url.isValid()) {
                        qSort(entry.streams);
                        QString genre=fixGenres(entry.comment);
                        StreamsModel::StreamItem *item=new StreamsModel::StreamItem(name, genre, QString(), url);
                        streams.append(item);
//                        foreach (const QString &genre, item->genres) {
//                            genres.insert(genre, item);
//                        }
                        names.insert(item->name);
                    }
                }
            }
        }
    }

//    trimGenres(genres);
    return streams;
}
