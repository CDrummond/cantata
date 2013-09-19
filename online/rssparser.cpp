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

#include "rssparser.h"
#include <QXmlStreamReader>
#include <QStringList>

static const char * constITunesNameSpace = "http://www.itunes.com/dtds/podcast-1.0.dtd";
static const char * constMediaNameSpace = "http://search.yahoo.com/mrss/";

using namespace RssParser;

static bool parseUntil(QXmlStreamReader &reader, const QString &elem)
{
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == elem) {
            return true;
        }
    }
    return false;
}

static void consumeCurrentElement(QXmlStreamReader &reader)
{
    int level = 1;
    while (0!=level && !reader.atEnd()) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: ++level; break;
            case QXmlStreamReader::EndElement:   --level; break;
            default: break;
        }
    }
}

static QUrl parseImage(QXmlStreamReader &reader)
{
    QUrl url;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (QLatin1String("url")==reader.name()) {
                url=QUrl::fromEncoded(reader.readElementText().toLatin1());
            } else {
                consumeCurrentElement(reader);
            }
        } else if (reader.isEndElement()) {
            break;
        }
    }
    return url;
}

static Episode parseEpisode(QXmlStreamReader &reader)
{
    Episode ep;

    while (!reader.atEnd()) {
        reader.readNext();
        const QStringRef name = reader.name();

        if (reader.isStartElement()) {
            if (QLatin1String("title")==name) {
                ep.name=reader.readElementText();
            } else if (QLatin1String("duration")==name && constITunesNameSpace==reader.namespaceUri()) {
                QStringList parts = reader.readElementText().split(':');
                if (2==parts.count()) {
                    ep.duration=(parts[0].toInt() * 60) + parts[1].toInt();
                } else if (parts.count()>=3) {
                    ep.duration=(parts[0].toInt() * 60*60) + (parts[1].toInt() * 60) + parts[2].toInt();
                }
            } else if (0==ep.duration && QLatin1String("content")==name && constMediaNameSpace==reader.namespaceUri()) {
                ep.duration=reader.attributes().value(QLatin1String("duration")).toString().toUInt();
            } else if (QLatin1String("enclosure")==name) {
                if (reader.attributes().value(QLatin1String("type")).toString().startsWith(QLatin1String("audio/"))) {
                    ep.url=QUrl::fromEncoded(reader.attributes().value(QLatin1String("url")).toString().toAscii());
                }
                consumeCurrentElement(reader);
            } else {
                consumeCurrentElement(reader);
            }
        } else if (reader.isEndElement()) {
            break;
        }
    }

    return ep;
}

Channel RssParser::parse(QIODevice *dev)
{
    Channel ch;
    QXmlStreamReader reader(dev);
    if (parseUntil(reader, QLatin1String("rss")) && parseUntil(reader, QLatin1String("channel"))) {
        while (!reader.atEnd()) {
            reader.readNext();

            if (reader.isStartElement()) {
                const QStringRef name = reader.name();
                if (ch.name.isEmpty() && QLatin1String("title")==name) {
                    ch.name=reader.readElementText();
                } else if (QLatin1String("image")==name && ch.image.isEmpty()) {
                    if (constITunesNameSpace==reader.namespaceUri()) {
                        ch.image=reader.attributes().value(QLatin1String("href")).toString();
                        consumeCurrentElement(reader);
                    } else {
                        ch.image=parseImage(reader);
                    }
                } else if (QLatin1String("item")==name) {
                    Episode ep=parseEpisode(reader);
                    if (!ep.name.isEmpty() && !ep.url.isEmpty()) {
                        ch.episodes.append(ep);
                    }
                } else {
                    consumeCurrentElement(reader);
                }
            } else if (reader.isEndElement()) {
                break;
            }
        }
    }

    return ch;
}
