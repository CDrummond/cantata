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

#include "coverdialog.h"
#include "messagebox.h"
#include "localize.h"
#include "listview.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "mpdconnection.h"
#include "utils.h"
#include "spinner.h"
#include "messageoverlay.h"
#include "icon.h"
#include "icons.h"
#include "qjson/parser.h"
#include "config.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QApplication>
#include <QProgressBar>
#include <QScrollArea>
#include <QDesktopWidget>
#include <QWheelEvent>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KFileDialog>
#else
#include <QFileDialog>
#endif
#include <QFileInfo>
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#include <QMimeData>
#endif
#include <QTemporaryFile>
#include <QDir>
#include <QXmlStreamReader>
#include <QDomDocument>
#include <QDomElement>
#include <QDebug>

#define DBUG if (Covers::debugEnabled()) qWarning() << "CoverDialog" << __FUNCTION__

static int iCount=0;
static const int constMaxTempFiles=20;

static QImage cropImage(QImage img, bool isArtist)
{
    if (isArtist && img.width()!=img.height()) {
        int size=qMin(img.width(), img.height());
        return img.copy((img.width()-size)/2, 0, size, size);
    }
    return img;
}

int CoverDialog::instanceCount()
{
    return iCount;
}

// Only really want square-ish covers!
bool canUse(int w, int h)
{
    return w>90 && h>90 && (w==h || (h<=(w*1.1) && w<=(h*1.1)));
}

enum Providers {
    Prov_LastFm   = 0x0001,
    Prov_Google   = 0x0002,
    Prov_DiscoGs  = 0x0004,
    Prov_CoverArt = 0x0008,
    Prov_Deezer   = 0x0010,
    Prov_Spotify  = 0x0020,
    Prov_ITunes   = 0x0040,

    Prov_All      = 0x007F
};

class CoverItem : public QListWidgetItem
{
public:
    CoverItem(const QString &u, const QString &tu, const QImage &img, const QString &text, QListWidget *parent, int w=-1, int h=-1, int sz=-1)
        : QListWidgetItem(parent)
        , imgUrl(u)
        , thmbUrl(tu)
        , list(parent) {
        setSizeHint(parent->gridSize());
        setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        setToolTip(u);
        if (!img.isNull()) {
            setImage(img);
        }
        if (sz>0) {
            setText(i18nc("name\nwidth x height (file size)", "%1\n%2 x %3 (%4)", text, w, h, Utils::formatByteSize(sz)));
        } else if (w>0 && h>0) {
            setText(i18nc("name\nwidth x height", "%1\n%2 x %3", text, w, h));
        } else {
            setText(text);
        }
    }
    const QString & url() const { return imgUrl; }
    const QString & thumbUrl() const { return thmbUrl; }
    virtual bool isLocal() const { return false; }
    virtual bool isExisting() const { return false; }

protected:
    void setImage(const QImage &img) {
        int size=list && list->parentWidget() && qobject_cast<CoverDialog *>(list->parentWidget())
                    ? static_cast<CoverDialog *>(list->parentWidget())->imageSize() : 100;
        QPixmap pix=QPixmap::fromImage(img.scaled(QSize(size, size), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        if (pix.width()<size || pix.height()<size) {
            QPixmap newPix(size, size);
            newPix.fill(Qt::transparent);
            QPainter p(&newPix);
            p.drawPixmap((size-pix.width())/2, (size-pix.height())/2, pix.width(), pix.height(), pix);
            p.end();
            pix=newPix;
        }
        setIcon(pix);
    }

protected:
    QString imgUrl;
    QString thmbUrl;
    QListWidget *list;
};

class ExistingCover : public CoverItem
{
public:
    ExistingCover(const Covers::Image &i, QListWidget *parent)
        : CoverItem(i.fileName, QString(), i.img, i18n("Current Cover"), parent, i.img.width(), i.img.height(), QFileInfo(i.fileName).size())
        , img(i.img) {
        QFont f(font());
        f.setBold((true));
        setFont(f);
    }
    bool isExisting() const { return true; }
    const QImage & image() const { return img; }
private:
    QImage img;
};

class LocalCover : public CoverItem
{
public:
    LocalCover(const QString &u, const QImage &i, QListWidget *parent)
        : CoverItem(u, QString(), i, Utils::getFile(u), parent, i.width(), i.height(), QFileInfo(u).size())
        , img(i) { }
    bool isLocal() const { return true; }
    const QImage & image() const { return img; }
private:
    QImage img;
};

class LastFmCover : public CoverItem
{
public:
    LastFmCover(const QString &u, const QString &tu, const QImage &img, QListWidget *parent)
        : CoverItem(u, tu, img, QLatin1String("Last.fm"), parent) { }
};

class GoogleCover : public CoverItem
{
public:
    GoogleCover(const QString &u, const QString &tu, const QImage &img, int w, int h, int size, QListWidget *parent)
        : CoverItem(u, tu, img, QLatin1String("Google"), parent, w, h, size*1024) { }
};

class DiscogsCover : public CoverItem
{
public:
    DiscogsCover(const QString &u, const QString &tu, const QImage &img, int w, int h, QListWidget *parent)
        : CoverItem(u, tu, img, QLatin1String("Discogs"), parent, w, h) { }
};

class CoverArtArchiveCover : public CoverItem
{
public:
    CoverArtArchiveCover(const QString &u, const QString &tu, const QImage &img, QListWidget *parent)
        : CoverItem(u, tu, img, i18n("CoverArt Archive"), parent)  { }
};

class DeezerCover : public CoverItem
{
public:
    DeezerCover(const QString &u, const QString &tu, const QImage &img, QListWidget *parent)
        : CoverItem(u, tu, img, QLatin1String("Deezer"), parent)  { }
};

class SpotifyCover : public CoverItem
{
public:
    SpotifyCover(const QString &u, const QString &tu, const QImage &img, QListWidget *parent)
        : CoverItem(u, tu, img, QLatin1String("Spotify"), parent, 640, 640)  { }
};

class ITunesCover : public CoverItem
{
public:
    ITunesCover(const QString &u, const QString &tu, const QImage &img, QListWidget *parent)
        : CoverItem(u, tu, img, QLatin1String("iTunes"), parent, 600, 600)  { }
};

CoverPreview::CoverPreview(QWidget *p)
    : Dialog(p)
    , zoom(1.0)
    , imgW(0),
      imgH(0)
{
    setButtons(Close);
    setWindowTitle(i18n("Image"));
    QWidget *mw=new QWidget(this);
    QVBoxLayout *layout=new QVBoxLayout(mw);
    loadingLabel=new QLabel(i18n("Downloading..."), mw);
    pbar=new QProgressBar(mw);
    imageLabel=new QLabel;
    pbar->setRange(0, 100);
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents(true);
    scrollArea = new QScrollArea(mw);
    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);

    layout->addWidget(loadingLabel);
    layout->addWidget(pbar);
    layout->addWidget(scrollArea);
    layout->setMargin(0);
    setMainWidget(mw);
}

void CoverPreview::showImage(const QImage &img, const QString &u)
{
    if (u==url) {
        zoom=1.0;
        url=u;
        loadingLabel->hide();
        pbar->hide();
        imageLabel->setPixmap(QPixmap::fromImage(img));
        imageLabel->adjustSize();
        scrollArea->show();
        QApplication::processEvents();
        adjustSize();
        QStyleOptionFrameV3 opt;
        opt.init(scrollArea);
        int fw=style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &opt, scrollArea);
        if (fw<0) {
            fw=2;
        }
        fw*=2;
        QRect desktop = qApp->desktop()->screenGeometry(this);
        int maxWidth=desktop.width()*0.75;
        int maxHeight=desktop.height()*0.75;
        int lrPad=width()-mainWidget()->width();
        int tbPad=height()-mainWidget()->height();
        imgW=img.width();
        imgH=img.height();
        resize(lrPad+qMax(100, qMin(maxWidth, imgW+fw)), tbPad+qMax(100, qMin(maxHeight, imgH+fw)));
        setWindowTitle(i18nc("Image (width x height zoom%)", "Image (%1 x %2 %3%)", imgW, imgH, zoom*100));
        show();
    }
}

