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
 
#include "contextwidget.h"
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
#include "qjson/parser.h"
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
#include <QDesktopWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QAction>
#include <QPair>
#include <QImage>
#include <qglobal.h>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void ContextWidget::enableDebug()
{
    debugEnabled=true;
}

//const QLatin1String ContextWidget::constHtbApiKey(0); // API key required
const QLatin1String ContextWidget::constFanArtApiKey("ee86404cb429fa27ac32a1a3c117b006");
const QLatin1String ContextWidget::constCacheDir("backdrops/");
static const double constBgndOpacity=0.15;

static QString cacheFileName(const QString &artist, bool createDir)
{
    return Utils::cacheDir(ContextWidget::constCacheDir, createDir)+Covers::encodeName(artist)+".jpg";
}

static QImage setOpacity(const QImage &orig)
{
    QImage img=QImage::Format_ARGB32==orig.format() ? orig : orig.convertToFormat(QImage::Format_ARGB32);
    uchar *bits = img.bits();
    for (int i = 0; i < img.height()*img.bytesPerLine(); i+=4) {
        bits[i+3] = constBgndOpacity*255;
    }
    return img;
}

static QColor splitterColor;

class ThinSplitterHandle : public QSplitterHandle
{
public:
    ThinSplitterHandle(Qt::Orientation orientation, ThinSplitter *parent)
        : QSplitterHandle(orientation, parent)
        , underMouse(false)
    {
        setMask(QRegion(contentsRect()));
        setAttribute(Qt::WA_MouseNoMask, true);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAttribute(Qt::WA_MouseTracking, true);
        QAction *act=new QAction(i18n("Reset Spacing"), this);
        addAction(act);
        connect(act, SIGNAL(triggered(bool)), parent, SLOT(reset()));
        setContextMenuPolicy(Qt::ActionsContextMenu);
    }

    void resizeEvent(QResizeEvent *event)
    {
        if (Qt::Horizontal==orientation()) {
            setContentsMargins(2, 0, 2, 0);
        } else {
            setContentsMargins(0, 2, 0, 2);
        }
        setMask(QRegion(contentsRect()));
        QSplitterHandle::resizeEvent(event);
    }

    void paintEvent(QPaintEvent *event)
    {
        if (underMouse) {
            QColor col(splitterColor);
            QPainter p(this);
            col.setAlphaF(0.75);
            p.fillRect(event->rect().adjusted(1, 0, -1, 0), col);
            col.setAlphaF(0.25);
            p.fillRect(event->rect(), col);
        }
    }

    bool event(QEvent *event)
    {
        switch(event->type()) {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            underMouse = true;
            update();
            break;
        case QEvent::ContextMenu:
        case QEvent::Leave:
        case QEvent::HoverLeave:
            underMouse = false;
            update();
            break;
        default:
            break;
        }

        return QWidget::event(event);
    }

    bool underMouse;
};


ThinSplitter::ThinSplitter(QWidget *parent)
    : QSplitter(parent)
{
    setHandleWidth(3);
    setChildrenCollapsible(false);
    setOrientation(Qt::Horizontal);
}

QSplitterHandle * ThinSplitter::createHandle()
{
    return new ThinSplitterHandle(orientation(), this);
}

void ThinSplitter::reset()
{
    int totalSize=0;
    foreach (int s, sizes()) {
        totalSize+=s;
    }
    QList<int> newSizes;
    int size=totalSize/count();
    for (int i=0; i<count()-1; ++i) {
        newSizes.append(size);
    }
    newSizes.append(totalSize-(size*newSizes.count()));
    setSizes(newSizes);
}

ContextWidget::ContextWidget(QWidget *parent)
    : QWidget(parent)
    , job(0)
    , drawBackdrop(true)
    , darkBackground(false)
