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
 
#include "contextpage.h"
#include "artistview.h"
#include "albumview.h"
#include "songview.h"
#include "song.h"
#include "utils.h"
#include "covers.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QPainter>
#include <QNetworkReply>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QXmlStreamReader>
#include <QFile>
#include <QDebug>

static const QLatin1String constApiKey("TEST_KEY"); // TODO: Apply for API key!!!
const QLatin1String ContextPage::constCacheDir("backdrops/");

static QString cacheFileName(const QString &artist, bool createDir)
{
    return Utils::cacheDir(ContextPage::constCacheDir, createDir)+Covers::encodeName(artist)+".jpg";
}

ContextPage::ContextPage(QWidget *parent)
    : QWidget(parent)
    , job(0)
    , drawBackdrop(true)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    artist = new ArtistView(this);
    album = new AlbumView(this);
    song = new SongView(this);

    int m=layout->margin();
    layout->setMargin(0);
    layout->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(artist);
    layout->addWidget(album);
    layout->addWidget(song);
    layout->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->setStretch(1, 1);
    layout->setStretch(2, 1);
    layout->setStretch(3, 1);
    setLayout(layout);
    connect(artist, SIGNAL(findArtist(QString)), this, SIGNAL(findArtist(QString)));
    connect(album, SIGNAL(playSong(QString)), this, SIGNAL(playSong(QString)));
    connect(artist, SIGNAL(haveBio(QString,QString)), album, SLOT(artistBio(QString,QString)), Qt::QueuedConnection);
    useBackdrop(Settings::self()->contextBackdrop());
}

void ContextPage::useBackdrop(bool u)
{
    if (u!=drawBackdrop) {
        drawBackdrop=u;
        if (isVisible() && !currentArtist.isEmpty()) {
            currentArtist.clear();
            updateArtist=currentArtist;
            updateBackdrop();
            QWidget::update();
        }
    }
}

void ContextPage::showEvent(QShowEvent *e)
{
    if (drawBackdrop) {
        updateBackdrop();
    }
    QWidget::showEvent(e);
}

void ContextPage::paintEvent(QPaintEvent *e)
{
    if (drawBackdrop && !backdrop.isNull()) {
        QPainter p(this);
        p.setOpacity(0.15);
        p.fillRect(rect(), QBrush(backdrop));
    }

    QWidget::paintEvent(e);
}

void ContextPage::update(const Song &s)
{
    artist->update(s);
    album->update(s);
    song->update(s);

    Song song=s;
    if (song.isVariousArtists()) {
        song.revertVariousArtists();
    }

    if (!song.albumartist.isEmpty() && !song.artist.isEmpty() && song.albumartist.length()<song.artist.length() && song.artist.startsWith(song.albumartist)) {
        updateArtist=song.albumartist;
    } else {
        updateArtist=song.artist;
    }

    updateArtist=Covers::fixArtist(updateArtist);
    if (isVisible() && drawBackdrop) {
        updateBackdrop();
    }
}

void ContextPage::cancel()
{
    if (job) {
        job->deleteLater();
        job=0;
    }
}

void ContextPage::updateBackdrop()
{
    if (updateArtist==currentArtist) {
        return;
    }
    currentArtist=updateArtist;
    if (currentArtist.isEmpty()) {
        if (!backdrop.isNull()) {
            backdrop=QImage();
            QWidget::update();
        }
        return;
    }
    QString fileName=cacheFileName(currentArtist, false);
    backdrop=QImage(fileName);
    if (backdrop.isNull()) {
        getBackdrop();
    } else {
        QWidget::update();
    }
}

void ContextPage::getBackdrop()
{
    cancel();
    QUrl url("http://htbackdrops.com/api/"+constApiKey+"/searchXML");
    #if QT_VERSION < 0x050000
    QUrl &q=url;
    #else
    QUrlQuery q;
    #endif

    q.addQueryItem(QLatin1String("keywords"), currentArtist);
    q.addQueryItem(QLatin1String("default_operator"), QLatin1String("and"));
    q.addQueryItem(QLatin1String("fields"), QLatin1String("title"));
    #if QT_VERSION >= 0x050000
    url.setQuery(q);
    #endif

    job=NetworkAccessManager::self()->get(url);
    connect(job, SIGNAL(finished()), this, SLOT(searchResponse()));
}

void ContextPage::searchResponse()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply || QNetworkReply::NoError!=reply->error()) {
        return;
    }

    QXmlStreamReader xml(reply);
    QString id;
    while (!xml.atEnd() && !xml.hasError() && id.isEmpty()) {
        xml.readNext();
        if (xml.isStartElement() && QLatin1String("search")==xml.name()) {
            while (xml.readNextStartElement() && id.isEmpty()) {
                if (xml.isStartElement() && QLatin1String("images")==xml.name()) {
                    while (xml.readNextStartElement() && id.isEmpty()) {
                        if (xml.isStartElement() && QLatin1String("image")==xml.name()) {
                            while (xml.readNextStartElement() && id.isEmpty()) {
                                if (xml.isStartElement() && QLatin1String("id")==xml.name()) {
                                    id=xml.readElementText();
                                } else {
                                    xml.skipCurrentElement();
                                }
                            }
                        } else {
                            xml.skipCurrentElement();
                        }
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        }
    }

    if (!id.isEmpty()) {
        QUrl url("http://htbackdrops.com/api/"+constApiKey+"/download/"+id+"/fullsize");
        job=NetworkAccessManager::self()->get(url);
        connect(job, SIGNAL(finished()), this, SLOT(downloadResponse()));
    }
}

void ContextPage::downloadResponse()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply || QNetworkReply::NoError!=reply->error()) {
        return;
    }

    QByteArray data=reply->readAll();
    backdrop=QImage::fromData(data);
    if (!backdrop.isNull()) {
        QFile f(cacheFileName(currentArtist, true));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        }
        QWidget::update();
    }
}

QNetworkReply * ContextPage::getReply(QObject *obj)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(obj);
    if (!reply) {
        return 0;
    }

    reply->deleteLater();
    if (reply!=job) {
        return 0;
    }
    job=0;
    return reply;
}
