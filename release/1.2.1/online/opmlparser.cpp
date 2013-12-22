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

#include "opmlparser.h"
#include <QXmlStreamReader>
#include <QStringList>

using namespace OpmlParser;

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

static void parseOutline(QXmlStreamReader &reader, Category &cat)
{
    while (!reader.atEnd()) {
        QXmlStreamReader::TokenType type = reader.readNext();
        switch (type) {
        case QXmlStreamReader::StartElement: {
            const QStringRef name = reader.name();
            if (name != QLatin1String("outline")) {
                consumeCurrentElement(reader);
                continue;
            }

            QXmlStreamAttributes attributes = reader.attributes();

            if (QLatin1String("rss")==attributes.value(QLatin1String("type")).toString() ||
                QLatin1String("link")==attributes.value(QLatin1String("type")).toString()) {
                // Parse the feed and add it to this container
                Podcast podcast;
                podcast.name=attributes.value("title").toString().trimmed();
                if (podcast.name.isEmpty()) {
                    podcast.name=attributes.value(QLatin1String("text")).toString();
                }
                podcast.description=attributes.value("description").toString().trimmed();
                if (podcast.description.isEmpty()) {
                    podcast.description=attributes.value(QLatin1String("text")).toString();
                }
                podcast.htmlUrl=attributes.value("htmlUrl").toString().trimmed();
                podcast.url=QUrl::fromEncoded(attributes.value(QLatin1String("xmlUrl")).toString().toLatin1());
                if (podcast.url.isEmpty()) {
                    podcast.url=QUrl::fromEncoded(attributes.value(QLatin1String("url")).toString().toLatin1());
                }
                podcast.image=QUrl::fromEncoded(attributes.value(QLatin1String("imageUrl")).toString().toLatin1());
                if (podcast.image.isEmpty()) {
                    podcast.image=QUrl::fromEncoded(attributes.value(QLatin1String("imageHref")).toString().toLatin1());
                }
                cat.podcasts.append(podcast);

                // Consume any children and the EndElement.
                consumeCurrentElement(reader);
            } else {
                // Create a new child container
                Category child;

                // Take the name from the fullname attribute first if it exists.
                child.name = attributes.value(QLatin1String("fullname")).toString().trimmed();
                if (child.name.isEmpty()) {
                    child.name = attributes.value(QLatin1String("title")).toString().trimmed();
                }
                if (child.name.isEmpty()) {
                    child.name = attributes.value(QLatin1String("text")).toString().trimmed();
                }

                // Parse its contents and add it to this container
                parseOutline(reader, child);
                cat.categories.append(child);
            }

            break;
        }
        case QXmlStreamReader::EndElement:
            return;
        default:
            break;
        }
    }
}

Category OpmlParser::parse(QIODevice *dev)
{
    Category cat;
    QXmlStreamReader reader(dev);
    if (parseUntil(reader, QLatin1String("body"))) {
        parseOutline(reader, cat);
    }

    return cat;
}

Category OpmlParser::parse(const QByteArray &data)
{
    Category cat;
    QXmlStreamReader reader(data);
    if (parseUntil(reader, QLatin1String("body"))) {
        parseOutline(reader, cat);
    }

    return cat;
}