void CoverPreview::downloading(const QString &u)
{
    url=u;
    if (!url.isEmpty()) {
        loadingLabel->show();
        scrollArea->hide();
        pbar->show();
        pbar->setValue(0);
        int spacing=Utils::layoutSpacing(this);
        QApplication::processEvents();
        adjustSize();
        resize(qMin(200, loadingLabel->width()+32), loadingLabel->height()+pbar->height()+(3*spacing));
        show();
    }
}

void CoverPreview::progress(qint64 rx, qint64 total)
{
    pbar->setValue((int)(((rx*100.0)/(total*1.0))+0.5));
}

void CoverPreview::scaleImage(int adjust)
{
    double newZoom=zoom+(adjust*0.25);

    if (newZoom<0.25 || newZoom>4.0 || (fabs(newZoom-zoom)<0.01)) {
        return;
    }
    zoom=newZoom;
    imageLabel->resize(zoom * imageLabel->pixmap()->size());
    setWindowTitle(i18nc("Image (width x height zoom%)", "Image (%1 x %2 %3%)", imgW, imgH, zoom*100));
}

void CoverPreview::wheelEvent(QWheelEvent *event)
{
    if (scrollArea->isVisible() && QApplication::keyboardModifiers() & Qt::ControlModifier) {
        const int numDegrees = event->delta() / 8;
        const int numSteps = numDegrees / 15;
        if (0!=numSteps) {
            scaleImage(numSteps);
        }
        event->accept();
        return;
    }
    Dialog::wheelEvent(event);
}

CoverDialog::CoverDialog(QWidget *parent)
    : Dialog(parent, "CoverDialog")
    , existing(0)
    , currentQueryProviders(0)
    , preview(0)
    , saving(false)
    , isArtist(false)
    , spinner(0)
    , msgOverlay(0)
    , page(0)
    , menu(0)
    , showAction(0)
    , removeAction(0)
{
    Configuration cfg("CoverDialog");
    enabledProviders=cfg.get("enabledProviders", (int)Prov_All);

    iCount++;
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    setAttribute(Qt::WA_DeleteOnClose);
    setButtons(Cancel|Ok);
    enableButton(Ok, false);
    connect(list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), SLOT(showImage(QListWidgetItem*)));
    connect(list, SIGNAL(itemSelectionChanged()), SLOT(checkStatus()));
    connect(search, SIGNAL(clicked()), SLOT(sendQuery()));
    connect(query, SIGNAL(returnPressed()), SLOT(sendQuery()));
    connect(addFileButton, SIGNAL(clicked()), SLOT(addLocalFile()));
    connect(list, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(menuRequested(const QPoint&)));

    QFont f(list->font());
    QFontMetrics origFm(f);
    iSize=origFm.height()*7;
    iSize=((iSize/10)*10)+((iSize%10) ? 10 : 0);
    f.setPointSizeF(f.pointSizeF()*0.75);
    QFontMetrics fm(f);
    list->setFont(f);
    list->setAcceptDrops(false);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setDragDropMode(QAbstractItemView::NoDragDrop);
    list->setDragEnabled(false);
    list->setDropIndicatorShown(false);
    list->setMovement(QListView::Static);
    list->setGridSize(QSize(imageSize()+10, imageSize()+10+(2.25*fm.height())));
    list->setIconSize(QSize(imageSize(), imageSize()));
    int spacing=style()->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Qt::Vertical);
    if (spacing<0) {
        spacing=4;
    }
    list->setSpacing(spacing);
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMinimumSize((list->gridSize().width()*4)+(spacing*5), list->gridSize().height()*3);
    list->setSortingEnabled(false);

    addFileButton->setIcon(Icon("document-open"));
    addFileButton->setAutoRaise(true);

    configureButton->setIcon(Icons::self()->configureIcon);
    configureButton->setAutoRaise(true);

    QMenu *configMenu=new QMenu(configureButton);
    addProvider(configMenu, QLatin1String("Last.fm"), Prov_LastFm, enabledProviders);
    addProvider(configMenu, i18n("CoverArt Archive"), Prov_CoverArt, enabledProviders);
    addProvider(configMenu, QLatin1String("Google"), Prov_Google, enabledProviders);
    addProvider(configMenu, QLatin1String("Discogs"), Prov_DiscoGs, enabledProviders);
    addProvider(configMenu, QLatin1String("Deezer"), Prov_Deezer, enabledProviders);
    addProvider(configMenu, QLatin1String("Spotify"), Prov_Spotify, enabledProviders);
    addProvider(configMenu, QLatin1String("iTunes"), Prov_ITunes, enabledProviders);
    configureButton->setMenu(configMenu);
    configureButton->setPopupMode(QToolButton::InstantPopup);
    setAcceptDrops(true);
}

