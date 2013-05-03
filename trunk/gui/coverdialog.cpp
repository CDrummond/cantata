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

#include "coverdialog.h"
#include "messagebox.h"
#include "localize.h"
#include "listview.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "mpdconnection.h"
#include "utils.h"
#include "spinner.h"
#include "icon.h"
#include "sha2/sha2.h"
#include "qjson/parser.h"
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

static QByteArray sha256(const QByteArray &data)
{
    SHA256_CTX context;
    SHA256_Init(&context);
    SHA256_Update(&context, reinterpret_cast<const u_int8_t*>(data.constData()),
                  data.length());

    QByteArray ret(SHA256_DIGEST_LENGTH, '\0');
    SHA256_Final(reinterpret_cast<u_int8_t*>(ret.data()), &context);
    return ret;
}

static QByteArray hmacSha256(const QByteArray &key, const QByteArray &data)
{
    static const int constBlockSize = 64; // bytes
    Q_ASSERT(key.length() <= constBlockSize);

    QByteArray innerPadding(constBlockSize, char(0x36));
    QByteArray outerPadding(constBlockSize, char(0x5c));

    for (int i=0 ; i<key.length() ; ++i) {
        innerPadding[i] = innerPadding[i] ^ key[i];
        outerPadding[i] = outerPadding[i] ^ key[i];
    }
    return sha256(outerPadding + sha256(innerPadding + data));
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

class CoverItem : public QListWidgetItem
{
public:
    enum Type {
        Type_Existing,
        Type_Local,
        Type_LastFm,
        Type_Google,
        Type_Discogs,
        Type_Amazon,
        Type_CoverArtArchive
    };

    CoverItem(const QString &u, const QString &tu, QListWidget *parent)
        : QListWidgetItem(parent)
        , imgUrl(u)
        , thmbUrl(tu)
        , list(parent) {
        setSizeHint(parent->gridSize());
        setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        setToolTip(u);
    }
    virtual quint32 key() const =0;
    virtual Type type() const =0;
    const QString & url() const { return imgUrl; }
    const QString & thumbUrl() const { return thmbUrl; }

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
    QString thmbUrl;
    QListWidget *list;
};

class LastFmCover : public CoverItem
{
public:
    LastFmCover(const QString &u, const QString &tu, const QImage &img, QListWidget *parent)
        : CoverItem(u, tu, parent) {
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
        : CoverItem(u, QString(), parent)
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
    GoogleCover(const QString &u, const QString &tu, const QImage &img, int w, int h, int size, QListWidget *parent)
        : CoverItem(u, tu, parent)
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

class DiscogsCover : public CoverItem
{
public:
    DiscogsCover(const QString &u, const QString &tu, const QImage &img, int w, int h, QListWidget *parent)
        : CoverItem(u, tu, parent)
        , width(w)
        , height(h) {
        setImage(img);
        setText(i18nc("Discogs\nwidth x height", "Discogs\n%1 x %2").arg(width).arg(height));
    }

    quint32 key() const { return width*height; }
    Type type()  const { return Type_Discogs; }
private:
    int width;
    int height;
};

class AmazonCover : public CoverItem
{
public:
    AmazonCover(const QString &u, const QString &tu, const QImage &img, int w, int h, QListWidget *parent)
        : CoverItem(u, tu, parent)
        , width(w)
        , height(h) {
        setImage(img);
        setText(i18nc("Amazon\nwidth x height", "Amazon\n%1 x %2").arg(width).arg(height));
    }

    quint32 key() const { return width*height; }
    Type type()  const { return Type_Amazon; }

private:
    int width;
    int height;
};

class CoverArtArchiveCover : public CoverItem
{
public:
    CoverArtArchiveCover(const QString &u, const QString &tu, const QImage &img, QListWidget *parent)
        : CoverItem(u, tu, parent)  {
        setImage(img);
        setText("coverartarchive.org");
    }

    quint32 key() const { return 0xFFFFFFFC; }
    Type type()  const { return Type_CoverArtArchive; }
};

class ExistingCover : public CoverItem
{
public:
    ExistingCover(const Covers::Image &i, QListWidget *parent)
        : CoverItem(i.fileName, QString(), parent)
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
    : Dialog(parent, "CoverDialog")
    , existing(0)
    , preview(0)
    , saving(false)
    , isArtist(false)
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
    amazonAccessKey=Settings::self()->amazonAccessKey();
    amazonSecretAccessKey=Settings::self()->amazonSecretAccessKey();
}

CoverDialog::~CoverDialog()
{
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
                            ? i18n("<p>An image already exists for this artist, and the file is not writeable.<p></p><i>%1</i></p>").arg(img.fileName)
                            : i18n("<p>A cover already exists for this album, and the file is not writeable.<p></p><i>%1</i></p>").arg(img.fileName));
        deleteLater();
        return;
    }
    if (isArtist) {
        setCaption(song.albumartist);
    } else {
        setCaption(i18nc("Album by Artist", "%1 by %2").arg(song.album).arg(song.albumArtist()));
    }
    if (!img.img.isNull()) {
        existing=new ExistingCover(isArtist ? Covers::Image(cropImage(img.img, true), img.fileName) : img, list);
        list->addItem(existing);
    }
    query->setText(isArtist ? song.albumartist : QString(song.album+" "+song.albumArtist()));
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
static const char * constRedirectsProperty="redirects";
static const char * constLastFmHost="ws.audioscrobbler.com";
static const char * constGoogleHost="images.google.com";
static const char * constDiscogsHost="api.discogs.com";
static const char * constCoverArtArchiveHost="coverartarchive.org";
static const char * constAmazonHost="ecs.amazonaws.com";
static const char * constAmazonUrl="http://ecs.amazonaws.com/onca/xml";
static const char * constAmazonAssociateTag="cantata";

static const int constMaxRedirects=5;

void CoverDialog::queryJobFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply || !currentQuery.contains(reply)) {
        return;
    }

    currentQuery.remove(reply);
    if(QNetworkReply::NoError==reply->error()) {
        QString host=reply->property(constHostProperty).toString();
        QVariant redirect = reply->header(QNetworkRequest::LocationHeader);
        if (redirect.isValid()) {
            int rd=reply->property(constRedirectsProperty).toInt();
            if (rd<constMaxRedirects) {
                sendQueryRequest(redirect.toUrl(), rd+1, host);
            }
        } else {
            QByteArray resp=reply->readAll();
            if (constLastFmHost==host) {
                parseLstFmQueryResponse(resp);
            } else if (constGoogleHost==host) {
                parseGoogleQueryResponse(resp);
            } else if (constDiscogsHost==host) {
                parseDiscogsQueryResponse(resp);
            } else if (constCoverArtArchiveHost==host) {
                parseCoverArtArchiveQueryResponse(resp);
            } else if (constAmazonHost==host) {
                parseAmazonQueryResponse(resp);
            }
        }
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
        QString host=reply->property(constHostProperty).toString();
        QVariant redirect = reply->header(QNetworkRequest::LocationHeader);
        if (redirect.isValid()) {
            int rd=reply->property(constRedirectsProperty).toInt();
            if (rd<constMaxRedirects) {
                QNetworkReply *j=downloadImage(redirect.toString(), dlType);
                if (j) {
                    j->setProperty(constRedirectsProperty, rd+1);
                    QStringList stringProps=QStringList() << constHostProperty << constLargeProperty << constThumbProperty;
                    QStringList intProps=QStringList() << constWidthProperty << constHeightProperty << constSizeProperty;
                    foreach (const QString &prop, stringProps) {
                        QVariant p=reply->property(prop.toLatin1().constData());
                        if (p.isValid()) {
                            j->setProperty(prop.toLatin1().constData(), p.toString());
                        }
                    }
                    foreach (const QString &prop, intProps) {
                        QVariant p=reply->property(prop.toLatin1().constData());
                        if (p.isValid()) {
                            j->setProperty(prop.toLatin1().constData(), p.toInt());
                        }
                    }
                }
            }
        } else {
            QString url=reply->url().toString();
            QByteArray data=reply->readAll();
            FileType fileType=url.endsWith(".jpg") || url.endsWith(".jpeg") ? FT_Jpg : (url.endsWith(".png") ? FT_Png : FT_Other);
            QImage img=QImage::fromData(data, FT_Jpg==fileType ? "JPG" : (FT_Png==fileType ? "PNG" : 0));
            if (!img.isNull()) {
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
                        int w=reply->property(constWidthProperty).toInt();
                        int h=reply->property(constHeightProperty).toInt();
                        if (canUse(w, h)) {
                            item=new GoogleCover(reply->property(constLargeProperty).toString(), url, img, w, h,
                                                 reply->property(constSizeProperty).toInt(), list);
                        }
                    } else if (constDiscogsHost==host) {
                        int w=reply->property(constWidthProperty).toInt();
                        int h=reply->property(constHeightProperty).toInt();
                        if (canUse(w, h)) {
                            item=new DiscogsCover(reply->property(constLargeProperty).toString(), url, img, w, h, list);
                        }
                    } else if (constCoverArtArchiveHost==host) {
                        item=new CoverArtArchiveCover(reply->property(constLargeProperty).toString(), url, img, list);
                    } else if (constAmazonHost==host) {
                        item=new AmazonCover(reply->property(constLargeProperty).toString(), url, img,
                                             reply->property(constWidthProperty).toInt(), reply->property(constHeightProperty).toInt(), list);
                    }
                    if (item) {
                        insertItem(item);
                    }
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


    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(list->viewport());
    }
    spinner->start();
    cancelButton->setEnabled(true);
    currentQueryString=fixedQuery;
    sendLastFmQuery(fixedQuery, page);
    sendGoogleQuery(fixedQuery, page);
    if (!isArtist) {
        sendDiscoGsQuery(fixedQuery, page);
        sendAmazonQuery(fixedQuery, page);
    }
}

void CoverDialog::sendLastFmQuery(const QString &fixedQuery, int page)
{
    QUrl url;
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif
    url.setScheme("http");
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
    query.addQueryItem("type", "release");
    query.addQueryItem("q", fixedQuery);
    query.addQueryItem("f", "json");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    sendQueryRequest(url);
}

void CoverDialog::sendAmazonQuery(const QString &fixedQuery, int page)
{
    #if QT_VERSION < 0x050000
    if (0!=page || amazonAccessKey.isEmpty()) {
        return;
    }

    // Taken from Clementine!!!
    typedef QPair<QString, QString> Arg;
    typedef QList<Arg> ArgList;
    typedef QPair<QByteArray, QByteArray> EncodedArg;
    typedef QList<EncodedArg> EncodedArgList;

    ArgList args = ArgList()
            << Arg("AWSAccessKeyId", amazonAccessKey)
            << Arg("AssociateTag", constAmazonAssociateTag)
            << Arg("Keywords", fixedQuery)
            << Arg("Operation", "ItemSearch")
            << Arg("ResponseGroup", "Images")
            << Arg("SearchIndex", "All")
            << Arg("Service", "AWSECommerceService")
            << Arg("Timestamp", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss.zzzZ"))
            << Arg("Version", "2009-11-01");
    EncodedArgList encodedArgs;
    QStringList queryItems;

    // Encode the arguments
    foreach (const Arg& arg, args) {
        EncodedArg encodedArg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
        encodedArgs << encodedArg;
        queryItems << encodedArg.first+"="+encodedArg.second;
    }

    // Sign the request
    QUrl url(constAmazonUrl);
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif

    const QByteArray dataToSign = QString("GET\n%1\n%2\n%3").arg(url.host()).arg(url.path()).arg(queryItems.join("&")).toAscii();
    const QByteArray signature(hmacSha256(amazonSecretAccessKey.toLatin1(), dataToSign));

    // Add the signature to the request
    encodedArgs << EncodedArg("Signature", QUrl::toPercentEncoding(signature.toBase64()));
    query.setEncodedQueryItems(encodedArgs);
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    sendQueryRequest(url);

    #else
    // Qt5 has no non-deprecated version of 'setEncodedQueryItems', so for the moment Amazon searches are disabled for Qt5 builds...
    Q_UNUSED(fixedQuery)
    Q_UNUSED(page)
    #endif
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

void CoverDialog::sendQueryRequest(const QUrl &url, int redirects, const QString &host)
{
    QNetworkReply *j=NetworkAccessManager::self()->get(QNetworkRequest(url));
    j->setProperty(constHostProperty, host.isEmpty() ? url.host() : host);
    j->setProperty(constTypeProperty, (int)DL_Query);
    j->setProperty(constRedirectsProperty, redirects);
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
    j->setProperty(constRedirectsProperty, 0);
    if (saving) {
        previewDialog()->downloading(url);
        connect(j, SIGNAL(downloadProgress(qint64, qint64)), preview, SLOT(progress(qint64, qint64)));
    }
    return j;
}

typedef QMap<QString, QString> SizeMap;
void CoverDialog::parseLstFmQueryResponse(const QByteArray &resp)
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

        if (!largeUrl.isEmpty() && !thumbUrl.isEmpty()) {
            QNetworkReply *j=downloadImage(thumbUrl, DL_Thumbnail);
            if (j) {
                j->setProperty(constHostProperty, constLastFmHost);
                j->setProperty(constLargeProperty, largeUrl);
                j->setProperty(constThumbProperty, thumbUrl);
            }
        }
    }

    foreach (const QString &id, musibBrainzIds) {
        QUrl coverartUrl;
        coverartUrl.setScheme("http");
        coverartUrl.setHost(constCoverArtArchiveHost);
        coverartUrl.setPath("/release/"+id);
        sendQueryRequest(coverartUrl);
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
                        if (rm.contains("uri")) {
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
                        }
//                        if (rm.contains("thumb")) {
//                            QString thumbUrl=rm["thumb"].toString();
//                            if (thumbUrl.contains("/image/R-150-")) {
//                                QString largeUrl=thumbUrl.replace("image/R-150-", "/image/R-");
//                                QNetworkReply *j=downloadImage(thumbUrl, DL_Thumbnail);
//                                if (j) {
//                                    j->setProperty(constHostProperty, constDiscogsHost);
//                                    j->setProperty(constLargeProperty, largeUrl);
//                                    j->setProperty(constThumbProperty, thumbUrl);
//                                }
//                            }
//                        }
                    }
                }
            }
        } else if (response.contains("release")) {
            QVariantMap release=response["release"].toMap();
            if (release.contains("images")) {
                QVariantList images=release["images"].toList();
                foreach (const QVariant &i, images) {
                    QVariantMap im=i.toMap();
                    if (im.contains("uri") && im.contains("uri150")) {
                        QString thumbUrl=im["uri150"].toString();
                        QString largeUrl=im["uri"].toString();
                        if (!thumbUrl.isEmpty() && !largeUrl.isEmpty()) {
                            QNetworkReply *j=downloadImage(thumbUrl, DL_Thumbnail);
                            if (j) {
                                j->setProperty(constHostProperty, constDiscogsHost);
                                j->setProperty(constLargeProperty, largeUrl);
                                j->setProperty(constThumbProperty, thumbUrl);
                                j->setProperty(constWidthProperty, im["width"].toString().toInt());
                                j->setProperty(constHeightProperty, im["height"].toString().toInt());
                            }
                        }
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
                if (!thumbUrl.isEmpty() && !largeUrl.isEmpty()) {
                    QNetworkReply *j=downloadImage(thumbUrl, DL_Thumbnail);
                    if (j) {
                        j->setProperty(constHostProperty, constCoverArtArchiveHost);
                        j->setProperty(constLargeProperty, largeUrl);
                        j->setProperty(constThumbProperty, thumbUrl);
                    }
                }
            }
        }
    }
}

