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

#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitempodcast.h"
#include "musiclibraryitemsong.h"
#include "onlineservicesmodel.h"
#include "onlineservice.h"
#include "song.h"
#include "icons.h"
#include "qtiocompressor/qtiocompressor.h"
#include "utils.h"
#include "covers.h"
#include "rssparser.h"
#include <QPixmap>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QCryptographicHash>
#include <QNetworkReply>

static QPixmap *theDefaultIcon=0;
static QPixmap *theDefaultLargeIcon=0;

static QLatin1String constTopTag("podcast");
static QLatin1String constImageAttribute("img");
static QLatin1String constRssAttribute("rss");
static QLatin1String constEpisodeTag("episode");
static QLatin1String constNameAttribute("name");
static QLatin1String constUrlAttribute("url");
static QLatin1String constTimeAttribute("time");
static QLatin1String constPlayedAttribute("played");
static QLatin1String constTrue("true");

const QString MusicLibraryItemPodcast::constExt=QLatin1String(".xml.gz");
const QString MusicLibraryItemPodcast::constDir=QLatin1String("podcasts");

static QString generateFileName(const QUrl &url, bool creatingNew)
{
    QString hash=QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Md5).toHex();
    QString dir=Utils::configDir(MusicLibraryItemPodcast::constDir, true);
    QString fileName=dir+hash+MusicLibraryItemPodcast::constExt;

    if (creatingNew) {
        int i=0;
        while (QFile::exists(fileName) && i<100) {
            fileName=dir+hash+QChar('_')+QString::number(i)+MusicLibraryItemPodcast::constExt;
            i++;
        }
    }

    return fileName;
}

MusicLibraryItemPodcast::MusicLibraryItemPodcast(const QString &fileName, MusicLibraryItemContainer *parent)
    : MusicLibraryItemContainer(QString(), parent)
    , m_coverIsDefault(false)
    , m_cover(0)
    , m_fileName(fileName)
    , m_unplayedEpisodeCount(0)
{
    if (!m_fileName.isEmpty()) {
        m_imageFile=m_fileName;
        m_imageFile=m_imageFile.replace(constExt, ".jpg");
    }
}

bool MusicLibraryItemPodcast::load()
{
    if (m_fileName.isEmpty()) {
        return false;
    }

    int track=0;
    QFile file(m_fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::ReadOnly)) {
        return false;
    }

    QXmlStreamReader reader(&compressor);
    m_unplayedEpisodeCount=0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.error() && reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                m_imageUrl=attributes.value(constImageAttribute).toString();
                m_rssUrl=attributes.value(constRssAttribute).toString();
                QString name=attributes.value(constNameAttribute).toString();
                if (m_rssUrl.isEmpty() || name.isEmpty()) {
                    return false;
                }
                m_itemData=name;
            } else if (constEpisodeTag == element) {
                QString name=attributes.value(constNameAttribute).toString();
                QString url=attributes.value(constUrlAttribute).toString();
                if (!name.isEmpty() && !url.isEmpty()) {
                    Song s;
                    s.title=name;
                    s.file=url;
                    s.track=++track;
                    s.artist=m_itemData;
                    s.setPlayed(constTrue==attributes.value(constPlayedAttribute).toString());
                    s.setPodcastImage(m_imageFile);
                    QString time=attributes.value(constTimeAttribute).toString();
                    s.time=time.isEmpty() ? 0 : time.toUInt();
                    MusicLibraryItemSong *song=new MusicLibraryItemSong(s, this);
                    m_childItems.append(song);
                    if (!s.hasbeenPlayed()) {
                        m_unplayedEpisodeCount++;
                    }
                }
            }
        }
    }

    return true;
}

static const QString constRssTag=QLatin1String("rss");

bool MusicLibraryItemPodcast::loadRss(QNetworkReply *dev)
{
    m_rssUrl=dev->url();
    if (m_fileName.isEmpty()) {
        m_fileName=m_imageFile=generateFileName(m_rssUrl, true);
        m_imageFile=m_imageFile.replace(constExt, ".jpg");
    }

    RssParser::Channel ch=RssParser::parse(dev);

    if (!ch.isValid()) {
        return false;
    }

    m_imageUrl=ch.image;
    m_itemData=ch.name;

    m_unplayedEpisodeCount=ch.episodes.count();
    int track=0;
    foreach (const RssParser::Episode &ep, ch.episodes) {
        Song s;
        s.title=ep.name;
        s.file=ep.url.toString(); // ????
        s.track=++track;
        s.artist=m_itemData;
        s.time=ep.duration;
        s.setPlayed(false);
        s.setPodcastImage(m_imageFile);
        MusicLibraryItemSong *song=new MusicLibraryItemSong(s, this);
        m_childItems.append(song);
    }

    return true;
}