CoverDialog::~CoverDialog()
{
    Configuration cfg("CoverDialog");
    cfg.set("enabledProviders", enabledProviders);

    iCount--;
    cancelQuery();
    clearTempFiles();
}

void CoverDialog::show(const Song &s, const Covers::Image &current)
{
    song=s;
    isArtist=song.isArtistImageRequest();
    Covers::Image img=current.img.isNull() ? Covers::locateImage(song) : current;

    if (!img.fileName.isEmpty() && !QFileInfo(img.fileName).isWritable()) {
        MessageBox::error(parentWidget(),
                          isArtist
                            ? i18n("<p>An image already exists for this artist, and the file is not writeable.<p></p><i>%1</i></p>", img.fileName)
                            : i18n("<p>A cover already exists for this album, and the file is not writeable.<p></p><i>%1</i></p>", img.fileName));
        deleteLater();
        return;
    }
    if (isArtist) {
        setCaption(i18n("'%1' Artist Image", song.albumartist));
    } else {
        setCaption(i18nc("'Artist - Album' Album Cover", "'%1 - %2' Album Cover", song.albumArtist(), song.album));
    }
    if (!img.img.isNull()) {
        existing=new ExistingCover(isArtist ? Covers::Image(cropImage(img.img, true), img.fileName) : img, list);
        list->addItem(existing);
    }
    query->setText(isArtist ? song.albumArtist() : QString(song.albumArtist()+QLatin1String(" ")+song.album));
    adjustSize();
    Dialog::show();
    sendQuery();
}

static const char * constHostProperty="host";
static const char * constLargeProperty="large";
static const char * constThumbProperty="thumb";
static const char * constWidthProperty="w";
static const char * constHeightProperty="h";
static const char * constSizeProperty="sz";
static const char * constTypeProperty="type";
static const char * constLastFmHost="ws.audioscrobbler.com";
static const char * constGoogleHost="images.google.com";
static const char * constDiscogsHost="api.discogs.com";
static const char * constCoverArtArchiveHost="coverartarchive.org";
static const char * constSpotifyHost="ws.spotify.com";
static const char * constITunesHost="itunes.apple.com";
static const char * constDeezerHost="api.deezer.com";

void CoverDialog::queryJobFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (!reply) {
        return;
    }
    reply->deleteLater();
    if (!currentQuery.contains(reply)) {
        return;
    }

    DBUG << reply->origUrl().toString() << reply->ok();
    currentQuery.remove(reply);
    if (reply->ok()) {
        QString host=reply->property(constHostProperty).toString();
        QByteArray resp=reply->readAll();
        if (constLastFmHost==host) {
            parseLastFmQueryResponse(resp);
        } else if (constGoogleHost==host) {
            parseGoogleQueryResponse(resp);
        } else if (constDiscogsHost==host) {
            parseDiscogsQueryResponse(resp);
        } else if (constCoverArtArchiveHost==host) {
            parseCoverArtArchiveQueryResponse(resp);
        } else if (constSpotifyHost==host) {
            parseSpotifyQueryResponse(resp);
        } else if (constITunesHost==host) {
            parseITunesQueryResponse(resp);
        } else if (constDeezerHost==host) {
            parseDeezerQueryResponse(resp);
        }
    }
    if (currentQuery.isEmpty()) {
        setSearching(false);
    }
}

void CoverDialog::insertItem(CoverItem *item)
{
    list->addItem(item);
    if (item->isLocal()) {
        list->scrollToItem(item);
        list->setItemSelected(item, true);
    }
}