//    , useHtBackdrops(0!=constHtbApiKey.latin1())
    , useFanArt(0!=constFanArtApiKey.latin1())
    , fadeValue(0)
    , isWide(false)
    , stack(0)
    , splitter(0)
    , viewCombo(0)
    , creator(0)
{
    animator.setPropertyName("fade");
    animator.setTargetObject(this);

    appLinkColor=QApplication::palette().color(QPalette::Link);
    artist = new ArtistView(this);
    album = new AlbumView(this);
    song = new SongView(this);
    minWidth=album->picSize().width()*2.5;

    artist->addEventFilter(this);
    album->addEventFilter(this);
    song->addEventFilter(this);

    connect(artist, SIGNAL(findArtist(QString)), this, SIGNAL(findArtist(QString)));
    connect(artist, SIGNAL(findAlbum(QString,QString)), this, SIGNAL(findAlbum(QString,QString)));
    connect(album, SIGNAL(playSong(QString)), this, SIGNAL(playSong(QString)));
    readConfig();
    setWide(true);
    splitterColor=palette().text().color();
    QDesktopWidget *dw=QApplication::desktop();
    if (dw) {
        minBackdropSize=dw->availableGeometry(this).size()-QSize(32, 64);
        minBackdropSize.setWidth(((int)(minBackdropSize.width()/32))*32);
        minBackdropSize.setHeight(((int)(minBackdropSize.height()/32))*32);
    } else {
        minBackdropSize=QSize(1024, 768);
    }
}

void ContextWidget::setWide(bool w)
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
        l->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
        QByteArray state;
        bool resetSplitter=splitter;
        if (!splitter) {
            splitter=new ThinSplitter(this);
            state=Settings::self()->contextSplitterState();
        }
        l->addWidget(splitter);
        artist->setParent(splitter);
        album->setParent(splitter);
        song->setParent(splitter);
        splitter->addWidget(artist);
        splitter->addWidget(album);
        splitter->setVisible(true);
        splitter->addWidget(song);
        if (resetSplitter) {
            splitter->reset();
        } else if (!state.isEmpty()) {
            splitter->restoreState(state);
        }
//        l->addWidget(album);
//        l->addWidget(song);
        //    layout->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
//        l->setStretch(1, 1);
//        l->setStretch(2, 1);
//        l->setStretch(3, 1);
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
            viewCombo->addItem(i18n("Artist Information"), "artist");
            viewCombo->addItem(i18n("Album Information"), "album");
            viewCombo->addItem(i18n("Lyrics"), "song");
            connect(viewCombo, SIGNAL(activated(int)), stack, SLOT(setCurrentIndex(int)));
        }
        if (splitter) {
            splitter->setVisible(false);
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
        QString lastSaved=Settings::self()->contextSlimPage();
        if (!lastSaved.isEmpty()) {
            for (int i=0; i<viewCombo->count(); ++i) {
                if (viewCombo->itemData(i).toString()==lastSaved) {
                    viewCombo->setCurrentIndex(i);
                    stack->setCurrentIndex(i);
                    break;
                }
            }
        }
    }
}

void ContextWidget::resizeEvent(QResizeEvent *e)
{
    if (isVisible()) {
        setWide(width()>minWidth);
    }
    QWidget::resizeEvent(e);
}

void ContextWidget::readConfig()
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

void ContextWidget::saveConfig()
{
    Settings::self()->saveContextZoom(artist->getZoom());
    if (viewCombo) {
        Settings::self()->saveContextSlimPage(viewCombo->itemData(viewCombo->currentIndex()).toString());
    }
    if (splitter) {
        Settings::self()->saveContextSplitterState(splitter->saveState());
    }
}

void ContextWidget::useBackdrop(bool u)
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

