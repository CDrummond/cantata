/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "covers.h"
#include "messagebox.h"
#include "localize.h"
#include "listview.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "mpdconnection.h"
#include "utils.h"
#include "spinner.h"
#include "icon.h"
#include <QtGui/QVBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QListWidget>
#include <QtGui/QPixmap>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QApplication>
#include <QtGui/QProgressBar>
#include <QtGui/QScrollArea>
#include <QtGui/QDesktopWidget>
#include <QtGui/QWheelEvent>
#include <QtGui/QScrollBar>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KFileDialog>
#else
#include <QtGui/QFileDialog>
#endif
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtCore/QTemporaryFile>
#include <QtCore/QDir>
#include <QtXml/QXmlStreamReader>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

static int iCount=0;
static const int constMaxTempFiles=20;

int CoverDialog::instanceCount()
{
    return iCount;
}

class CoverItem : public QListWidgetItem
{
public:
    enum Type {
        Type_Existing,
        Type_Local,
        Type_LastFm,
        Type_Google
//        Type_Yahoo,
//        Type_Discogs
    };

    CoverItem(const QString &u, QListWidget *parent)
        : QListWidgetItem(parent)
        , imgUrl(u)
        , list(parent) {
        setSizeHint(parent->gridSize());
        setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        setToolTip(u);
    }
    virtual quint32 key() const =0;
    virtual Type type() const =0;
    const QString & url() const { return imgUrl; }

    //bool operator<(const CoverItem &o) const {
    //    return key()<o.key();
    //}

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
    QListWidget *list;
};

class LastFmCover : public CoverItem
{
public:
    LastFmCover(const QString &u, const QImage &img, QListWidget *parent)
        : CoverItem(u, parent) {
        setImage(img);
        setText(i18n("Last.fm"));
    }

    quint32 key() const { return 0xFFFFFFFD; }
    Type type()  const { return Type_LastFm; }
};

class LocalCover : public CoverItem
{
public:
    LocalCover(const QString &u, const QImage &i, QListWidget *parent)
        : CoverItem(u, parent)
        , img(i) {
        setImage(i);
        setText(i18nc("name\nwidth x height (file size)", "%1\n%2 x %3 (%4)")
                .arg(Utils::getFile(u)).arg(img.width()).arg(img.height()).arg(Utils::formatByteSize(QFileInfo(u).size())));
    }

    quint32 key() const { return 0xFFFFFFFE; }
    Type type()  const { return Type_Local; }
    const QImage & image() const { return img; }
private:
    QImage img;
};

class GoogleCover : public CoverItem
{
public:
    GoogleCover(const QString &u, const QImage &img, int w, int h, int size, QListWidget *parent)
        : CoverItem(u, parent)
        , width(w)
        , height(h) {
        setImage(img);
        setText(i18nc("Google\nwidth x height (file size)", "Google\n%1 x %2 (%3)").arg(width).arg(height).arg(Utils::formatByteSize(size*1024)));
    }

    quint32 key() const { return width*height; }
    Type type()  const { return Type_Google; }

private:
    int width;
    int height;
};

//class YahooCover : public CoverItem
//{
//public:
//    YahooCover(const QString &u, const QImage &img, QListWidget *parent)
//        : CoverItem(u, parent) {
//        setImage(img);
//        setText(i18n("Yahoo!"));
//    }

//    quint32 key() const { return 0xFFFFFFFE; }
//    Type type()  const { return Type_Yahoo; }
//};

//class DiscogsCover : public CoverItem
//{
//public:
//    DiscogsCover(const QString &u, const QImage &img, int w, int h, QListWidget *parent)
//        : CoverItem(u, parent)
//        , width(w)
//        , height(h) {
//        setImage(img);
//        setText(i18nc("Discogs\nwidth x height", "Discogs\n%1 x %2").arg(width).arg(height));
//    }

//    quint32 key() const { return width*height; }
//    Type type()  const { return Type_Discogs; }

//private:
//    int width;
//    int height;
//};

class ExistingCover : public CoverItem
{
public:
    ExistingCover(const Covers::Image &i, QListWidget *parent)
        : CoverItem(i.fileName, parent)
        , img(i) {
        setImage(img.img);
        QFont f(font());
        f.setBold((true));
        setFont(f);
        setText(i18nc("Current Cover\nwidth x height", "Current Cover\n%1 x %2").arg(img.img.width()).arg(img.img.height()));
    }