void CoverDialog::downloadJobFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (!reply) {
        return;
    }
    reply->deleteLater();
    if (!currentQuery.contains(reply)) {
        return;
    }

    DownloadType dlType=(DownloadType)reply->property(constTypeProperty).toInt();

    if (DL_LargeSave==dlType) {
        saving=false;
    }

    DBUG << reply->origUrl().toString() << reply->ok();
    currentQuery.remove(reply);
    if (reply->ok()) {
        QString host=reply->property(constHostProperty).toString();
        QString url=reply->url().toString();
        QByteArray data=reply->readAll();
        const char *format=Covers::imageFormat(data);
        QImage img=QImage::fromData(data, format);
        if (!img.isNull()) {
            bool isLarge=reply->property(constThumbProperty).toString().isEmpty();
            QTemporaryFile *temp=0;

            if (isLarge || (reply->property(constThumbProperty).toString()==reply->property(constLargeProperty).toString())) {
                temp=new QTemporaryFile(QDir::tempPath()+"/cantata_XXXXXX."+(format ? QString(QLatin1String(format)).toLower() : "png"));

                if (temp->open()) {
                    if (!format) {
                        img.save(temp, "PNG");
                    } else {
                        temp->write(data);
                    }
                    if (tempFiles.size()>=constMaxTempFiles) {
                        QTemporaryFile *last=tempFiles.takeLast();
                        last->remove();
                        delete last;
                    }
                    temp->close();
                    temp->setProperty(constLargeProperty, reply->property(constLargeProperty));
                    tempFiles.prepend(temp);
                } else {
                    delete temp;
                    temp=0;
                }
            }
            if (isLarge) {
                if (DL_LargePreview==dlType) {
                    previewDialog()->showImage(cropImage(img, isArtist), reply->property(constLargeProperty).toString());
                } else if (DL_LargeSave==dlType) {
                    if (!temp) {
                        MessageBox::error(this, i18n("Failed to set cover!\nCould not download to temporary file!"));
                    } else if (saveCover(temp->fileName(), img)) {
                        accept();
                    }
                }
            } else {
                CoverItem *item=0;
                img=cropImage(img, isArtist);
                if (constLastFmHost==host) {
                    item=new LastFmCover(reply->property(constLargeProperty).toString(), url, img, list);
                } else if (constGoogleHost==host) {
                    item=new GoogleCover(reply->property(constLargeProperty).toString(), url, img, reply->property(constWidthProperty).toInt(),
                                         reply->property(constHeightProperty).toInt(), reply->property(constSizeProperty).toInt(), list);
                } else if (constDiscogsHost==host) {
                    item=new DiscogsCover(reply->property(constLargeProperty).toString(), url, img, reply->property(constWidthProperty).toInt(),
                                          reply->property(constHeightProperty).toInt(), list);
                } else if (constCoverArtArchiveHost==host) {
                    item=new CoverArtArchiveCover(reply->property(constLargeProperty).toString(), url, img, list);
                } else if (constSpotifyHost==host) {
                    item=new SpotifyCover(reply->property(constLargeProperty).toString(), url, img, list);
                } else if (constITunesHost==host) {
                    item=new ITunesCover(reply->property(constLargeProperty).toString(), url, img, list);
                } else if (constDeezerHost==host) {
                    item=new DeezerCover(reply->property(constLargeProperty).toString(), url, img, list);
                }
                if (item) {
                    insertItem(item);
                }
            }
        }
    } else if (reply->property(constThumbProperty).toString().isEmpty()) {
        if (preview && preview->aboutToShow(reply->property(constLargeProperty).toString())) {
            preview->hide();
        }
        MessageBox::error(this, i18n("Failed to download image!"));
    }
    if (currentQuery.isEmpty()) {
        setSearching(false);
    }
}

void CoverDialog::showImage(QListWidgetItem *item)
{
    if (saving) {
        return;
    }

    CoverItem *cover=static_cast<CoverItem *>(item);

    if (cover->isExisting()) {
        previewDialog()->downloading(cover->url());
        previewDialog()->showImage(static_cast<ExistingCover *>(cover)->image(), cover->url());
    } else if (cover->isLocal()) {
        previewDialog()->downloading(cover->url());
        previewDialog()->showImage(static_cast<LocalCover *>(cover)->image(), cover->url());
    } else {
        previewDialog()->downloading(cover->url());
        NetworkJob *j=downloadImage(cover->url(), DL_LargePreview);
        if (j) {
            j->setProperty(constLargeProperty, cover->url());
            connect(j, SIGNAL(downloadProgress(qint64, qint64)), preview, SLOT(progress(qint64, qint64)));
        }
    }
}

CoverPreview *CoverDialog::previewDialog() {
    if (!preview) {
        preview=new CoverPreview(this);
    }
    return preview;
}

void CoverDialog::sendQuery()
{
    if (saving) {
        return;
    }

    QString fixedQuery(query->text().trimmed());
    fixedQuery.remove(QChar('?'));

    if (fixedQuery.isEmpty()) {
        return;
    }

    if (currentQueryString==fixedQuery && enabledProviders==currentQueryProviders) {
        page++;
    } else {
        page=0;
    }

    if (0==page) {
        QList<CoverItem *> keep;

        while (list->count()) {
            CoverItem *item=static_cast<CoverItem *>(list->takeItem(0));
            if (item->isExisting() || item->isLocal()) {
                keep.append(item);
            } else {
                currentUrls.remove(item->url());
                currentUrls.remove(item->thumbUrl());
                delete item;
            }
        }

        foreach (CoverItem *item, keep) {
            list->addItem(item);
        }

        cancelQuery();
    }

    currentQueryString=fixedQuery;
    if (enabledProviders&Prov_LastFm) {
        sendLastFmQuery(fixedQuery, page);
    }
    if (enabledProviders&Prov_Google) {
        sendGoogleQuery(fixedQuery, page);
    }
    if (enabledProviders&Prov_DiscoGs) {
        sendDiscoGsQuery(fixedQuery, page);
    }
    if (page==0) {
        if (enabledProviders&Prov_Spotify) {
            sendSpotifyQuery(fixedQuery);
        }
        if (enabledProviders&Prov_ITunes) {
            sendITunesQuery(fixedQuery);
        }
        if (enabledProviders&Prov_Deezer) {
            sendDeezerQuery(fixedQuery);
        }
    }
    currentQueryProviders=enabledProviders;
    setSearching(!currentQuery.isEmpty());
}