void ContextWidget::useDarkBackground(bool u)
{
    if (u!=darkBackground) {
        darkBackground=u;
        QPalette pal=darkBackground ? palette() : parentWidget()->palette();
        QColor prevLinkColor;
        QColor linkCol;

        if (darkBackground) {
            QColor dark(32, 32, 32);
            QColor light(240, 240, 240);
            QColor linkVisited(164, 164, 164);
            pal.setColor(QPalette::Window, dark);
            pal.setColor(QPalette::Base, dark);
            // Dont globally change window/button text - else this can mess up scrollbar buttons
            // with some styles (e.g. plastique)
//            pal.setColor(QPalette::WindowText, light);
//            pal.setColor(QPalette::ButtonText, light);
            pal.setColor(QPalette::Text, light);
            pal.setColor(QPalette::Link, light);
            pal.setColor(QPalette::LinkVisited, linkVisited);
            prevLinkColor=appLinkColor;
            linkCol=pal.color(QPalette::Link);
            splitterColor=light;
        } else {
            linkCol=appLinkColor;
            prevLinkColor=QColor(240, 240, 240);
            splitterColor=pal.text().color();
        }
        setPalette(pal);
        artist->setPal(pal, linkCol, prevLinkColor);
        album->setPal(pal, linkCol, prevLinkColor);
        song->setPal(pal, linkCol, prevLinkColor);
        QWidget::update();
    }
}

void ContextWidget::showEvent(QShowEvent *e)
{
    setWide(width()>minWidth);
    if (drawBackdrop) {
        updateBackdrop();
    }
    QWidget::showEvent(e);
}

void ContextWidget::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    QRect r(rect());

    if (!isWide && viewCombo) {
        int space=fontMetrics().height()/4;
        r.adjust(0, 0, 0, -(viewCombo->rect().height()+space));
    }
    if (darkBackground) {
        p.fillRect(r, palette().background().color());
    }
    if (drawBackdrop) {
        if (!oldBackdrop.isNull()) {
            if (!qFuzzyCompare(fadeValue, qreal(0.0))) {
                p.setOpacity(1.0-fadeValue);
            }
            p.fillRect(r, QBrush(oldBackdrop));
        }
        if (!currentBackdrop.isNull()) {
            p.setOpacity(fadeValue);
            p.fillRect(r, QBrush(currentBackdrop));
        }
//        if (!backdropText.isEmpty() && isWide) {
//            int pad=fontMetrics().height()*2;
//            QFont f("Sans", font().pointSize()*12);
//            f.setBold(true);
//            p.setFont(f);
//            p.setOpacity(0.15);
//            QTextOption textOpt(Qt::AlignBottom|(Qt::RightToLeft==layoutDirection() ? Qt::AlignRight : Qt::AlignLeft));
//            textOpt.setWrapMode(QTextOption::NoWrap);
//            p.drawText(QRect(pad, pad, width(), height()-(2*pad)), backdropText, textOpt);
//        }
    }
    if (!darkBackground) {
        QWidget::paintEvent(e);
    }
}

void ContextWidget::setFade(float value)
{
    if (fadeValue!=value) {
        fadeValue = value;
        if (qFuzzyCompare(fadeValue, qreal(1.0))) {
            oldBackdrop=QPixmap();
        }
        QWidget::update();
    }
}

