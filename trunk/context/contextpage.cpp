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
#include "wikipediaengine.h"
#include "localize.h"
#include "backdropcreator.h"
#include "musiclibrarymodel.h"
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QPainter>
#include <QNetworkReply>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QXmlStreamReader>
#include <QFile>
#include <QWheelEvent>
#include <QApplication>
#include <QComboBox>
#include <QStackedWidget>
#include <QDebug>

//#define DBUG qWarning() << metaObject()->className() << __FUNCTION__

#ifndef DBUG
#define DBUG qDebug()
#endif

static const QLatin1String constApiKey(""); // TODO: Apply for API key!!!
const QLatin1String ContextPage::constCacheDir("backdrops/");

static QString cacheFileName(const QString &artist, bool createDir)
{
    return Utils::cacheDir(ContextPage::constCacheDir, createDir)+Covers::encodeName(artist)+".jpg";
}

ContextPage::ContextPage(QWidget *parent)
    : QWidget(parent)
    , job(0)
    , drawBackdrop(true)
    , darkBackground(false)
    , useHtBackdrops(!QString(constApiKey).isEmpty())
    , fadeValue(0)
    , isWide(false)
    , stack(0)
    , viewCombo(0)
    , creator(0)
{
    animator.setPropertyName("fade");
    animator.setTargetObject(this);

    appLinkColor=QApplication::palette().color(QPalette::Link);
    artist = new ArtistView(this);
    album = new AlbumView(this);
    song = new SongView(this);
    minWidth=album->picSize().width()*1.9;

    artist->addEventFilter(this);
    album->addEventFilter(this);
    song->addEventFilter(this);

    connect(artist, SIGNAL(findArtist(QString)), this, SIGNAL(findArtist(QString)));
    connect(artist, SIGNAL(findAlbum(QString,QString)), this, SIGNAL(findAlbum(QString,QString)));
    connect(album, SIGNAL(playSong(QString)), this, SIGNAL(playSong(QString)));
    connect(artist, SIGNAL(haveBio(QString,QString)), album, SLOT(artistBio(QString,QString)), Qt::QueuedConnection);
    readConfig();
    setWide(true);
}

void ContextPage::setWide(bool w)
{
    if (w==isWide) {
        return;
    }

    isWide=w;
    if (w) {
        if (layout()) {
            delete layout();
        }
        QHBoxLayout *l=new QHBoxLayout(this);
        setLayout(l);
        int m=l->margin()/2;
        l->setMargin(0);
        if (stack) {
            stack->setVisible(false);
            viewCombo->setVisible(false);
            stack->removeWidget(artist);
            stack->removeWidget(album);
            stack->removeWidget(song);
            artist->setVisible(true);
            album->setVisible(true);
            song->setVisible(true);
        }
        artist->setParent(this);
        album->setParent(this);
        song->setParent(this);
        l->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
        l->addWidget(artist);
        l->addWidget(album);
        l->addWidget(song);
        //    layout->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
        l->setStretch(1, 1);
        l->setStretch(2, 1);
        l->setStretch(3, 1);
    } else {
        if (layout()) {
            delete layout();
        }
        QGridLayout *l=new QGridLayout(this);
        setLayout(l);
        int m=l->margin()/2;
        l->setMargin(0);
        l->setSpacing(0);
        if (!stack) {
            stack=new QStackedWidget(this);
        }
        if (!viewCombo) {
            viewCombo=new QComboBox(this);
            viewCombo->addItem(i18n("Artist Information"));
            viewCombo->addItem(i18n("Album Information"));
            viewCombo->addItem(i18n("Lyrics"));
            connect(viewCombo, SIGNAL(activated(int)), stack, SLOT(setCurrentIndex(int)));
        }
        stack->setVisible(true);
        viewCombo->setVisible(true);
        artist->setParent(stack);
        album->setParent(stack);
        song->setParent(stack);
        stack->addWidget(artist);
        stack->addWidget(album);
        stack->addWidget(song);
        l->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed), 0, 0, 1, 1);
        l->addWidget(stack, 0, 1, 1, 1);
        l->addWidget(viewCombo, 1, 0, 1, 2);
    }
}

void ContextPage::resizeEvent(QResizeEvent *e)
{
    if (isVisible()) {
        setWide(width()>minWidth);
    }
    QWidget::resizeEvent(e);
}

void ContextPage::readConfig()
{
    useBackdrop(Settings::self()->contextBackdrop());
    useDarkBackground(Settings::self()->contextDarkBackground());
    WikipediaEngine::setIntroOnly(Settings::self()->wikipediaIntroOnly());
    int zoom=Settings::self()->contextZoom();
    if (zoom) {
        artist->setZoom(zoom);
        album->setZoom(zoom);
        song->setZoom(zoom);
    }
}

void ContextPage::saveConfig()
{
    Settings::self()->saveContextZoom(artist->getZoom());
}

void ContextPage::useBackdrop(bool u)
{
    if (u!=drawBackdrop) {
        drawBackdrop=u;
        if (isVisible() && !currentArtist.isEmpty()) {
            updateArtist=currentArtist;
            currentArtist.clear();
            updateBackdrop();
            QWidget::update();
        }
    }
}