void CoverDialog::sendLastFmQuery(const QString &fixedQuery, int page)
{
    QUrl url;
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif
    url.setScheme("https");
    url.setHost(constLastFmHost);
    url.setPath("/2.0/");
    query.addQueryItem("api_key", Covers::constLastFmApiKey);
    query.addQueryItem("limit", QString::number(20));
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem(isArtist ? "artist" : "album", fixedQuery);
    query.addQueryItem("method", isArtist ? "artist.search" : "album.search");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    sendQueryRequest(url);
}

void CoverDialog::sendGoogleQuery(const QString &fixedQuery, int page)
{
    QUrl url;
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost(constGoogleHost);
    url.setPath("/images");
    query.addQueryItem("q", fixedQuery);
    query.addQueryItem("gbv", QChar('1'));
    query.addQueryItem("filter", QChar('1'));
    query.addQueryItem("start", QString::number(20 * page));
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    sendQueryRequest(url);
}

void CoverDialog::sendDiscoGsQuery(const QString &fixedQuery, int page)
{
    QUrl url;
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost(constDiscogsHost);
    url.setPath("/search");
    query.addQueryItem("page", QString::number(page + 1));
    query.addQueryItem("per_page", QString::number(20));
    query.addQueryItem("type", isArtist ? "artist" : "release");
    query.addQueryItem("q", fixedQuery);
    query.addQueryItem("f", "json");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    sendQueryRequest(url);
}

void CoverDialog::sendSpotifyQuery(const QString &fixedQuery)
{
    #ifdef ENABLE_HTTPS_SUPPORT
    QUrl url;
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost(constSpotifyHost);
    url.setPath(isArtist ? "/search/1/artist.json" : "/search/1/album.json");
    query.addQueryItem("q", fixedQuery);
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    sendQueryRequest(url);
    #else
    Q_UNUSED(fixedQuery)
    #endif
}

void CoverDialog::sendITunesQuery(const QString &fixedQuery)
{
    if (isArtist) { // TODO???
        return;
    }

    QUrl url;
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost(constITunesHost);
    url.setPath("/search");
    query.addQueryItem("term", fixedQuery);
    query.addQueryItem("limit", QString::number(10));
    query.addQueryItem("media", "music");
    query.addQueryItem("entity", "album");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    sendQueryRequest(url);
}

void CoverDialog::sendDeezerQuery(const QString &fixedQuery)
{
    QUrl url;
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost(constDeezerHost);
    url.setPath(isArtist ? "/search/artist" : "/search/album");
    query.addQueryItem("q", fixedQuery);
    query.addQueryItem("nb_items", QString::number(10));
    query.addQueryItem("output", "json");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    sendQueryRequest(url);
}

void CoverDialog::checkStatus()
{
    QList<QListWidgetItem*> items=list->selectedItems();
    enableButtonOk(1==items.size() && !static_cast<CoverItem *>(items.at(0))->isExisting());
}

void CoverDialog::cancelQuery()
{
    foreach (NetworkJob *job, currentQuery) {
        job->cancelAndDelete();
    }
    currentQuery.clear();
    setSearching(false);
}

void CoverDialog::addLocalFile()
{
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), "image/jpeg image/png", this, i18n("Load Local Cover"));
    #else
    QString fileName=QFileDialog::getOpenFileName(this, i18n("Load Local Cover"), QDir::homePath(), i18n("Images (*.png *.jpg)"));
    #endif

    if (!fileName.isEmpty()) {
        if (currentLocalCovers.contains(fileName)) {
            MessageBox::error(this, i18n("File is already in list!"));
        } else {
            QImage img(fileName);
            if (img.isNull()) {
                MessageBox::error(this, i18n("Failed to read image!"));
            } else {
                currentLocalCovers.insert(fileName);
                insertItem(new LocalCover(fileName, img, list));
            }
        }
    }
}

void CoverDialog::menuRequested(const QPoint &pos)
{
    if (!menu) {
        menu=new QMenu(list);
        showAction=menu->addAction(Icon("zoom-original"), i18n("Display"));
        removeAction=menu->addAction(Icon("list-remove"), i18n("Remove"));
        connect(showAction, SIGNAL(triggered(bool)), SLOT(showImage()));
        connect(removeAction, SIGNAL(triggered(bool)), SLOT(removeImages()));
    }

    QList<QListWidgetItem*> items=list->selectedItems();
    showAction->setEnabled(1==items.count());
    removeAction->setEnabled(!items.isEmpty());
    if (removeAction->isEnabled()) {
        foreach (QListWidgetItem *i, items) {
            if (static_cast<CoverItem *>(i)->isExisting()) {
                removeAction->setEnabled(false);
            }
        }
    }
    menu->popup(list->mapToGlobal(pos));
}

void CoverDialog::showImage()
{
    QList<QListWidgetItem*> items=list->selectedItems();
    if (1==items.count()) {
        showImage(items.at(0));
    }
}

void CoverDialog::removeImages()
{
    QList<QListWidgetItem*> items=list->selectedItems();
    foreach (QListWidgetItem *i, items) {
        delete i;
    }
}

void CoverDialog::updateProviders()
{
    QAction *s=qobject_cast<QAction *>(sender());
    enabledProviders=0;

    if (s) {
        if (Prov_LastFm==s->data().toUInt() && !s->isChecked()) {
            providers[Prov_CoverArt]->setChecked(false);
        } else if (Prov_CoverArt==s->data().toUInt() && s->isChecked()) {
            providers[Prov_LastFm]->setChecked(true);
        }
    }

    foreach (const QAction *act, configureButton->menu()->actions()) {
        if (act->isChecked()) {
            enabledProviders|=act->data().toUInt();
        }
    }
    DBUG << enabledProviders;
}