void ContextWidget::updateImage(const QImage &img, bool created)
{
    DBUG << img.isNull() << currentBackdrop.isNull();
//    backdropText=currentArtist;
    oldBackdrop=currentBackdrop;
    currentBackdrop=QPixmap();
    animator.stop();
    if (img.isNull()) {
        backdropAlbums.clear();
    }
    if (img.isNull() && oldBackdrop.isNull()) {
        return;
    }
    if (!img.isNull()) {
        currentBackdrop=QPixmap::fromImage(setOpacity(img));
    }

    if (!currentBackdrop.isNull() && !created && currentBackdrop.width()<minBackdropSize.width() && currentBackdrop.height()<minBackdropSize.height()) {
        QSize size(minBackdropSize);
        if (currentBackdrop.width()<minBackdropSize.width()/4 && currentBackdrop.height()<minBackdropSize.height()/4) {
            size=QSize(minBackdropSize.width()/2, minBackdropSize.height()/2);
        }
        currentBackdrop=currentBackdrop.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    fadeValue=0.0;
    animator.setDuration(250);
    animator.setEndValue(1.0);
    animator.start();
}

void ContextWidget::search()
{
    if (song->isVisible()) {
        song->search();
    }
}

void ContextWidget::update(const Song &s)
{
    Song sng=s;
    if (sng.isVariousArtists()) {
        sng.revertVariousArtists();
    }
    artist->update(sng);
    album->update(sng);
    song->update(sng);

    updateArtist=Covers::fixArtist(sng.basicArtist());
    if (isVisible() && drawBackdrop) {
        updateBackdrop();
    }
}

bool ContextWidget::eventFilter(QObject *o, QEvent *e)
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

void ContextWidget::cancel()
{
    if (job) {
        job->deleteLater();
        job=0;
    }
}

void ContextWidget::updateBackdrop()
{
    DBUG << updateArtist << currentArtist;
    if (updateArtist==currentArtist) {
        return;
    }
    currentArtist=updateArtist;
    backdropAlbums.clear();
    if (currentArtist.isEmpty()) {
        updateImage(QImage());
        QWidget::update();
        return;
    }

    QImage img(cacheFileName(currentArtist, false));
    if (img.isNull()) {
        getBackdrop();
    } else {
        updateImage(img);
        QWidget::update();
    }
}

static QString fixArtist(const QString &artist)
{
    QString fixed(artist.trimmed());
    fixed.remove(QChar('?'));
    return fixed;
}

void ContextWidget::getBackdrop()
{
    cancel();
    if (artistsCreatedBackdropsFor.contains(currentArtist)) {
        createBackdrop();
    } else /*if (useHtBackdrops) {
        getHtBackdrop();
    } else*/ if (useFanArt) {
        getFanArtBackdrop();
    } else {
        getDiscoGsImage();
    }
}

//void ContextWidget::getHtBackdrop()
//{
//    QUrl url("http://htbackdrops.com/api/"+constHtbApiKey+"/searchXML");
//    #if QT_VERSION < 0x050000
//    QUrl &q=url;
//    #else
//    QUrlQuery q;
//    #endif

//    q.addQueryItem(QLatin1String("keywords"), fixArtist(currentArtist));
//    q.addQueryItem(QLatin1String("default_operator"), QLatin1String("and"));
//    q.addQueryItem(QLatin1String("fields"), QLatin1String("title"));
//    #if QT_VERSION >= 0x050000
//    url.setQuery(q);
//    #endif
//    job=NetworkAccessManager::self()->get(url, 5000);
//    DBUG << url.toString();
//    connect(job, SIGNAL(finished()), this, SLOT(htBackdropsResponse()));
//}

void ContextWidget::getFanArtBackdrop()
{
    // First we need to query musicbrainz to get id
    QUrl url("http://www.musicbrainz.org/ws/2/artist/");
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif

    query.addQueryItem("query", "artist:"+fixArtist(currentArtist));
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    job = NetworkAccessManager::self()->get(url);
    DBUG << url.toString();
    connect(job, SIGNAL(finished()), this, SLOT(musicbrainzResponse()));
}

void ContextWidget::getDiscoGsImage()
{
    cancel();
    QUrl url;
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost("api.discogs.com");
    url.setPath("/search");
    query.addQueryItem("per_page", QString::number(5));
    query.addQueryItem("type", "artist");
    query.addQueryItem("q", fixArtist(currentArtist));
    query.addQueryItem("f", "json");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    job=NetworkAccessManager::self()->get(url, 5000);
    DBUG << url.toString();
    connect(job, SIGNAL(finished()), this, SLOT(discoGsResponse()));
}

//void ContextWidget::htBackdropsResponse()
//{
//    QNetworkReply *reply = getReply(sender());
//    if (!reply) {
//        return;
//    }

//    DBUG << "status" << reply->error() << reply->errorString();

//    QString id;
//    if (QNetworkReply::NoError==reply->error()) {
//        QXmlStreamReader xml(reply);
//        while (!xml.atEnd() && !xml.hasError() && id.isEmpty()) {
//            xml.readNext();
//            if (xml.isStartElement() && QLatin1String("search")==xml.name()) {
//                while (xml.readNextStartElement() && id.isEmpty()) {
//                    if (xml.isStartElement() && QLatin1String("images")==xml.name()) {
//                        while (xml.readNextStartElement() && id.isEmpty()) {
//                            if (xml.isStartElement() && QLatin1String("image")==xml.name()) {
//                                while (xml.readNextStartElement() && id.isEmpty()) {
//                                    if (xml.isStartElement() && QLatin1String("id")==xml.name()) {
//                                        id=xml.readElementText();
//                                    } else {
//                                        xml.skipCurrentElement();
//                                    }
//                                }
//                            } else {
//                                xml.skipCurrentElement();
//                            }
//                        }
//                    } else {
//                        xml.skipCurrentElement();
//                    }
//                }
//            }
//        }
//    } else if (QNetworkReply::OperationCanceledError==reply->error()) {
//        // We timed out, someting wrong with htbackdrops? Jsut use auto-generated backdrops for now...
//        useHtBackdrops=false;
//    }

//    if (id.isEmpty()) {
//        getDiscoGsImage();
//    } else {
//        QUrl url("http://htbackdrops.com/api/"+constHtbApiKey+"/download/"+id+"/fullsize");
//        job=NetworkAccessManager::self()->get(url);
//        DBUG << url.toString();
//        connect(job, SIGNAL(finished()), this, SLOT(downloadResponse()));
//    }
//}

void ContextWidget::musicbrainzResponse()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();

    bool inSection=false;
    QXmlStreamReader doc(reply);
    QString id;

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            if (!inSection && QLatin1String("artist-list")==doc.name()) {
                inSection=true;
            } if (inSection && QLatin1String("artist")==doc.name()) {
                id=doc.attributes().value("id").toString();
                break;
            }
        } else if (doc.isEndElement() && inSection && QLatin1String("artist")==doc.name()) {
            break;
        }
    }

    if (id.isEmpty()) {
        getDiscoGsImage();
    } else {
        QUrl url("http://api.fanart.tv/webservice/artist/"+constFanArtApiKey+"/"+id+"/json/artistbackground/1");
        job=NetworkAccessManager::self()->get(url);
        DBUG << url.toString();
        connect(job, SIGNAL(finished()), this, SLOT(fanArtResponse()));
    }
}

