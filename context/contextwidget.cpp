/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef ENABLE_ONLINE_SERVICES
#include "onlineview.h"
#endif
#include "mpd-interface/song.h"
#include "support/utils.h"
#include "gui/covers.h"
#include "network/networkaccessmanager.h"
#include "gui/settings.h"
#include "wikipediaengine.h"
#include "support/localize.h"
#include "backdropcreator.h"
#include "support/gtkstyle.h"
#include "widgets/playqueueview.h"
#include "widgets/treeview.h"
#include "widgets/thinsplitterhandle.h"
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QStylePainter>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#include <QJsonDocument>
#else
#include "qjson/parser.h"
#endif
#include <QXmlStreamReader>
#include <QFile>
#include <QWheelEvent>
#include <QApplication>
#ifndef SCALE_CONTEXT_BGND
#include <QDesktopWidget>
#endif
#include <QStackedWidget>
#include <QAction>
#include <QPair>
#include <QImage>
#include <QToolButton>
#include <QButtonGroup>
#include <QWheelEvent>
#include <qglobal.h>

// Exported by QtGui
void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void ContextWidget::enableDebug()
{
    debugEnabled=true;
}

const QLatin1String ContextWidget::constBackdropFileName("backdrop");
//const QLatin1String ContextWidget::constHtbApiKey(0); // API key required
const QLatin1String ContextWidget::constFanArtApiKey("ee86404cb429fa27ac32a1a3c117b006");
const QLatin1String ContextWidget::constCacheDir("backdrops/");

static QString cacheFileName(const QString &artist, bool createDir)
{
    return Utils::cacheDir(ContextWidget::constCacheDir, createDir)+Covers::encodeName(artist)+".jpg";
}

class ViewSelectorButton : public QToolButton
{
public:
    ViewSelectorButton(QWidget *p) : QToolButton(p) { }
    void paintEvent(QPaintEvent *ev)
    {
        Q_UNUSED(ev)
        QPainter painter(this);
        QString txt=text();

        txt.replace("&", "");
        QFont f(font());
        if (isChecked()) {
            f.setBold(true);
        }
        QFontMetrics fm(f);
        if (fm.width(txt)>rect().width()) {
            txt=fm.elidedText(txt, isRightToLeft() ? Qt::ElideLeft : Qt::ElideRight, rect().width());
        }

        painter.setFont(f);
        painter.setPen(palette().color(!isChecked() && underMouse() ? QPalette::Highlight : QPalette::Text));
        painter.drawText(rect(), Qt::AlignCenter, txt);
    }
};

static const char *constDataProp="view-data";

ViewSelector::ViewSelector(QWidget *p)
    : QWidget(p)
{
    group=new QButtonGroup(this);
}

void ViewSelector::addItem(const QString &label, const QVariant &data)
{
    QHBoxLayout *l;
    if (buttons.isEmpty()) {
        l = new QHBoxLayout(this);
        l->setMargin(0);
        l->setSpacing(0);
    } else {
        l=static_cast<QHBoxLayout *>(layout());
    }
    QToolButton *button=new ViewSelectorButton(this);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    button->setAutoRaise(true);
    button->setText(label);
    button->setCheckable(true);
    button->setProperty(constDataProp, data);
    connect(button, SIGNAL(toggled(bool)), this, SLOT(buttonActivated()));
    buttons.append(button);
    group->addButton(button);
    l->addWidget(button);
}

void ViewSelector::buttonActivated()
{
    QToolButton *button=qobject_cast<QToolButton *>(sender());
    if (button && button->isChecked()) {
        emit activated(buttons.indexOf(button));
    }
}

QVariant ViewSelector::itemData(int index) const
{
    return index>=0 && index<buttons.count() ? buttons.at(index)->property(constDataProp) : QVariant();
}

int ViewSelector::currentIndex() const
{
    for (int i=0; i<buttons.count(); ++i) {
        if (buttons.at(i)->isChecked()) {
            return i;
        }
    }
    return -1;
}

void ViewSelector::setCurrentIndex(int index)
{
    QFont f(font());
    for (int i=0; i<buttons.count(); ++i) {
        QToolButton *btn=buttons.at(i);
        bool wasChecked=btn->isChecked();
        btn->setChecked(i==index);
        if (i==index) {
            emit activated(i);
        } else if (wasChecked) {
            btn->setFont(f);
        }
    }
}