void ContextPage::useDarkBackground(bool u)
{
    if (u!=darkBackground) {
        darkBackground=u;
        QPalette pal=darkBackground ? palette() : parentWidget()->palette();
        QPalette appPal=QApplication::palette();
        QColor prevLinkColor;
        QColor linkCol;

        if (darkBackground) {
            QColor dark(32, 32, 32);
            QColor light(240, 240, 240);
            QColor linkVisited(164, 164, 164);
            pal.setColor(QPalette::Window, dark);
            pal.setColor(QPalette::Base, dark);
            pal.setColor(QPalette::WindowText, light);
            pal.setColor(QPalette::ButtonText, light);
            pal.setColor(QPalette::Text, light);
            pal.setColor(QPalette::Link, light);
            pal.setColor(QPalette::LinkVisited, linkVisited);
            prevLinkColor=appLinkColor;
            linkCol=pal.color(QPalette::Link);
        } else {
            linkCol=appLinkColor;
            prevLinkColor=QColor(240, 240, 240);
        }
        setPalette(pal);
        artist->setPal(pal, linkCol, prevLinkColor);
        album->setPal(pal, linkCol, prevLinkColor);
        song->setPal(pal, linkCol, prevLinkColor);
        QWidget::update();
    }
}

void ContextPage::showEvent(QShowEvent *e)
{
    setWide(width()>minWidth);
    if (drawBackdrop) {
        updateBackdrop();
    }
    QWidget::showEvent(e);
}

void ContextPage::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    if (darkBackground) {
        p.fillRect(rect(), palette().background().color());
    }
    if (drawBackdrop) {
        if (!oldBackdrop.isNull()) {
            p.setOpacity(0.15*((100.0-fadeValue)/100.0));
            p.fillRect(rect(), QBrush(oldBackdrop));
        }
        if (!newBackdrop.isNull()) {
            p.setOpacity(0.15*(fadeValue/100.0));
            p.fillRect(rect(), QBrush(newBackdrop));
        }
        if (!backdropText.isEmpty()) {
            int pad=fontMetrics().height()*2;
            QFont f("Sans", font().pointSize()*12);
            f.setBold(true);
            p.setFont(f);
            p.setOpacity(0.15);
            QTextOption textOpt(Qt::AlignBottom|(Qt::RightToLeft==layoutDirection() ? Qt::AlignRight : Qt::AlignLeft));
            textOpt.setWrapMode(QTextOption::NoWrap);
            p.drawText(QRect(pad, pad, width(), height()-(2*pad)), backdropText, textOpt);
        }
    }
    if (!darkBackground) {
        QWidget::paintEvent(e);
    }
}

void ContextPage::setFade(float value)
{
    if (fadeValue!=value) {
        fadeValue = value;
        if (fadeValue>99.9999999) {
            oldBackdrop=QImage();
        }
        QWidget::update();
    }
}

void ContextPage::updateImage(const QImage &img)
{
    DBUG << img.isNull() << newBackdrop.isNull();
    backdropText=currentArtist;
    oldBackdrop=newBackdrop;
    newBackdrop=img;
    animator.stop();
    if (newBackdrop.isNull() && oldBackdrop.isNull()) {
        return;
    }
    fadeValue=0.0;
    animator.setDuration(150);
    animator.setEndValue(100);
    animator.start();
}

void ContextPage::search()
{
    if (song->isVisible()) {
        song->search();
    }
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

    updateArtist=Covers::fixArtist(song.basicArtist());
    if (isVisible() && drawBackdrop) {
        updateBackdrop();
    }
}

bool ContextPage::eventFilter(QObject *o, QEvent *e)
{
    if (QEvent::Wheel==e->type()) {
        QWheelEvent *we=static_cast<QWheelEvent *>(e);
        if (Qt::ControlModifier==we->modifiers()) {
            int numDegrees = static_cast<QWheelEvent *>(e)->delta() / 8;
            int numSteps = numDegrees / 15;
            artist->setZoom(numSteps);
            album->setZoom(numSteps);
            song->setZoom(numSteps);
            return true;
        }
    }
    return QObject::eventFilter(o, e);
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
    DBUG << updateArtist << currentArtist;
    if (updateArtist==currentArtist) {
        return;
    }
    currentArtist=updateArtist;
    if (currentArtist.isEmpty()) {
        updateImage(QImage());
        QWidget::update();
        return;
    }

    QImage img(cacheFileName(currentArtist, false));
    updateImage(img);
    if (img.isNull()) {
        getBackdrop();
    } else {
        QWidget::update();
    }
}

void ContextPage::getBackdrop()
{
    cancel();
    if (!useHtBackdrops) {
        createBackdrop();
        return;
    }
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
    job=NetworkAccessManager::self()->get(url, 5000);
    connect(job, SIGNAL(finished()), this, SLOT(searchResponse()));
}

void ContextPage::searchResponse()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply) {
        return;
    }

    QString id;
    if (QNetworkReply::NoError==reply->error()) {
        QXmlStreamReader xml(reply);
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
    } else if (QNetworkReply::OperationCanceledError==reply->error()) {
        // We timed out, someting wrong with htbackdrops? Jsut use auto-generated backdrops for now...
        useHtBackdrops=false;
    }

    if (id.isEmpty()) {
        createBackdrop();
//        updateImage(QImage());
//        QWidget::update();
    } else {
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
    QImage img=QImage::fromData(data);
    if (img.isNull()) {
        createBackdrop();
        return;
    }
    updateImage(img);
//    if (!img.isNull()) {
        QFile f(cacheFileName(currentArtist, true));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        }
//    }
    QWidget::update();
}

void ContextPage::createBackdrop()
{
    DBUG << currentArtist;
    if (!creator) {
        creator = new BackdropCreator();
        connect(creator, SIGNAL(created(QString,QImage)), SLOT(backdropCreated(QString,QImage)));
        connect(this, SIGNAL(createBackdrop(QString,QList<Song>)), creator, SLOT(create(QString,QList<Song>)));
    }
    emit createBackdrop(currentArtist, MusicLibraryModel::self()->getArtistAlbums(currentArtist));
}

void ContextPage::backdropCreated(const QString &artist, const QImage &img)
{
    DBUG << artist << img.isNull() << currentArtist;
    if (artist==currentArtist) {
        updateImage(img);
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