void ContextWidget::fanArtResponse()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();
    QString url;

    if (QNetworkReply::NoError==reply->error()) {
        QJson::Parser parser;
        bool ok=false;
        QVariantMap parsed=parser.parse(reply, &ok).toMap();
        if (ok && !parsed.isEmpty()) {
            QVariantMap artist=parsed[parsed.keys().first()].toMap();
            if (artist.contains("artistbackground")) {
                QVariantList artistbackgrounds=artist["artistbackground"].toList();
                if (!artistbackgrounds.isEmpty()) {
                    QVariantMap artistbackground=artistbackgrounds.first().toMap();
                    if (artistbackground.contains("url")) {
                        url=artistbackground["url"].toString();
                    }
                }
            }
        }
    }

    if (url.isEmpty()) {
        getDiscoGsImage();
    } else {
        job=NetworkAccessManager::self()->get(QUrl(url));
        DBUG << url;
        connect(job, SIGNAL(finished()), this, SLOT(downloadResponse()));
    }
}

static bool matchesArtist(const QString &titleOrig, const QString &artistOrig)
{
    QString title=titleOrig.toLower();
    QString artist=artistOrig.toLower();

    if (title==artist) {
        return true;
    }

    if (artist.startsWith(QLatin1String("the ")) && title.endsWith(QLatin1String(", the"))) {
        QString theArtist=artist.mid(4)+QLatin1String(", the");
        if (title==theArtist) {
            return true;
        }
    }

    typedef QPair<QChar, QChar> ChPair;
    QList<ChPair> replacements = QList<ChPair>() << ChPair('-', '/') << ChPair('.', 0)
                                                 << ChPair(QChar(0x00ff /* ÿ */), 'y');

    foreach (const ChPair &r, replacements) {
        QString a=artist;
        QString t=title;

        if (r.second.isNull()) {
            a=a.replace(QString()+r.first, "");
            t=t.replace(QString()+r.first, "");
        } else {
            a=a.replace(r.first, r.second);
            t=t.replace(r.first, r.second);
        }
        if (t==a) {
            return true;
        }
    }

    return false;
}