void CoverDialog::clearTempFiles()
{
    foreach (QTemporaryFile *file, tempFiles) {
        file->remove();
        delete file;
    }
}

void CoverDialog::sendQueryRequest(const QUrl &url, const QString &host)
{
    DBUG << url.toString();
    NetworkJob *j=NetworkAccessManager::self()->get(QNetworkRequest(url));
    j->setProperty(constHostProperty, host.isEmpty() ? url.host() : host);
    j->setProperty(constTypeProperty, (int)DL_Query);
    connect(j, SIGNAL(finished()), this, SLOT(queryJobFinished()));
    currentQuery.insert(j);
}

NetworkJob * CoverDialog::downloadImage(const QString &url, DownloadType dlType)
{
    DBUG << url << dlType;
    if (DL_Thumbnail==dlType) {
        if (currentUrls.contains(url)) {
            return 0;
        }
        currentUrls.insert(url);
    } else {
        foreach (QTemporaryFile *tmp, tempFiles) {
            if (tmp->property(constLargeProperty).toString()==url) {
                QImage img;
                if (img.load(tmp->fileName())) {
                    if (DL_LargePreview==dlType) {
                        previewDialog()->downloading(url);
                        previewDialog()->showImage(img, url);
                    } else if (DL_LargeSave==dlType) {
                        if (saveCover(tmp->fileName(), img)) {
                            accept();
                        }
                    }
                    return 0;
                }
                tmp->remove();
                delete tmp;
                tempFiles.removeAll(tmp);
                break;
            }
        }
    }

    if (DL_LargeSave==dlType) {
        saving=true;
    }

    NetworkJob *j=NetworkAccessManager::self()->get(QNetworkRequest(QUrl(url)));
    connect(j, SIGNAL(finished()), this, SLOT(downloadJobFinished()));
    currentQuery.insert(j);
    j->setProperty(constTypeProperty, (int)dlType);
    if (saving) {
        previewDialog()->downloading(url);
        connect(j, SIGNAL(downloadProgress(qint64, qint64)), preview, SLOT(progress(qint64, qint64)));
    }
    return j;
}

void CoverDialog::downloadThumbnail(const QString &thumbUrl, const QString &largeUrl, const QString &host, int w, int h, int sz)
{
    if (thumbUrl.isEmpty() || largeUrl.isEmpty()) {
        return;
    }

    NetworkJob *j=downloadImage(thumbUrl, DL_Thumbnail);
    if (j) {
        j->setProperty(constThumbProperty, thumbUrl);
        j->setProperty(constLargeProperty, largeUrl);
        j->setProperty(constHostProperty, host);
        if (w>0) {
            j->setProperty(constWidthProperty, w);
        }
        if (h>0) {
            j->setProperty(constHeightProperty, h);
        }
        if (sz>0) {
            j->setProperty(constSizeProperty, sz);
        }
    }
}

typedef QMap<QString, QString> SizeMap;
void CoverDialog::parseLastFmQueryResponse(const QByteArray &resp)
{
    bool inSection=false;
    QXmlStreamReader doc(resp);
    SizeMap urls;
    QList<SizeMap> entries;
    QStringList musibBrainzIds;

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            if (!inSection && QLatin1String(isArtist ? "artist" : "album")==doc.name()) {
                inSection=true;
                urls.clear();
            } else if (inSection && QLatin1String("image")==doc.name()) {
                QString size=doc.attributes().value("size").toString();
                QString url=doc.readElementText();
                if (!size.isEmpty() && !url.isEmpty()) {
                    urls.insert(size, url);
                }
            } else if (inSection && QLatin1String("mbid")==doc.name()) {
                QString id=doc.readElementText();
                if (id.length()>4) {
                    musibBrainzIds.append(id);
                }
            }
        } else if (doc.isEndElement() && inSection && QLatin1String(isArtist ? "artist" : "album")==doc.name()) {
            if (!urls.isEmpty()) {
                entries.append(urls);
                urls.clear();
            }
            inSection=false;
        }
    }

    QStringList largeUrls=QStringList() << "extralarge" << "large";
    QStringList thumbUrls=QStringList() << "large" << "medium" << "small";

    foreach (const SizeMap &urls, entries) {
        QString largeUrl;
        QString thumbUrl;

        foreach (const QString &u, largeUrls) {
            if (urls.contains(u)) {
                largeUrl=urls[u];
                break;
            }
        }

        foreach (const QString &u, thumbUrls) {
            if (urls.contains(u)) {
                thumbUrl=urls[u];
                break;
            }
        }
        downloadThumbnail(thumbUrl, largeUrl, constLastFmHost);
    }

    if (enabledProviders&Prov_CoverArt) {
        foreach (const QString &id, musibBrainzIds) {
            QUrl coverartUrl;
            coverartUrl.setScheme("http");
            coverartUrl.setHost(constCoverArtArchiveHost);
            coverartUrl.setPath("/release/"+id);
            sendQueryRequest(coverartUrl);
        }
    }
}

void CoverDialog::parseGoogleQueryResponse(const QByteArray &resp)
{
    // Code based on Audex CDDA Extractor
    QRegExp rx("<a\\shref=\"(\\/imgres\\?imgurl=[^\"]+)\">[\\s\\n]*<img[^>]+src=\"([^\"]+)\"");
    rx.setCaseSensitivity(Qt::CaseInsensitive);
    rx.setMinimal(true);

    int pos = 0;
    QString xml(QString::fromUtf8(resp));
    QString html = xml.replace(QLatin1String("&amp;"), QLatin1String("&"));

    while (-1!=(pos=rx.indexIn(html, pos))) {
        #if QT_VERSION < 0x050000
        QUrl url("http://www.google.com"+rx.cap(1));
        #else
        QUrl u("http://www.google.com"+rx.cap(1));
        QUrlQuery url(u);
        #endif
        int width=url.queryItemValue("w").toInt();
        int height=url.queryItemValue("h").toInt();
        if (canUse(width, height)) {
            downloadThumbnail(rx.cap(2), url.queryItemValue("imgurl"), constGoogleHost, width, height, url.queryItemValue("sz").toInt());
        }
        pos += rx.matchedLength();
    }
}