    quint32 key() const { return 0xFFFFFFFF; }
    Type type()  const { return Type_Existing; }
    const QImage & image() const { return img.img; }

private:
    Covers::Image img;
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
        setWindowTitle(i18nc("Image (width x height zoom%)", "Image (%1 x %2 %3%)").arg(imgW).arg(imgH).arg(zoom*100));
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
        int spacing=style()->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Qt::Vertical);
        if (spacing<0) {
            spacing=4;
        }
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
    setWindowTitle(i18nc("Image (width x height zoom%)", "Image (%1 x %2 %3%)").arg(imgW).arg(imgH).arg(zoom*100));
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
    : Dialog(parent)
    , existing(0)
    , preview(0)
    , saving(false)
    , spinner(0)
    , page(0)
    , menu(0)
    , showAction(0)
    , removeAction(0)
{
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
    connect(cancelButton, SIGNAL(clicked()), SLOT(cancelQuery()));
    connect(addFileButton, SIGNAL(clicked()), SLOT(addLocalFile()));
    connect(list, SIGNAL(customContextMenuRequested(const QPoint&)), SLOT(menuRequested(const QPoint&)));

    QFont f(list->font());
    QFontMetrics origFm(f);
    iSize=origFm.height()*7;
    iSize=((iSize/10)*10)+(iSize%10 ? 10 : 0);
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
    cancelButton->setIcon(Icon("stop"));
    cancelButton->setEnabled(false);
    cancelButton->setAutoRaise(true);
    addFileButton->setAutoRaise(true);
    setAcceptDrops(true);
}

CoverDialog::~CoverDialog()
{
    iCount--;
    cancelQuery();
    clearTempFiles();
}

void CoverDialog::show(const Song &s)
{
    song=s;
    Covers::Image img=Covers::getImage(song);

    if (!img.fileName.isEmpty() && !QFileInfo(img.fileName).isWritable()) {
        MessageBox::error(parentWidget(), i18n("<p>A cover already exists for this album, and the file is not writeable.<p></p><i>%1</i></p>").arg(img.fileName));
        deleteLater();
        return;
    }
    setCaption(i18nc("Album by Artist", "%1 by %2").arg(song.album).arg(song.albumArtist()));
    if (!img.img.isNull()) {
        existing=new ExistingCover(img, list);
        list->addItem(existing);
    }
    query->setText(song.album+" "+song.albumArtist());
    sendQuery();
    Dialog::show();
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
//static const char * constYahooHost="boss.yahooapis.com";
//static const char * constDiscogsHost="www.discogs.com";
//static const char * constYahooApiKey=0;
//static const char * constDiscogsApiKey=0;

void CoverDialog::queryJobFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply || !currentQuery.contains(reply)) {
        return;
    }

    currentQuery.remove(reply);
    if(QNetworkReply::NoError==reply->error()) {
        QString resp=QString::fromUtf8(reply->readAll());
        QString host=reply->property(constHostProperty).toString();
        if (constLastFmHost==host) {
            parseLstFmQueryResponse(resp);
        } else if (constGoogleHost==host) {
            parseGoogleQueryResponse(resp);
        }
//        else if (constYahooHost==host) {
//            parseYahooQueryResponse(resp);
//        } else if (constDiscogsHost==host) {
//            parseDiscogsQueryResponse(resp);
//        }
    }
    reply->deleteLater();
    if (spinner && currentQuery.isEmpty()) {
        spinner->stop();
    }
}

void CoverDialog::insertItem(CoverItem *item)
{
    /*
    int pos=0;
    for (; pos<list->count(); ++pos) {
        CoverItem *c=(CoverItem *)list->item(pos);
        if (c->key()<item->key()) {
            break;
        }
    }

    qWarning() << "INSERT" << pos << item->key();
    list->insertItem(pos, item);

    for (; pos<list->count(); ++pos) {
        CoverItem *c=(CoverItem *)list->item(pos);
        qWarning() << "...      " << pos << c->key();
    }*/

    /*
    QMultiMap<int, CoverItem *> sortItems;
    for (int pos=0; pos<list->count(); ++pos) {
        CoverItem *i=(CoverItem *)list->item(pos);
        sortItems.insert(i->key(), i);
    }
    sortItems.insert(item->key(), item);
    QList<CoverItem *> coverItems = sortItems.values();

    foreach (CoverItem *i, coverItems) {
        list->addItem(i);
    }
    */
    list->addItem(item);
    if (CoverItem::Type_Local==item->type()) {
        list->scrollToItem(item);
        list->setItemSelected(item, true);
    }
}