struct AmazonImage
{
    AmazonImage() : w(-1), h(-1) { }
    bool isNull() { return w<0 || h<0 || url.isEmpty(); }
    QString url;
    int w;
    int h;
};

struct AmazonEntry
{
    AmazonImage small;
    AmazonImage large;
};

static AmazonImage readImage(QXmlStreamReader &doc)
{
    AmazonImage image;
    QString tag=doc.name().toString();

    while (!doc.atEnd()) {
        doc.readNext();
        if (QXmlStreamReader::StartElement==doc.tokenType()) {
            if (QLatin1String("URL")==doc.name()) {
                image.url= doc.readElementText();
            } else if (QLatin1String("Width")==doc.name()) {
                image.w=doc.readElementText().toInt();
            } else if (QLatin1String("Height")==doc.name()) {
                image.h=doc.readElementText().toInt();
            } else {
                doc.skipCurrentElement();
            }
        } else if (QXmlStreamReader::EndElement==doc.tokenType() && tag==doc.name()) {
            break;
        }
    }
    return image;
}

static AmazonEntry readItem(QXmlStreamReader &doc)
{
    AmazonEntry entry;
    QString tag=doc.name().toString();
    while (!doc.atEnd()) {
        doc.readNext();
        if (QXmlStreamReader::StartElement==doc.tokenType()) {
            if (QLatin1String("LargeImage")==doc.name()) {
                entry.large=readImage(doc);
            } else if (QLatin1String("SmallImage")==doc.name()) {
                entry.small=readImage(doc);
            } else if (entry.small.h<0 && QLatin1String("MediumImage")==doc.name()) {
                entry.small=readImage(doc);
            } else {
                doc.skipCurrentElement();
            }
        } else if (QXmlStreamReader::EndElement==doc.tokenType() && tag==doc.name()) {
            break;
        }
    }
    return entry;
}