void ContextWidget::discoGsResponse()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();
    QString url;

    if (QNetworkReply::NoError==reply->error()) {
        QJson::Parser parser;
        bool ok=false;
        QVariantMap parsed=parser.parse(reply, &ok).toMap();
        if (ok && parsed.contains("resp")) {
            QVariantMap response=parsed["resp"].toMap();
            if (response.contains("search")) {
                QVariantMap search=response["search"].toMap();
                if (search.contains("exactresults")) {
                    QVariantList results=search["exactresults"].toList();
                    foreach (const QVariant &r, results) {
                        QVariantMap rm=r.toMap();
                        if (rm.contains("thumb") && rm.contains("title")) {
                            QString thumbUrl=rm["thumb"].toString();
                            QString title=rm["title"].toString();
                            if (thumbUrl.contains("/image/A-150-") && matchesArtist(title, currentArtist)) {
                                url=thumbUrl.replace("image/A-150-", "/image/A-");
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (url.isEmpty()) {
        createBackdrop();
    } else {
        job=NetworkAccessManager::self()->get(QUrl(url));
        DBUG << url;
        connect(job, SIGNAL(finished()), this, SLOT(downloadResponse()));
    }
}

void ContextWidget::downloadResponse()
{
    QNetworkReply *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();

    if (QNetworkReply::NoError!=reply->error()) {
        return;
    }

    QByteArray data=reply->readAll();
    QImage img=QImage::fromData(data);

    if (img.isNull()) {
        createBackdrop();
    } else {
        updateImage(img);
        QFile f(cacheFileName(currentArtist, true));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        }
        QWidget::update();
    }
}

void ContextWidget::createBackdrop()
{
    DBUG << currentArtist;
    if (!creator) {
        creator = new BackdropCreator();
        connect(creator, SIGNAL(created(QString,QImage)), SLOT(backdropCreated(QString,QImage)));
        connect(this, SIGNAL(createBackdrop(QString,QList<Song>)), creator, SLOT(create(QString,QList<Song>)));
    }
    QList<Song> artistAlbumsFirstTracks=artist->getArtistAlbumsFirstTracks();
    QSet<QString> albumNames;

    foreach (const Song &s, artistAlbumsFirstTracks) {
        albumNames.insert(s.albumArtist()+" - "+s.album);
    }

    if (backdropAlbums!=albumNames) {
        backdropAlbums=albumNames;
        emit createBackdrop(currentArtist, artistAlbumsFirstTracks);
    }
}

void ContextWidget::backdropCreated(const QString &artist, const QImage &img)
{
    DBUG << artist << img.isNull() << currentArtist;
    if (artist==currentArtist) {
        artistsCreatedBackdropsFor.removeAll(artist);
        artistsCreatedBackdropsFor.append(artist);
        if (artistsCreatedBackdropsFor.count()>20) {
            artistsCreatedBackdropsFor.removeFirst();
        }
        updateImage(img, true);
        QWidget::update();
    }
}

QNetworkReply * ContextWidget::getReply(QObject *obj)
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