void CoverDialog::downloadJobFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply || !currentQuery.contains(reply)) {
        return;
    }

    DownloadType dlType=(DownloadType)reply->property(constTypeProperty).toInt();

    if (DL_LargeSave==dlType) {
        saving=false;
    }

    currentQuery.remove(reply);
    if(QNetworkReply::NoError==reply->error()) {
        QString url=reply->url().toString();
        QByteArray data=reply->readAll();
        FileType fileType=url.endsWith(".jpg") ? FT_Jpg : (url.endsWith(".png") ? FT_Png : FT_Other);
        QImage img=QImage::fromData(data, FT_Jpg==fileType ? "JPG" : (FT_Png==fileType ? "PNG" : 0));
        if (!img.isNull()) {
            QString host=reply->property(constHostProperty).toString();
            bool isLarge=reply->property(constThumbProperty).toString().isEmpty();
            QTemporaryFile *temp=0;

            if (isLarge || (reply->property(constThumbProperty).toString()==reply->property(constLargeProperty).toString())) {
                temp=new QTemporaryFile(QDir::tempPath()+"/cantata_XXXXXX."+(FT_Jpg==fileType ? "jpg" : "png"));

                if (temp->open()) {
                    if (FT_Other==fileType) {
                        img.save(temp, "PNG");
                    } else {
                        temp->write(data);
                    }
                    if (tempFiles.size()>=constMaxTempFiles) {
                        QTemporaryFile *last=tempFiles.takeLast();
                        last->remove();
                        delete last;
                    }
                    temp->setProperty(constLargeProperty, reply->property(constLargeProperty));
                    tempFiles.prepend(temp);
                } else {
                    delete temp;
                    temp=0;
                }
            }
            if (isLarge) {
                if (DL_LargePreview==dlType) {
                    previewDialog()->showImage(img, reply->property(constLargeProperty).toString());
                } else if (DL_LargeSave==dlType) {
                    if (!temp) {
                        MessageBox::error(this, i18n("Failed to set cover!\nCould not daownload to temporary file!"));
                    } else if (saveCover(temp->fileName(), img)) {
                        accept();
                    }
                }
            } else {
                CoverItem *item=0;
                if (constLastFmHost==host) {
                    item=new LastFmCover(reply->property(constLargeProperty).toString(), img, list);
                } else if (constGoogleHost==host) {
                    item=new GoogleCover(reply->property(constLargeProperty).toString(), img,
                                         reply->property(constWidthProperty).toInt(), reply->property(constHeightProperty).toInt(),
                                         reply->property(constSizeProperty).toInt(), list);
                }
//                else if (constYahooHost==host) {
//                    item=new YahooCover(reply->property(constLargeProperty).toString(), img, list);
//                } else if (constDiscogsHost==host) {
//                    item=new DiscogsCover(reply->property(constLargeProperty).toString(), img,
//                                          reply->property(constWidthProperty).toInt(), reply->property(constHeightProperty).toInt(), list);
//                }
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
    reply->deleteLater();
    if (spinner && currentQuery.isEmpty()) {
        spinner->stop();
        cancelButton->setEnabled(false);
    }
}

void CoverDialog::showImage(QListWidgetItem *item)
{
    if (saving) {
        return;
    }

    CoverItem *cover=(CoverItem *)item;

    if (CoverItem::Type_Existing==cover->type()) {
        previewDialog()->downloading(cover->url());
        previewDialog()->showImage(((ExistingCover *)cover)->image(), cover->url());
    } else if (CoverItem::Type_Local==cover->type()) {
        previewDialog()->downloading(cover->url());
        previewDialog()->showImage(((LocalCover *)cover)->image(), cover->url());
    } else {
        previewDialog()->downloading(cover->url());
        QNetworkReply *j=downloadImage(cover->url(), DL_LargePreview);
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

    if (currentQueryString==fixedQuery) {
        page++;
    } else {
        page=0;
    }

    if (0==page) {
        QList<CoverItem *> keep;

        while (list->count()) {
            CoverItem *item=(CoverItem *)list->takeItem(0);
            if (CoverItem::Type_Existing==item->type() || CoverItem::Type_Local==item->type()) {
                keep.append(item);
            } else {
                delete item;
            }
        }

        foreach (CoverItem *item, keep) {
            list->addItem(item);
        }

        cancelQuery();
    }


    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(list->viewport());
    }
    spinner->start();
    cancelButton->setEnabled(true);

    currentQueryString=fixedQuery;
    QUrl lastFm;
    lastFm.setScheme("http");
    lastFm.setHost(constLastFmHost);
    lastFm.setPath("/2.0/");
    lastFm.addQueryItem("api_key", Covers::constLastFmApiKey);
    lastFm.addQueryItem("limit", QString::number(20));
    lastFm.addQueryItem("page", QString::number(page));
    lastFm.addQueryItem("album", fixedQuery);
    lastFm.addQueryItem("method", "album.search");
    sendQueryRequest(lastFm);

    QUrl google;
    google.setScheme("http");
    google.setHost(constGoogleHost);
    google.setPath("/images");
    google.addQueryItem("q", fixedQuery);
    google.addQueryItem("gbv", QChar('1'));
    google.addQueryItem("filter", QChar('1'));
    google.addQueryItem("start", QString::number(20 * page));
    sendQueryRequest(google);

//    QUrl yahoo;
//    yahoo.setScheme("http");
//    yahoo.setHost(constYahooHost);
//    yahoo.setPath("/ysearch/images/v1/" + fixedQuery);
//    yahoo.addQueryItem("appid", constYahooApiKey);
//    yahoo.addQueryItem("count", QString::number(20));
//    yahoo.addQueryItem("start", QString::number(20 * page));
//    yahoo.addQueryItem("format", "xml");
//    sendQueryRequest(yahoo);

//    QUrl discogs;
//    discogs.setScheme("http");
//    discogs.setHost(constDiscogsHost);
//    discogs.setPath("/search");
//    discogs.addQueryItem("api_key", constDiscogsApiKey);
//    discogs.addQueryItem("page", QString::number(page + 1));
//    discogs.addQueryItem("type", "all");
//    discogs.addQueryItem("q", fixedQuery);
//    discogs.addQueryItem("f", "xml");
//    sendQueryRequest(discogs);
}

void CoverDialog::checkStatus()
{
    QList<QListWidgetItem*> items=list->selectedItems();
    enableButtonOk(1==items.size() && CoverItem::Type_Existing!=((CoverItem *)items.at(0))->type());
}

void CoverDialog::cancelQuery()
{
    foreach (QNetworkReply *job, currentQuery) {
        if (DL_Query==job->property(constTypeProperty).toInt()) {
            disconnect(job, SIGNAL(finished()), this, SLOT(queryJobFinished()));
        } else {
            disconnect(job, SIGNAL(finished()), this, SLOT(downloadJobFinished()));
        }
        job->close();
        job->deleteLater();
    }
    currentQuery.clear();

    if (spinner) {
        spinner->stop();
    }
    cancelButton->setEnabled(false);
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
            CoverItem *c=(CoverItem *)i;
            if (CoverItem::Type_Existing==c->type()) {
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

void CoverDialog::clearTempFiles()
{
    foreach (QTemporaryFile *file, tempFiles) {
        file->remove();
        delete file;
    }
}

void CoverDialog::sendQueryRequest(const QUrl &url)
{
    QNetworkReply *j=NetworkAccessManager::self()->get(QNetworkRequest(url));
    j->setProperty(constHostProperty, url.host());
    j->setProperty(constTypeProperty, (int)DL_Query);
    connect(j, SIGNAL(finished()), this, SLOT(queryJobFinished()));
    currentQuery.insert(j);
}

QNetworkReply * CoverDialog::downloadImage(const QString &url, DownloadType dlType)
{
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

    QNetworkReply *j=NetworkAccessManager::self()->get(QNetworkRequest(QUrl(url)));
    connect(j, SIGNAL(finished()), this, SLOT(downloadJobFinished()));
    currentQuery.insert(j);
    j->setProperty(constTypeProperty, (int)dlType);
    if (saving) {
        previewDialog()->downloading(url);
        connect(j, SIGNAL(downloadProgress(qint64, qint64)), preview, SLOT(progress(qint64, qint64)));
    }
    return j;
}

typedef QMap<QString, QString> SizeMap;
void CoverDialog::parseLstFmQueryResponse(const QString &resp)
{
    bool inSection=false;
    QXmlStreamReader doc(resp);
    SizeMap urls;
    QList<SizeMap> entries;

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            if (!inSection && QLatin1String("album")==doc.name()) {
                inSection=true;
                urls.clear();
            } else if (inSection && QLatin1String("image")==doc.name()) {
                QString size=doc.attributes().value("size").toString();
                QString url=doc.readElementText();
                if (!size.isEmpty() && !url.isEmpty()) {
                    urls.insert(size, url);
                }
            }
        } else if (doc.isEndElement() && inSection && QLatin1String("album")==doc.name()) {
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

        if (!largeUrl.isEmpty() && !thumbUrl.isEmpty()) {
            QNetworkReply *j=downloadImage(thumbUrl, DL_Thumbnail);
            if (j) {
                j->setProperty(constHostProperty, constLastFmHost);
                j->setProperty(constLargeProperty, largeUrl);
                j->setProperty(constThumbProperty, thumbUrl);
            }
        }
    }
}

void CoverDialog::parseGoogleQueryResponse(const QString &resp)
{
    // Code based on Audex CDDA Extractor
    QRegExp rx("<a\\shref=\"(\\/imgres\\?imgurl=[^\"]+)\">[\\s\\n]*<img[^>]+src=\"([^\"]+)\"");
    rx.setCaseSensitivity(Qt::CaseInsensitive);
    rx.setMinimal(true);

    int pos = 0;
    QString xml(resp);
    QString html = xml.replace(QLatin1String("&amp;"), QLatin1String("&"));

    while (-1!=(pos=rx.indexIn(html, pos))) {
        QUrl url("http://www.google.com"+rx.cap(1));
        int width=url.queryItemValue("w").toInt();
        int height=url.queryItemValue("h").toInt();
        if (width>=100 && height>=100) {
            QString largeUrl=url.queryItemValue("imgurl");
            QString thumbUrl=rx.cap(2);
            if (!thumbUrl.isEmpty() && !largeUrl.isEmpty()) {
                QNetworkReply *j=downloadImage(thumbUrl, DL_Thumbnail);
                if (j) {
                    j->setProperty(constHostProperty, constGoogleHost);
                    j->setProperty(constLargeProperty, largeUrl);
                    j->setProperty(constThumbProperty, thumbUrl);
                    j->setProperty(constWidthProperty, width);
                    j->setProperty(constHeightProperty, height);
                    j->setProperty(constSizeProperty, url.queryItemValue("sz").toInt());
                }
            }
        }
        pos += rx.matchedLength();
    }
}

//void CoverDialog::parseYahooQueryResponse(const QString &resp)
//{
//    QXmlStreamReader xml(resp);

//    while (!xml.atEnd() && !xml.hasError()) {
//        xml.readNext();
//        if (!xml.isStartElement() || "resultset_images"!=xml.name()) {
//            continue;
//        }

//        while (!xml.atEnd() && !xml.hasError()) {
//            xml.readNext();
//            if (xml.isEndElement() && "resultset_images"==xml.name()) {
//                break;
//            }
//            if (!xml.isStartElement()) {
//                continue;
//            }

//            if ("result"==xml.name()) {
//                QString thumbUrl;
//                QString largeUrl;
//                while (!xml.atEnd() && !xml.hasError()) {
//                    xml.readNext();
//                    const QStringRef &n = xml.name();
//                    if (xml.isEndElement() && "result"==n) {
//                        break;
//                    }

//                    if (!xml.isStartElement()) {
//                        continue;
//                    }

//                    if ("thumbnail_url"==n) {
//                        thumbUrl=xml.readElementText();
//                    } else if ("url"==n) {
//                        largeUrl=xml.readElementText();
//                    }
//                }

//                if (!thumbUrl.isEmpty() && !largeUrl.isEmpty()) {
//                    QNetworkReply *j=downloadImage(thumbUrl, DL_Thumbnail);
//                    if (j) {
//                        j->setProperty(constHostProperty, constYahooHost);
//                        j->setProperty(constLargeProperty, largeUrl);
//                        j->setProperty(constThumbProperty, thumbUrl);
//                    }
//                }
//            } else {
//                xml.skipCurrentElement();
//            }
//        }
//    }
//}

//void CoverDialog::parseDiscogsQueryResponse(const QString &resp)
//{
//    QXmlStreamReader xml(resp);

//    while (!xml.atEnd() && !xml.hasError()) {
//        xml.readNext();
//        if(!xml.isStartElement() || "release"!=xml.name()) {
//            continue;
//        }

//        const QString releaseId = xml.attributes().value("id").toString();
//        while(!xml.atEnd() && !xml.hasError()) {
//            xml.readNext();
//            const QStringRef &n = xml.name();
//            if(xml.isEndElement() && "release"==n) {
//                break;
//            }

//            if(!xml.isStartElement()) {
//                continue;
//            }

//            if ("images"==n) {
//                while (!xml.atEnd() && !xml.hasError()) {
//                    xml.readNext();
//                    if (xml.isEndElement() && "images"==xml.name()) {
//                        break;
//                    }

//                    if (!xml.isStartElement()) {
//                        continue;
//                    }
//                    if ("image"==xml.name()) {
//                        const QXmlStreamAttributes &attr = xml.attributes();
//                        QString thumbUrl(attr.value("uri150").toString());
//                        QString largeUrl(attr.value("uri").toString());

//                        if (!thumbUrl.isEmpty() && !largeUrl.isEmpty()) {
//                            QNetworkReply *j=downloadImage(thumbUrl, DL_Thumbnail);
//                            if (j) {
//                                j->setProperty(constHostProperty, constDiscogsHost);
//                                j->setProperty(constLargeProperty, largeUrl);
//                                j->setProperty(constThumbProperty, thumbUrl);
//                                j->setProperty(constWidthProperty, attr.value("width").toString().toInt());
//                                j->setProperty(constHeightProperty, attr.value("height").toString().toInt());
//                            }
//                        }
//                    } else {
//                        xml.skipCurrentElement();
//                    }
//                }
//            }
//            else
//                xml.skipCurrentElement();
//        }
//    }
//}

void CoverDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        QList<QListWidgetItem*> items=list->selectedItems();
        if (1==items.size()) {
            CoverItem *cover=(CoverItem *)items.at(0);
            if (CoverItem::Type_Local==cover->type()) {
                if (saveCover(cover->url(), ((LocalCover *)cover)->image())) {
                    accept();
                }
            } else if (CoverItem::Type_Existing!=cover->type()) {
                QNetworkReply *j=downloadImage(cover->url(), DL_LargeSave);
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
    QString existingBackup;

    if (existing) {
        static const QLatin1String constBakExt(".bak");
        existingBackup=existing->url()+constBakExt;
        if (!QFile::rename(existing->url(), existingBackup)) {
            MessageBox::error(this, i18n("Failed to set cover!\nCould not backup original!"));
            return false;
        }
    }

    QString destName;
    if (Settings::self()->storeCoversInMpdDir()) {
        QString coverName=MPDConnection::self()->getDetails().coverName;
        if (coverName.isEmpty()) {
            coverName=Covers::constFileName;
        }
        bool haveAbsPath=song.file.startsWith('/');
        QString dirName=song.file.endsWith('/') ? (haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+song.file
                                                : Utils::getDir((haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+song.file);
        destName=dirName+coverName+src.mid(src.length()-4);
    } else { // Save to cache dir...
        QString dir(Utils::cacheDir(Covers::constCoverDir+Covers::encodeName(song.albumArtist()), true));
        destName=dir+Covers::encodeName(song.album)+src.mid(src.length()-4);
    }

    if (QFile::copy(src, destName)) {
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
        MessageBox::error(this, i18n("Failed to set cover!\nCould not copy file to '%1'!").arg(destName));
        return false;
    }
}

void CoverDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->provides("text/uri-list")) {
        event->acceptProposedAction();
    }
}

void CoverDialog::dropEvent(QDropEvent *event)
{
    if(event->provides("text/uri-list") && Qt::CopyAction==event->dropAction()) {
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