void CoverDialog::parseDiscogsQueryResponse(const QByteArray &resp)
{
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok && parsed.contains("resp")) {
        QVariantMap response=parsed["resp"].toMap();
        if (response.contains("search")) {
            QVariantMap search=response["search"].toMap();
            if (search.contains("searchresults")) {
                QVariantMap searchresults=search["searchresults"].toMap();
                if (searchresults.contains("results")) {
                    QVariantList results=searchresults["results"].toList();
                    foreach (const QVariant &r, results) {
                        QVariantMap rm=r.toMap();
                        if (!isArtist && rm.contains("uri")) {
                            QStringList parts=rm["uri"].toString().split("/", QString::SkipEmptyParts);
                            if (!parts.isEmpty()) {
                                QUrl discogsUrl;
                                #if QT_VERSION < 0x050000
                                QUrl &discogsQuery=discogsUrl;
                                #else
                                QUrlQuery discogsQuery;
                                #endif
                                discogsUrl.setScheme("http");
                                discogsUrl.setHost(constDiscogsHost);
                                discogsUrl.setPath("/release/"+parts.last());
                                discogsQuery.addQueryItem("f", "json");
                                #if QT_VERSION >= 0x050000
                                discogsUrl.setQuery(discogsQuery);
                                #endif
                                sendQueryRequest(discogsUrl);
                            }
                        } else if (isArtist && rm.contains("thumb")) {
                            QString thumbUrl=rm["thumb"].toString();
                            if (thumbUrl.contains("/image/A-150-")) {
                                downloadThumbnail(thumbUrl, QString(thumbUrl).replace("image/A-150-", "/image/A-"), constDiscogsHost);
                            }
                        }
                    }
                }
            }
        } else if (response.contains("release")) {
            QVariantMap release=response["release"].toMap();
            if (release.contains("images")) {
                QVariantList images=release["images"].toList();
                foreach (const QVariant &i, images) {
                    QVariantMap im=i.toMap();
                    if (im.contains("uri") && im.contains("uri150") && im.contains("width") && im.contains("height") &&
                        canUse(im["width"].toString().toInt(), im["height"].toString().toInt())) {
                        downloadThumbnail(im["uri150"].toString(), im["uri"].toString(), constDiscogsHost,
                                          im["width"].toString().toInt(), im["height"].toString().toInt());
                    }
                }
            }
        }
    }
}

void CoverDialog::parseCoverArtArchiveQueryResponse(const QByteArray &resp)
{
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok && parsed.contains("images")) {
        QVariantList images=parsed["images"].toList();
        foreach (const QVariant &i, images) {
            QVariantMap im=i.toMap();
            if (im.contains("front") && im["front"].toBool() && im.contains("image") && im.contains("thumbnails")) {
                QVariantMap thumb=im["thumbnails"].toMap();
                QString largeUrl=im["image"].toString();
                QString thumbUrl;
                if (thumb.contains("small")) {
                    thumbUrl=thumb["small"].toString();
                } else if (thumb.contains("large")) {
                    thumbUrl=thumb["large"].toString();
                }
                downloadThumbnail(thumbUrl, largeUrl, constCoverArtArchiveHost);
            }
        }
    }
}

void CoverDialog::parseSpotifyQueryResponse(const QByteArray &resp)
{
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok) {
        if (parsed.contains("info")) { // Initial query response...
            const QString key=QLatin1String(isArtist ? "artists" : "albums");
            const QString href=QLatin1String("spotify:")+QLatin1String(isArtist ? "artist:" : "album:");
            const QString baseUrl=QLatin1String("https://embed.spotify.com/oembed/?url=http://open.spotify.com/")+QLatin1String(isArtist ? "artist/" : "album/");
            if (parsed.contains(key)) {
                QVariantList results=parsed[key].toList();
                foreach (const QVariant &res, results) {
                    QVariantMap item=res.toMap();
                    if (item.contains("href")) {
                        QString url=item["href"].toString();
                        if (url.contains(href)) {
                            url=baseUrl+url.remove(href);
                            sendQueryRequest(QUrl(url), constSpotifyHost);
                        }
                    }
                }
            }
        } else if (parsed.contains("provider_url") && parsed.contains("thumbnail_url")) { // Response to query above
            QString thumbUrl=parsed["thumbnail_url"].toString();
            downloadThumbnail(thumbUrl, QString(thumbUrl).replace("/cover/", "/640/"), constSpotifyHost);
        }
    }
}

void CoverDialog::parseITunesQueryResponse(const QByteArray &resp)
{
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok && parsed.contains("results")) {
        QVariantList results=parsed["results"].toList();
        foreach (const QVariant &res, results) {
            QVariantMap item=res.toMap();
            if (item.contains("artworkUrl100")) {
                QString thumbUrl=item["artworkUrl100"].toString();
                downloadThumbnail(thumbUrl, QString(thumbUrl).replace("100x100", "600x600"), constITunesHost);
            }
        }
    }
}

void CoverDialog::parseDeezerQueryResponse(const QByteArray &resp)
{
    const QString key=QLatin1String(isArtist ? "picture" : "cover");
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok && parsed.contains("data")) {
        QVariantList results=parsed["data"].toList();
        foreach (const QVariant &res, results) {
            QVariantMap item=res.toMap();
            if (item.contains(key)) {
                QString thumbUrl=item[key].toString();
                downloadThumbnail(thumbUrl, thumbUrl+"&size=big", constDeezerHost);
            }
        }
    }
}

void CoverDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        QList<QListWidgetItem*> items=list->selectedItems();
        if (1==items.size()) {
            CoverItem *cover=static_cast<CoverItem *>(items.at(0));
            if (cover->isLocal()) {
                if (saveCover(cover->url(), static_cast<LocalCover *>(cover)->image())) {
                    accept();
                }
            } else if (!cover->isExisting()) {
                NetworkJob *j=downloadImage(cover->url(), DL_LargeSave);
                if (j) {
                    j->setProperty(constLargeProperty, cover->url());
                }
            }
        }
        break;
    }
    case Cancel:
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}

bool CoverDialog::saveCover(const QString &src, const QImage &img)
{
    QString filePath=song.filePath();
    if (song.isCdda()) {
        QString dir = Utils::cacheDir(Covers::constCddaCoverDir, true);
        if (!dir.isEmpty()) {
            QString destName=dir+filePath.mid(7)+src.mid(src.length()-4);
            if (QFile::exists(destName)) {
                QFile::remove(destName);
            }
            if (QFile::copy(src, destName)) {
                emit selectedCover(img, destName);
                return true;
            }
        }
        MessageBox::error(this, i18n("Failed to set cover!\nCould not make copy!"));
        return false;
    }
    QString existingBackup;

    if (existing && !existing->url().isEmpty()) {
        static const QLatin1String constBakExt(".bak");
        existingBackup=existing->url()+constBakExt;
        if (!QFile::rename(existing->url(), existingBackup)) {
            MessageBox::error(this, i18n("Failed to set cover!\nCould not backup original!"));
            return false;
        }
    }

    QString destName;
    QString mpdDir;
    QString dirName;
    bool saveInMpd=Settings::self()->storeCoversInMpdDir();

    if (saveInMpd) {
        bool haveAbsPath=filePath.startsWith('/');
        mpdDir=MPDConnection::self()->getDetails().dir;
        if (haveAbsPath || !mpdDir.isEmpty()) {
            dirName=filePath.endsWith('/') ? (haveAbsPath ? QString() : mpdDir)+filePath
                                            : Utils::getDir((haveAbsPath ? QString() : mpdDir)+filePath);
        }
    }

    if (isArtist) {
        if (saveInMpd && !mpdDir.isEmpty() && dirName.startsWith(mpdDir) && 2==dirName.mid(mpdDir.length()).split('/', QString::SkipEmptyParts).count()) {
            QDir d(dirName);
            d.cdUp();
            destName=d.absolutePath()+'/'+Covers::artistFileName(song)+src.mid(src.length()-4);
        } else {
            destName=Utils::cacheDir(Covers::constCoverDir, true)+Covers::encodeName(song.albumartist)+src.mid(src.length()-4);
        }
    } else {
        if (saveInMpd) {
            destName=dirName+Covers::albumFileName(song)+src.mid(src.length()-4);
        } else { // Save to cache dir...
            QString dir(Utils::cacheDir(Covers::constCoverDir+Covers::encodeName(song.albumArtist()), true));
            destName=dir+Covers::encodeName(song.album)+src.mid(src.length()-4);
        }
    }

    if (!destName.startsWith("http://") && QFile::copy(src, destName)) {
        Utils::setFilePerms(destName);
        if (!existingBackup.isEmpty() && QFile::exists(existingBackup)) {
            QFile::remove(existingBackup);
        }
        Covers::self()->emitCoverUpdated(song, img, destName);
        return true;
    } else {
        if (existing && !existingBackup.isEmpty()) {
            QFile::rename(existingBackup, existing->url());
        }
        MessageBox::error(this, i18n("Failed to set cover!\nCould not copy file to '%1'!", destName));
        return false;
    }
}

static bool hasMimeType(QDropEvent *event)
{
    #if QT_VERSION < 0x050000
    return event->provides("text/uri-list");
    #else
    return event->mimeData()->formats().contains(QLatin1String("text/uri-list"));
    #endif
}

void CoverDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (hasMimeType(event)) {
        event->acceptProposedAction();
    }
}

void CoverDialog::dropEvent(QDropEvent *event)
{
    if (hasMimeType(event) && Qt::CopyAction==event->dropAction()) {
        event->acceptProposedAction();

        QList<QUrl> urls(event->mimeData()->urls());
        foreach (const QUrl &url, urls) {
            if (url.scheme().isEmpty() || "file"==url.scheme()) {
                QString path=url.path();
                if (!currentLocalCovers.contains(path) && (path.endsWith(".jpg", Qt::CaseInsensitive) || path.endsWith(".png", Qt::CaseInsensitive))) {
                    QImage img(path);
                    if (!img.isNull()) {
                        currentLocalCovers.insert(path);
                        insertItem(new LocalCover(path, img, list));
                    }
                }
            }
        }
    }
}

void CoverDialog::setSearching(bool s)
{
    if (!spinner && !s) {
        return;
    }
    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(list);
    }
    if (!msgOverlay) {
        msgOverlay=new MessageOverlay(this);
        msgOverlay->setWidget(list);
        connect(msgOverlay, SIGNAL(cancel()), SLOT(cancelQuery()));
    }
    if (s) {
        spinner->start();
        msgOverlay->setText(i18n("Searching..."));
    } else {
        spinner->stop();
        msgOverlay->setText(QString());
    }
}

void CoverDialog::addProvider(QMenu *mnu, const QString &name, int bit, int value)
{
    QAction *act=mnu->addAction(name);
    act->setData(bit);
    act->setCheckable(true);
    act->setChecked(value&bit);
    connect(act, SIGNAL(toggled(bool)), this, SLOT(updateProviders()));
    providers.insert(bit, act);
}