void CoverDialog::parseAmazonQueryResponse(const QByteArray &resp)
{
    QXmlStreamReader doc(resp);

    while (!doc.atEnd()) {
      doc.readNext();
        if (QXmlStreamReader::StartElement==doc.tokenType()) {
            if (QLatin1String("Item")==doc.name()) {
                AmazonEntry entry=readItem(doc);

                if (!entry.small.isNull() && !entry.large.isNull()) {
                    QNetworkReply *j=downloadImage(entry.small.url, DL_Thumbnail);
                    if (j) {
                        j->setProperty(constHostProperty, constAmazonHost);
                        j->setProperty(constLargeProperty, entry.large.url);
                        j->setProperty(constThumbProperty, entry.small.url);
                        j->setProperty(constWidthProperty, entry.large.w);
                        j->setProperty(constHeightProperty, entry.large.h);
                    }
                }
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
    if (song.isCdda()) {
        QString dir = Utils::cacheDir(Covers::constCddaCoverDir, true);
        if (!dir.isEmpty()) {
            QString destName=dir+song.file.mid(7)+src.mid(src.length()-4);
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
        bool haveAbsPath=song.file.startsWith('/');
        mpdDir=MPDConnection::self()->getDetails().dir;
        if (haveAbsPath || !mpdDir.isEmpty()) {
            dirName=song.file.endsWith('/') ? (haveAbsPath ? QString() : mpdDir)+song.file
                                            : Utils::getDir((haveAbsPath ? QString() : mpdDir)+song.file);
        }
    }

    if (isArtist) {
        if (saveInMpd) {
            if (!mpdDir.isEmpty() && dirName.startsWith(mpdDir) && 2==dirName.mid(mpdDir.length()).split('/', QString::SkipEmptyParts).count()) {
                QDir d(dirName);
                d.cdUp();
                destName=d.absolutePath()+'/'+Covers::artistFileName(song)+src.mid(src.length()-4);
            }
        } else {
            destName=Utils::cacheDir(Covers::constCoverDir)+Covers::encodeName(song.albumartist)+src.mid(src.length()-4);
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
        MessageBox::error(this, i18n("Failed to set cover!\nCould not copy file to '%1'!").arg(destName));
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