void ViewSelector::wheelEvent(QWheelEvent *ev)
{
    int numDegrees = ev->delta() / 8;
    int numSteps = numDegrees / 15;
    if (numSteps > 0) {
        for (int i = 0; i < numSteps; ++i) {
            int index=currentIndex();
            setCurrentIndex(index==count()-1 ? 0 : index+1);
        }
    } else {
        for (int i = 0; i > numSteps; --i) {
            int index=currentIndex();
            setCurrentIndex(index==0 ? count()-1 : index-1);
        }
    }
}

void ViewSelector::paintEvent(QPaintEvent *ev)
{
    Q_UNUSED(ev)
}

ThinSplitter::ThinSplitter(QWidget *parent)
    : QSplitter(parent)
{
    setChildrenCollapsible(true);
    setOrientation(Qt::Horizontal);
    resetAct=new QAction(i18n("Reset Spacing"), this);
    connect(resetAct, SIGNAL(triggered()), this, SLOT(reset()));
    setHandleWidth(0);
}

QSplitterHandle * ThinSplitter::createHandle()
{
    ThinSplitterHandle *handle=new ThinSplitterHandle(orientation(), this);
    handle->addAction(resetAct);
    handle->setContextMenuPolicy(Qt::ActionsContextMenu);
    handle->setHighlightUnderMouse();
    return handle;
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
    , alwaysCollapsed(false)
    , backdropType(PlayQueueView::BI_Cover)
    , darkBackground(false)
    , useFanArt(0!=constFanArtApiKey.latin1())
    , albumCoverBackdrop(false)
    , oldIsAlbumCoverBackdrop(false)
    , fadeValue(1.0)
    , isWide(false)
    , stack(0)
    #ifdef ENABLE_ONLINE_SERVICES
    , onlineContext(0)
    #endif
    , splitter(0)
    , viewSelector(0)
    , creator(0)
{
    QHBoxLayout *layout=new QHBoxLayout(this);
    mainStack=new QStackedWidget(this);
    standardContext=new QWidget(mainStack);
    mainStack->addWidget(standardContext);
    layout->setMargin(0);
    layout->addWidget(mainStack);
    animator.setPropertyName("fade");
    animator.setTargetObject(this);

    appLinkColor=QApplication::palette().color(QPalette::Link);
    artist = new ArtistView(standardContext);
    album = new AlbumView(standardContext);
    song = new SongView(standardContext);
    minWidth=album->picSize().width()*2.5;

    artist->addEventFilter(this);
    album->addEventFilter(this);
    song->addEventFilter(this);

    connect(artist, SIGNAL(findArtist(QString)), this, SIGNAL(findArtist(QString)));
    connect(artist, SIGNAL(findAlbum(QString,QString)), this, SIGNAL(findAlbum(QString,QString)));
    connect(album, SIGNAL(playSong(QString)), this, SIGNAL(playSong(QString)));
    readConfig();
    setZoom();
    setWide(true);

    #ifndef SCALE_CONTEXT_BGND
    QDesktopWidget *dw=QApplication::desktop();
    if (dw) {
        QSize geo=dw->availableGeometry(this).size()-QSize(32, 64);
        minBackdropSize=geo;
        minBackdropSize.setWidth(((int)(minBackdropSize.width()/32))*32);
        minBackdropSize.setHeight(((int)(minBackdropSize.height()/32))*32);
        maxBackdropSize=QSize(geo.width()*1.25, geo.height()*1.25);
    } else {
        minBackdropSize=QSize(Utils::scaleForDpi(1024*3), Utils::scaleForDpi(768*3);
        maxBackdropSize=QSize(minBackdropSize.width()*2, minBackdropSize.height()*2);
    }
    #endif
}

void ContextWidget::setZoom()
{
    int zoom=Settings::self()->contextZoom();
    if (zoom) {
        artist->setZoom(zoom);
        album->setZoom(zoom);
        song->setZoom(zoom);
    }
}

void ContextWidget::setWide(bool w)
{
    if (w==isWide) {
        return;
    }

    isWide=w;
    if (w) {
        if (standardContext->layout()) {
            delete standardContext->layout();
        }
        QHBoxLayout *l=new QHBoxLayout(standardContext);
        standardContext->setLayout(l);
        int m=l->margin()/2;
        l->setMargin(0);
        if (stack) {
            stack->setVisible(false);
            viewSelector->setVisible(false);
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
            splitter=new ThinSplitter(standardContext);
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
    } else {
        if (standardContext->layout()) {
            delete standardContext->layout();
        }
        QGridLayout *l=new QGridLayout(standardContext);
        standardContext->setLayout(l);
        int m=l->margin()/2;
        l->setMargin(0);
        l->setSpacing(0);
        if (!stack) {
            stack=new QStackedWidget(standardContext);
        }
        if (!viewSelector) {
            viewSelector=new ViewSelector(standardContext);
            viewSelector->addItem(i18n("&Artist"), "artist");
            viewSelector->addItem(i18n("Al&bum"), "album");
            viewSelector->addItem(i18n("&Track"), "song");
            viewSelector->setPalette(palette());
            connect(viewSelector, SIGNAL(activated(int)), stack, SLOT(setCurrentIndex(int)));
        }
        if (splitter) {
            splitter->setVisible(false);
        }
        stack->setVisible(true);
        viewSelector->setVisible(true);
        artist->setParent(stack);
        album->setParent(stack);
        song->setParent(stack);
        stack->addWidget(artist);
        stack->addWidget(album);
        stack->addWidget(song);
        l->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed), 0, 0, 1, 1);
        l->addWidget(stack, 0, 1, 1, 1);
        l->addWidget(viewSelector, 1, 0, 1, 2);
        QString lastSaved=Settings::self()->contextSlimPage();
        if (!lastSaved.isEmpty()) {
            for (int i=0; i<viewSelector->count(); ++i) {
                if (viewSelector->itemData(i).toString()==lastSaved) {
                    viewSelector->setCurrentIndex(i);
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
        setWide(width()>minWidth && !alwaysCollapsed);
    }
    #ifdef SCALE_CONTEXT_BGND
    resizeBackdrop();
    #endif
    QWidget::resizeEvent(e);
}

void ContextWidget::readConfig()
{
    int origOpacity=backdropOpacity;
    int origBlur=backdropBlur;
    QString origCustomBackdropFile=customBackdropFile;
    int origType=backdropType;
    backdropType=Settings::self()->contextBackdrop();
    backdropOpacity=Settings::self()->contextBackdropOpacity();
    backdropBlur=Settings::self()->contextBackdropBlur();
    customBackdropFile=Settings::self()->contextBackdropFile();
    switch (backdropType) {
    case PlayQueueView::BI_None:
        if (origType!=backdropType && isVisible() && !currentArtist.isEmpty()) {
            updateBackdrop(true);
            QWidget::update();
        }
        break;
    case PlayQueueView::BI_Cover:
        if (origType!=backdropType || backdropOpacity!=origOpacity || backdropBlur!=origBlur) {
            if (isVisible() && !currentArtist.isEmpty()) {
                updateBackdrop(true);
                QWidget::update();
            }
        }
        break;
   case PlayQueueView::BI_Custom:
        if (origType!=backdropType || backdropOpacity!=origOpacity || backdropBlur!=origBlur || origCustomBackdropFile!=customBackdropFile) {            
            updateImage(QImage(customBackdropFile), false);
            artistsCreatedBackdropsFor.clear();
        }
        break;
    }

    useDarkBackground(Settings::self()->contextDarkBackground());
    WikipediaEngine::setIntroOnly(Settings::self()->wikipediaIntroOnly());
    bool wasCollpased=stack && stack->isVisible();
    alwaysCollapsed=Settings::self()->contextAlwaysCollapsed();
    if (alwaysCollapsed && !wasCollpased) {
        setWide(false);
    }
}

void ContextWidget::saveConfig()
{
    Settings::self()->saveContextZoom(artist->getZoom());
    if (viewSelector) {
        Settings::self()->saveContextSlimPage(viewSelector->itemData(viewSelector->currentIndex()).toString());
    }
    if (splitter) {
        Settings::self()->saveContextSplitterState(splitter->saveState());
    }
    song->saveConfig();
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
            // Dont globally change window/button text - because this can mess up scrollbar buttons
            // with some styles (e.g. plastique)
//            pal.setColor(QPalette::WindowText, light);
//            pal.setColor(QPalette::ButtonText, light);
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
        if (viewSelector) {
            viewSelector->setPalette(pal);
        }
        QWidget::update();
    }
}

void ContextWidget::showEvent(QShowEvent *e)
{
    setWide(width()>minWidth && !alwaysCollapsed);
    if (backdropType) {
        updateBackdrop();
    }
    QWidget::showEvent(e);
}

void ContextWidget::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    QRect r(rect());

    if (darkBackground) {
        p.fillRect(r, palette().background().color());
    }
    if (backdropType) {
        if (!oldBackdrop.isNull()) {
            if (!qFuzzyCompare(fadeValue, qreal(0.0))) {
                p.setOpacity(1.0-fadeValue);
            }
            #ifdef SCALE_CONTEXT_BGND
            if (!oldIsAlbumCoverBackdrop && oldBackdrop.height()<height()) {
                p.drawPixmap(0, (height()-oldBackdrop.height())/2, oldBackdrop);
            } else
            #endif
            p.fillRect(r, QBrush(oldBackdrop));
        }
        if (!currentBackdrop.isNull()) {
            p.setOpacity(fadeValue);
            #ifdef SCALE_CONTEXT_BGND
            if (!albumCoverBackdrop && currentBackdrop.height()<height()) {
                p.drawPixmap(0, (height()-currentBackdrop.height())/2, currentBackdrop);
            } else
            #endif
            p.fillRect(r, QBrush(currentBackdrop));
        }
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
            oldIsAlbumCoverBackdrop=false;
        }
        QWidget::update();
    }
}

void ContextWidget::updateImage(QImage img, bool created)
{
    DBUG << img.isNull() << currentBackdrop.isNull();
    oldBackdrop=currentBackdrop;
    oldIsAlbumCoverBackdrop=albumCoverBackdrop;
    currentBackdrop=QPixmap();
    animator.stop();
    if (img.isNull()) {
        backdropAlbums.clear();
    }
    if (img.isNull() && oldBackdrop.isNull()) {
        return;
    }
    if (!img.isNull()) {
        if (backdropOpacity<100) {
            img=TreeView::setOpacity(img, (backdropOpacity*1.0)/100.0);
        }
        if (backdropBlur>0) {
            QImage blurred(img.size(), QImage::Format_ARGB32_Premultiplied);
            blurred.fill(Qt::transparent);
            QPainter painter(&blurred);
            qt_blurImage(&painter, img, backdropBlur, true, false);
            painter.end();
            img = blurred;
        }
        #ifdef SCALE_CONTEXT_BGND
        currentImage=img;
        #else
        currentBackdrop=QPixmap::fromImage(img);
        #endif
    }
    albumCoverBackdrop=created;
    resizeBackdrop();

    animator.stop();
    if (PlayQueueView::BI_Custom==backdropType || !isVisible()) {
        setFade(1.0);
    } else {
        fadeValue=0.0;
        animator.setDuration(250);
        animator.setEndValue(1.0);
        animator.start();
    }
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

    if (sng.isStandardStream() && sng.artist.isEmpty() && sng.albumartist.isEmpty() && sng.album.isEmpty()) {
        int pos=sng.title.indexOf(QLatin1String(" - "));
        if (pos>3) {
            sng.artist=sng.title.left(pos);
            sng.title=sng.title.mid(pos+3);
        }
    }

    if (s.albumArtist()!=currentSong.albumArtist()) {
        cancel();
    }

    #ifdef ENABLE_ONLINE_SERVICES
    if (Song::OnlineSvrTrack==sng.type) {
        if (!onlineContext) {
            QWidget *onlinePage=new QWidget(mainStack);
            QHBoxLayout *onlineLayout=new QHBoxLayout(onlinePage);
            int m=onlineLayout->margin()/2;
            onlineLayout->setMargin(0);
            onlineLayout->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
            onlineContext=new OnlineView(onlinePage);
            onlineLayout->addWidget(onlineContext);
            mainStack->addWidget(onlinePage);
        }
        onlineContext->update(sng);
        mainStack->setCurrentIndex(1);
        updateArtist=QString();
        if (isVisible() && PlayQueueView::BI_Cover==backdropType) {
            updateBackdrop();
        }
        return;
    }
    mainStack->setCurrentIndex(0);
    #endif
    artist->update(sng);
    album->update(sng);
    song->update(sng);
    currentSong=s;

    updateArtist=Covers::fixArtist(sng.basicArtist());
    if (isVisible() && PlayQueueView::BI_Cover==backdropType) {
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
        job->cancelAndDelete();
        job=0;
    }
}

void ContextWidget::updateBackdrop(bool force)
{
    DBUG << updateArtist << currentArtist << currentSong.file << force;
    if (!force && updateArtist==currentArtist) {
        return;
    }
    currentArtist=updateArtist;
    backdropAlbums.clear();
    if (currentArtist.isEmpty()) {
        updateImage(QImage());
        QWidget::update();
        return;
    }

    QString encoded=Covers::encodeName(currentArtist);
    QStringList names=QStringList() << encoded+"-"+constBackdropFileName+".jpg" << encoded+"-"+constBackdropFileName+".png"
                                    << constBackdropFileName+".jpg" << constBackdropFileName+".png";

    if (!currentSong.isStream()) {
        bool localNonMpd=currentSong.file.startsWith(Utils::constDirSep);
        QString dirName=localNonMpd ? QString() : MPDConnection::self()->getDetails().dir;
        if (localNonMpd || (!dirName.isEmpty() && !dirName.startsWith(QLatin1String("http:/")) && MPDConnection::self()->getDetails().dirReadable)) {
            dirName+=Utils::getDir(currentSong.file);

            for (int level=0; level<2; ++level) {
                foreach (const QString &fileName, names) {
                    DBUG << "Checking file(1)" << QString(dirName+fileName);
                    if (QFile::exists(dirName+fileName)) {
                        QImage img(dirName+fileName);

                        if (!img.isNull()) {
                            DBUG << "Got backdrop from" << QString(dirName+fileName);
                            updateImage(img);
                            QWidget::update();
                            return;
                        }
                    }
                }
                QDir d(dirName);
                d.cdUp();
                dirName=Utils::fixPath(d.absolutePath());
            }
        }
    }

    // For various artists tracks, or for non-MPD files, see if we have a matching backdrop in MPD.
    // e.g. artist=Wibble, look for $mpdDir/Wibble/backdrop.png
    if (currentSong.isVariousArtists() || currentSong.isNonMPD()) {
        QString dirName=MPDConnection::self()->getDetails().dirReadable ? MPDConnection::self()->getDetails().dir : QString();
        if (!dirName.isEmpty() && !dirName.startsWith(QLatin1String("http:/"))) {
            dirName+=currentArtist+Utils::constDirSep;
            foreach (const QString &fileName, names) {
                DBUG << "Checking file(2)" << QString(dirName+fileName);
                if (QFile::exists(dirName+fileName)) {
                    QImage img(dirName+fileName);

                    if (!img.isNull()) {
                        DBUG << "Got backdrop from" << QString(dirName+fileName);
                        updateImage(img);
                        QWidget::update();
                        return;
                    }
                }
            }
        }
    }

    QString cacheName=cacheFileName(currentArtist, false);
    QImage img(cacheName);
    if (img.isNull()) {
        getBackdrop();
    } else {
        DBUG << "Use cache file:" << cacheName;
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
    } else if (useFanArt) {
        getFanArtBackdrop();
    } else {
        getDiscoGsImage();
    }
}

void ContextWidget::getFanArtBackdrop()
{
    // First we need to query musicbrainz to get id
    getMusicbrainzId(fixArtist(currentArtist));
}

static const char * constArtistProp="artist-name";
void ContextWidget::getMusicbrainzId(const QString &artist)
{
    QUrl url("http://www.musicbrainz.org/ws/2/artist/");
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif

    query.addQueryItem("query", "artist:"+artist);
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    job = NetworkAccessManager::self()->get(url);
    DBUG << url.toString();
    job->setProperty(constArtistProp, artist);
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

void ContextWidget::musicbrainzResponse()
{
    NetworkJob *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();

    QString id;

    if (reply->ok()) {
        bool inSection=false;
        QXmlStreamReader doc(reply->actualJob());

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
    }

    if (id.isEmpty()) {
        QString artist=reply->property(constArtistProp).toString();
        // MusicBrainz does not seem to like AC/DC, but AC DC works - so if we fail with an artist
        // containing /, then try with space...
        if (!artist.isEmpty() && artist.contains("/")) {
            artist=artist.replace("/", " ");
            getMusicbrainzId(artist);
        } else {
            getDiscoGsImage();
        }
    } else {
        QUrl url("http://api.fanart.tv/webservice/artist/"+constFanArtApiKey+"/"+id+"/json/artistbackground/1");
        job=NetworkAccessManager::self()->get(url);
        DBUG << url.toString();
        connect(job, SIGNAL(finished()), this, SLOT(fanArtResponse()));
    }
}

void ContextWidget::fanArtResponse()
{
    NetworkJob *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();
    QString url;

    if (reply->ok()) {
        #if QT_VERSION >= 0x050000
        QJsonParseError jsonParseError;
        QVariantMap parsed=QJsonDocument::fromJson(reply->readAll(), &jsonParseError).toVariant().toMap();
        bool ok=QJsonParseError::NoError==jsonParseError.error;
        #else
        bool ok=false;
        QVariantMap parsed = QJson::Parser().parse(reply->readAll(), &ok).toMap();
        #endif

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
    NetworkJob *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();
    QString url;

    if (reply->ok()) {
        #if QT_VERSION >= 0x050000
        QJsonParseError jsonParseError;
        QVariantMap parsed=QJsonDocument::fromJson(reply->readAll(), &jsonParseError).toVariant().toMap();
        bool ok=QJsonParseError::NoError==jsonParseError.error;
        #else
        bool ok=false;
        QVariantMap parsed = QJson::Parser().parse(reply->readAll(), &ok).toMap();
        #endif

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
    NetworkJob *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();

    QImage img;
    QByteArray data;

    if (reply->ok()) {
        data=reply->readAll();
        img=QImage::fromData(data);
    }

    if (img.isNull()) {
        createBackdrop();
    } else {
        updateImage(img);
        bool saved=false;

        if (Settings::self()->storeBackdropsInMpdDir() && !currentSong.isVariousArtists() &&
            !currentSong.isNonMPD() && MPDConnection::self()->getDetails().dirReadable) {
            QString mpdDir=MPDConnection::self()->getDetails().dir;
            QString songDir=Utils::getDir(currentSong.file);
            if (!mpdDir.isEmpty() && 2==songDir.split(Utils::constDirSep, QString::SkipEmptyParts).count()) {
                QDir d(mpdDir+songDir);
                d.cdUp();
                QString fileName=Utils::fixPath(d.absolutePath())+constBackdropFileName+".jpg";
                QFile f(fileName);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(data);
                    f.close();
                    DBUG << "Saved backdrop to" << fileName << "for artist" << currentArtist << ", current song" << currentSong.file;
                    saved=true;
                }
            } else {
                DBUG << "Not saving to mpd folder, mpd dir:" << mpdDir
                     << "num parts:" << songDir.split(Utils::constDirSep, QString::SkipEmptyParts).count();
            }
        } else {
            DBUG << "Not saving to mpd folder - set to save in mpd?" << Settings::self()->storeBackdropsInMpdDir()
                 << "isVa:" << currentSong.isVariousArtists() << "isNonMPD:" << currentSong.isNonMPD()
                 << "mpd readable:" << MPDConnection::self()->getDetails().dirReadable;
        }

        if (!saved) {
            QString cacheName=cacheFileName(currentArtist, true);
            QFile f(cacheName);
            if (f.open(QIODevice::WriteOnly)) {
                DBUG << "Saved backdrop to (cache)" << cacheName << "for artist" << currentArtist << ", current song" << currentSong.file;
                f.write(data);
                f.close();
            }
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

void ContextWidget::resizeBackdrop()
{
    #ifdef SCALE_CONTEXT_BGND
    if (!albumCoverBackdrop && !currentImage.isNull() &&( currentBackdrop.isNull() || (!currentBackdrop.isNull() && currentBackdrop.width()!=width()))) {
        QSize sz(width(), width()*currentImage.height()/currentImage.width());
        currentBackdrop = QPixmap::fromImage(currentImage.scaled(sz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
    #else
    if (!currentBackdrop.isNull() && !albumCoverBackdrop) {
        if (currentBackdrop.width()<minBackdropSize.width() && currentBackdrop.height()<minBackdropSize.height()) {
            QSize size(minBackdropSize);
            if (currentBackdrop.width()<minBackdropSize.width()/4 && currentBackdrop.height()<minBackdropSize.height()/4) {
                size=QSize(minBackdropSize.width()/2, minBackdropSize.height()/2);
            }
            currentBackdrop=currentBackdrop.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else if (maxBackdropSize.width()>1024 && maxBackdropSize.height()>768 &&
                   (currentBackdrop.width()>maxBackdropSize.width() || currentBackdrop.height()>maxBackdropSize.height())) {
            currentBackdrop=currentBackdrop.scaled(maxBackdropSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    #endif
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

NetworkJob * ContextWidget::getReply(QObject *obj)
{
    NetworkJob *reply = qobject_cast<NetworkJob*>(obj);
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
