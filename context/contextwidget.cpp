/*
 * Cantata
 *
 * Copyright (c) 2011-2019 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "onlineview.h"
#include "mpd-interface/song.h"
#include "support/utils.h"
#include "gui/apikeys.h"
#include "gui/covers.h"
#include "gui/settings.h"
#include "network/networkaccessmanager.h"
#include "wikipediaengine.h"
#include "support/gtkstyle.h"
#include "widgets/playqueueview.h"
#include "widgets/treeview.h"
#include "widgets/thinsplitterhandle.h"
#ifdef Q_OS_MAC
#include "support/osxstyle.h"
#endif
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QStylePainter>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QXmlStreamReader>
#include <QFile>
#include <QWheelEvent>
#include <QApplication>
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
const QLatin1String ContextWidget::constCacheDir("backdrops/");

static QString cacheFileName(const QString &artist, bool createDir)
{
    return Utils::cacheDir(ContextWidget::constCacheDir, createDir)+Covers::encodeName(artist)+".jpg";
}

class ViewSelectorButton : public QToolButton
{
public:
    ViewSelectorButton(QWidget *p) : QToolButton(p) { }
    void paintEvent(QPaintEvent *ev) override
    {
        Q_UNUSED(ev)
        QPainter painter(this);
        QString txt=text();
        bool mo=underMouse();

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
        if (isChecked() || mo) {
            int lh=Utils::isHighDpi() ? 5 : 3;
            #ifdef Q_OS_MAC
            QColor col=OSXStyle::self()->viewPalette().highlight().color();
            #else
            QColor col=palette().color(QPalette::Highlight);
            #endif
            if (mo) {
                col.setAlphaF(isChecked() ? 0.75 : 0.5);
            }
            QRect r=rect();
            painter.fillRect(r.x(), r.y()+r.height()-(lh+1), r.width(), lh, col);
        }
        painter.setPen(palette().color(QPalette::Text));
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
    resetAct=new QAction(tr("Reset Spacing"), this);
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
    for (int s: sizes()) {
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
    , shown(false)
    , job(nullptr)
    , alwaysCollapsed(false)
    , backdropType(PlayQueueView::BI_Cover)
    , darkBackground(false)
    , fadeValue(1.0)
    , isWide(false)
    , stack(nullptr)
    , onlineContext(nullptr)
    , splitter(nullptr)
    , viewSelector(nullptr)
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
        splitter->setCollapsible(0, false);
        splitter->setCollapsible(1, false);
        splitter->setCollapsible(2, false);
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
            viewSelector->addItem(tr("&Artist"), "artist");
            viewSelector->addItem(tr("Al&bum"), "album");
            viewSelector->addItem(tr("&Track"), "song");
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
    resizeBackdrop();
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
            updateImage(customBackdropFile.isEmpty() ? QImage() : QImage(customBackdropFile));
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
        darkBackground = u;
        updatePalette();
    }
}

void ContextWidget::updatePalette()
{
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
        // pal.setColor(QPalette::WindowText, light);
        // pal.setColor(QPalette::ButtonText, light);
        pal.setColor(QPalette::Text, light);
        pal.setColor(QPalette::Link, light);
        pal.setColor(QPalette::LinkVisited, linkVisited);
        prevLinkColor=appLinkColor;
        linkCol=pal.color(QPalette::Link);
    } else {
        linkCol=appLinkColor;
        prevLinkColor=QColor(240, 240, 240);
        pal.setColor(QPalette::Base, pal.color(QPalette::Window));
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

void ContextWidget::showEvent(QShowEvent *e)
{
    setWide(width()>minWidth && !alwaysCollapsed);
    if (backdropType) {
        updateBackdrop();
    }
    if (!shown) {
        // Some styles (e.g Adwaita-Qt) draw base colour for scrollbar background.
        // We need to fix this to use the window background. Therefore, the first
        // time we are shown set the palette.
        updatePalette();
        shown = true;
    }
    QWidget::showEvent(e);
}

void ContextWidget::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    QRect r(rect());

    if (darkBackground) {
        p.fillRect(r, palette().background().color());
    } else {
        QColor col = palette().windowText().color();
        col.setAlphaF(0.15);
        p.setPen(col);
        p.drawLine(r.topLeft(), r.topRight());
        if (parentWidget() && parentWidget()->parentWidget() && 0==qstrcmp(parentWidget()->parentWidget()->metaObject()->className(), "QStackedWidget")) {
            if (Qt::LeftToRight==layoutDirection()) {
                p.drawLine(r.topLeft()+QPoint(0, 1), r.bottomLeft());
            } else {
                p.drawLine(r.topRight()+QPoint(0, 1), r.bottomRight());
            }
        }
    }
    if (backdropType) {
        if (!oldBackdrop.isNull()) {
            if (!qFuzzyCompare(fadeValue, qreal(0.0))) {
                p.setOpacity(1.0-fadeValue);
            }
            if (oldBackdrop.height()<height()) {
                p.drawPixmap(0, (height()-oldBackdrop.height())/2, oldBackdrop);
            } else {
                p.fillRect(r, QBrush(oldBackdrop));
            }
        }
        if (!currentBackdrop.isNull()) {
            p.setOpacity(fadeValue);
            if (currentBackdrop.height()<height()) {
                p.drawPixmap(0, (height()-currentBackdrop.height())/2, currentBackdrop);
            } else {
                p.fillRect(r, QBrush(currentBackdrop));
            }
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
        }
        QWidget::update();
    }
}

void ContextWidget::updateImage(QImage img)
{
    DBUG << img.isNull() << currentBackdrop.isNull();
    oldBackdrop=currentBackdrop;
    currentBackdrop=QPixmap();
    animator.stop();
    if (img.isNull() && oldBackdrop.isNull()) {
        return;
    }
    if (img.isNull()) {
        currentImage=img;
    } else {
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
        currentImage=img;
    }
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
    QWidget::update();
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
        job=nullptr;
    }
}

void ContextWidget::updateBackdrop(bool force)
{
    DBUG << updateArtist << currentArtist << currentSong.file << force;
    if (!force && updateArtist==currentArtist) {
        return;
    }
    currentArtist=updateArtist;
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
                for (const QString &fileName: names) {
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
            for (const QString &fileName: names) {
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
    getFanArtBackdrop();
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
    QUrlQuery query;

    query.addQueryItem("query", "artist:"+artist);
    url.setQuery(query);

    job = NetworkAccessManager::self()->get(url);
    DBUG << url.toString();
    job->setProperty(constArtistProp, artist);
    connect(job, SIGNAL(finished()), this, SLOT(musicbrainzResponse()));
}

void ContextWidget::musicbrainzResponse()
{
    NetworkJob *reply = getReply(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();

    QString id;
    QString currentId;
    QString artist=reply->property(constArtistProp).toString();

    if (reply->ok()) {
        bool inSection=false;
        QXmlStreamReader doc(reply->actualJob());

        while (!doc.atEnd()) {
            doc.readNext();

            if (doc.isStartElement()) {
                if (!inSection && QLatin1String("artist-list")==doc.name()) {
                    inSection=true;
                } else if (inSection && QLatin1String("artist")==doc.name()) {
                    // Store this artist ID as the current ID
                    currentId=doc.attributes().value("id").toString();
                    if (id.isEmpty()) {
                        // If we have no ID set, then use the first one - for now
                        id=currentId;
                    }
                } else if (inSection && QLatin1String("name")==doc.name()) {
                    if (doc.readElementText()==artist) {
                        // Found an arist in the artist-list whose name matches what we are looking for, so use this.
                        id=currentId;
                        break;
                    }
                }
            } else if (doc.isEndDocument() && inSection && QLatin1String("artist-list")==doc.name()) {
                break;
            }
        }
    }

    if (id.isEmpty()) {
        // MusicBrainz does not seem to like AC/DC, but AC DC works - so if we fail with an artist
        // containing /, then try with space...
        if (!artist.isEmpty() && artist.contains("/")) {
            artist=artist.replace("/", " ");
            getMusicbrainzId(artist);
        } else {
            updateImage(QImage());
        }
    } else {
        QUrl url("http://webservice.fanart.tv/v3/music/"+id);
        QUrlQuery query;

        ApiKeys::self()->addKey(query, ApiKeys::FanArt);
        url.setQuery(query);

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
        QJsonParseError jsonParseError;
        QVariantMap parsed=QJsonDocument::fromJson(reply->readAll(), &jsonParseError).toVariant().toMap();
        bool ok=QJsonParseError::NoError==jsonParseError.error;

        if (ok && !parsed.isEmpty()) {
            QVariantList artistbackgrounds;

            if (parsed.contains("artistbackground")) {
                artistbackgrounds=parsed["artistbackground"].toList();
            } else {
                QVariantMap artist=parsed[parsed.keys().first()].toMap();

                if (artist.contains("artistbackground")) {
                    artistbackgrounds=artist["artistbackground"].toList();
                }
            }
            if (!artistbackgrounds.isEmpty()) {
                QVariantMap artistbackground=artistbackgrounds.first().toMap();
                if (artistbackground.contains("url")) {
                    url=artistbackground["url"].toString();
                }
            }
        }
    }

    if (url.isEmpty()) {
        updateImage(QImage());
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

    if (!img.isNull()) {
        QString cacheName=cacheFileName(currentArtist, true);
        QFile f(cacheName);
        if (f.open(QIODevice::WriteOnly)) {
            DBUG << "Saved backdrop to (cache)" << cacheName << "for artist" << currentArtist << ", current song" << currentSong.file;
            f.write(data);
            f.close();
        }
    }
    updateImage(img);
}

void ContextWidget::resizeBackdrop()
{
    if (!currentImage.isNull() &&( currentBackdrop.isNull() || (!currentBackdrop.isNull() && currentBackdrop.width()!=width()))) {
        QSize sz(width(), width()*currentImage.height()/currentImage.width());
        currentBackdrop = QPixmap::fromImage(currentImage.scaled(sz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }
}

NetworkJob * ContextWidget::getReply(QObject *obj)
{
    NetworkJob *reply = qobject_cast<NetworkJob*>(obj);
    if (!reply) {
        return nullptr;
    }

    reply->deleteLater();
    if (reply!=job) {
        return nullptr;
    }
    job=nullptr;
    return reply;
}

#include "moc_contextwidget.cpp"