bool MusicLibraryItemPodcast::save()
{
    if (m_fileName.isEmpty()) {
        return false;
    }

    QFile file(m_fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::WriteOnly)) {
        return false;
    }

    QXmlStreamWriter writer(&compressor);
    writer.writeStartElement(constTopTag);
    writer.writeAttribute(constImageAttribute, m_imageUrl.toString()); // ??
    writer.writeAttribute(constRssAttribute, m_rssUrl.toString()); // ??
    writer.writeAttribute(constNameAttribute, m_itemData);
    foreach (MusicLibraryItem *i, m_childItems) {
        const Song &s=static_cast<MusicLibraryItemSong *>(i)->song();
        writer.writeStartElement(constEpisodeTag);
        writer.writeAttribute(constNameAttribute, s.title);
        writer.writeAttribute(constUrlAttribute, s.file);
        if (s.time) {
            writer.writeAttribute(constTimeAttribute, QString::number(s.time));
        }
        if (s.hasbeenPlayed()) {
            writer.writeAttribute(constPlayedAttribute, constTrue);
        }
        writer.writeEndElement();
    }
    writer.writeEndElement();
    compressor.close();
    return true;
}

void MusicLibraryItemPodcast::setCoverImage(const QImage &img) const
{
    int size=MusicLibraryItemAlbum::iconSize(largeImages());
    QImage scaled=img.scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_cover = new QPixmap(QPixmap::fromImage(scaled));
    m_coverIsDefault=false;
}

bool MusicLibraryItemPodcast::setCover(const QImage &img, bool update) const
{
    if ((update || m_coverIsDefault) && !img.isNull()) {
        setCoverImage(img);
        return true;
    }

    return false;
}

const QPixmap & MusicLibraryItemPodcast::cover()
{
    if (m_coverIsDefault) {
        if (largeImages()) {
            if (theDefaultLargeIcon) {
                return *theDefaultLargeIcon;
            }
        } else if (theDefaultIcon) {
            return *theDefaultIcon;
        }
    }

    if (!m_cover) {
        bool useLarge=largeImages();
        int iSize=MusicLibraryItemAlbum::iconSize(useLarge);

        if ((useLarge && !theDefaultLargeIcon) || (!useLarge && !theDefaultIcon)) {
            int cSize=iSize;
            if (0==cSize) {
                cSize=22;
            }
            if (useLarge) {
                theDefaultLargeIcon = new QPixmap(Icons::self()->podcastIcon.pixmap(cSize, cSize)
                                                 .scaled(QSize(cSize, cSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            } else {
                theDefaultIcon = new QPixmap(Icons::self()->podcastIcon.pixmap(cSize, cSize)
                                            .scaled(QSize(cSize, cSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            }
        }
        m_coverIsDefault = true;
        QImage img=OnlineServicesModel::self()->requestImage(static_cast<OnlineService *>(parentItem())->id(), data(), QString(), m_imageUrl.toString(), // ??
                                                             m_imageFile, 300);

        if (!img.isNull()) {
            setCoverImage(img);
            return *m_cover;
        }
        return useLarge ? *theDefaultLargeIcon : *theDefaultIcon;
    }

    return *m_cover;
}

void MusicLibraryItemPodcast::remove(int row)
{
    delete m_childItems.takeAt(row);
    resetRows();
}

void MusicLibraryItemPodcast::remove(MusicLibraryItemSong *i)
{
    int idx=m_childItems.indexOf(i);
    if (-1!=idx) {
        remove(idx);
    }
}

void MusicLibraryItemPodcast::clearImage()
{
    if (!m_coverIsDefault) {
        m_coverIsDefault=true;
        delete m_cover;
        m_cover=0;
        if (theDefaultIcon) {
            m_cover=theDefaultIcon;
        }
    }
}

void MusicLibraryItemPodcast::removeFiles()
{
    if (!m_fileName.isEmpty() && QFile::exists(m_fileName)) {
        QFile::remove(m_fileName);
    }
    if (!m_imageFile.isEmpty() && QFile::exists(m_imageFile)) {
        QFile::remove(m_imageFile);
    }
}

void MusicLibraryItemPodcast::updateTrackNumbers()
{
    quint16 track=0;
    resetRows();
    m_unplayedEpisodeCount=0;
    foreach (MusicLibraryItem *i, m_childItems) {
        static_cast<MusicLibraryItemSong *>(i)->setTrack(++track);
        if (!static_cast<MusicLibraryItemSong *>(i)->song().hasbeenPlayed()) {
            m_unplayedEpisodeCount++;
        }
    }
}

void MusicLibraryItemPodcast::setPlayed(MusicLibraryItemSong *song)
{
    if (!song->song().hasbeenPlayed()) {
        song->setPlayed(true);
        m_unplayedEpisodeCount--;
    }
}

bool MusicLibraryItemPodcast::largeImages() const
{
    return m_parentItem && Type_Root==m_parentItem->itemType() &&
           static_cast<MusicLibraryItemRoot *>(m_parentItem)->useLargeImages();
}
