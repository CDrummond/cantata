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
#include <QtGui/QVBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QListWidget>
#include <QtGui/QPixmap>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtCore/QDebug>
#include <QtXml/QXmlStreamReader>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

static int iCount=0;

int CoverDialog::instanceCount()
{
    return iCount;
}

class ListWidget : public QListWidget
{
public:
    ListWidget(QWidget *p)
        : QListWidget(p) {
        QFont f(font());
        f.setPointSizeF(f.pointSizeF()*0.75);
        QFontMetrics fm(f);
        setFont(f);
        setAcceptDrops(false);
        setContextMenuPolicy(Qt::CustomContextMenu);
        setDragDropMode(QAbstractItemView::NoDragDrop);
        setDragEnabled(false);
        setDropIndicatorShown(false);
        setMovement(QListView::Static);
        setGridSize(QSize(imageSize()+10, imageSize()+10+(2.25*fm.height())));
        setIconSize(QSize(imageSize(), imageSize()));
        setSpacing(4);
        setViewMode(QListView::IconMode);
        setResizeMode(QListView::Adjust);
        setMinimumSize(gridSize().width()*4, gridSize().height()*3);
    }

    int imageSize() const { return 120; }
};

class CoverItem : public QListWidgetItem
{
public:
    CoverItem(ListWidget *parent)
        : QListWidgetItem(parent)
        , list (parent) {
        setSizeHint(list->gridSize());
        setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    }
    virtual quint32 key() const =0;

    bool operator<(const CoverItem &o) const {
        return key()<o.key();
    }

protected:
    void setImage(const QImage &img) {
        setIcon(QPixmap::fromImage(img.scaled(QSize(list->imageSize(), list->imageSize()), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }

private:
    ListWidget *list;
};

class LastFmCover : public CoverItem
{
public:
    LastFmCover(const QString &u, const QImage &s, ListWidget *parent)
        : CoverItem(parent)
        , url(u)
        , small(s) {
        setImage(small);
        setText(i18n("Last.fm"));
    }

    quint32 key() const { return 33; }

private:
    QString url;
    QImage small;
    QImage large;
};

class ExistingCover : public CoverItem
{
public:
    ExistingCover(const Covers::Image &i, ListWidget *parent)
        : CoverItem(parent)
        , img(i) {
        setImage(img.img);
        QFont f(font());
        f.setBold((true));
        setFont(f);
        setText(i18n("Current Cover\n%1 x %2").arg(img.img.width()).arg(img.img.height()));
    }

    quint32 key() const { return 0xFFFFFFFF; }

private:
    Covers::Image img;
};

CoverDialog::CoverDialog(QWidget *parent)
    : Dialog(parent)
{
    iCount++;
    setAttribute(Qt::WA_DeleteOnClose);
    setButtons(Cancel|Ok);
    enableButton(Ok, false);
    QWidget *mw=new QWidget(this);
    QVBoxLayout *layout=new QVBoxLayout(mw);
    QLabel *label=new QLabel(i18n("Please select the desired cover from the list below:"), mw);
    list=new ListWidget(mw);

    layout->addWidget(label);
    layout->addWidget(list);
    setMainWidget(mw);
}

CoverDialog::~CoverDialog()
{
    iCount--;
    cancelQuery();
}

void CoverDialog::show(const Song &song)
{
    Covers::Image img=Covers::getImage(song);

    if (!img.fileName.isEmpty() && !QFileInfo(img.fileName).isWritable()) {
        MessageBox::error(parentWidget(), i18n("<p>A cover already exists for this album, and the file is not writeable.<p></p><i>%1</i></p>").arg(img.fileName));
        deleteLater();
        return;
    }
    setCaption(i18n("Cover: %1").arg(song.album));
    if (!img.img.isNull()) {
        list->addItem(new ExistingCover(img, list));
    }
    sendQuery(song.album+" "+song.albumArtist());
    Dialog::show();
}

static const char * constHostProperty="host";
static const char * constLargeProperty="large";
static const char * constThumbProperty="thumb";
static const char * constLastFmHost="ws.audioscrobbler.com";
static const char * constGoogleHost="images.google.com";
//static const char * constYahooHost="boss.yahooapis.com";
//static const char * constDiscogsHost="www.discogs.com";

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
        }
    }
    reply->deleteLater();
}

void CoverDialog::downloadJobFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply || !currentQuery.contains(reply)) {
        return;
    }

    currentQuery.remove(reply);
    if(QNetworkReply::NoError==reply->error()) {
        QString url=reply->url().toString();
        QImage img=QImage::fromData(reply->readAll(), url.endsWith(".jpg") ? "JPG" : (url.endsWith(".png") ? "PNG" : 0));
        if (!img.isNull()) {
            QString host=reply->property(constHostProperty).toString();
            bool isLarge=reply->property(constThumbProperty).toString().isEmpty();

            if (isLarge) {
            } else {
                if (constLastFmHost==host) {
                    list->addItem(new LastFmCover(reply->property(constLargeProperty).toString(), img, list));
                } else if (constGoogleHost==host) {
                }
            }
        }
    }
    reply->deleteLater();
}

void CoverDialog::sendQuery(const QString &query)
{
    QString fixedQuery(query);
    fixedQuery.remove(QChar('?'));

    // query="Album Artist" replace spaces with %20 - url encode!
    cancelQuery();
    QUrl lastFm;
    lastFm.setScheme("http");
    lastFm.setHost(constLastFmHost);
    lastFm.setPath("/2.0/");
    lastFm.addQueryItem("api_key", Covers::constLastFmApiKey);
    lastFm.addQueryItem("limit", QString::number(20));
    lastFm.addQueryItem("page", QString::number(0));
    lastFm.addQueryItem("album", fixedQuery);
    lastFm.addQueryItem("method", "album.search");

    QUrl google;
    google.setScheme("http");
    google.setHost(constGoogleHost);
    google.setPath("/images");
    google.addQueryItem("q", fixedQuery);
    google.addQueryItem("gbv", QChar('1'));
    google.addQueryItem("filter", QChar('1'));
    google.addQueryItem("start", QString::number(20 * 0));

    /*
    QUrl yahoo;
    yahoo.setScheme("http");
    yahoo.setHost(constYahooHost);
    yahoo.setPath("/ysearch/images/v1/" + fixedQuery);
    yahoo.addQueryItem("appid", constYahooApiKey);
    yahoo.addQueryItem("count", QString::number(20));
    yahoo.addQueryItem("start", QString::number(20 * 0));
    yahoo.addQueryItem("format", "xml" );

    QUrl discogs;
    discogs.setScheme("http");
    discogs.setHost(constDiscogsHost);
    discogs.setPath("/search");
    discogs.addQueryItem("api_key", constDiscogsApiKey);
    discogs.addQueryItem("page", QString::number(0 + 1));
    discogs.addQueryItem("type", "all" );
    discogs.addQueryItem("q", fixedQuery);
    discogs.addQueryItem("f", "xml" );
    */

    sendQueryRequest(lastFm);
    //sendQueryRequest(google);
    //sendQueryRequest(yahoo);
    //sendQueryRequest(discogs);
}

void CoverDialog::cancelQuery()
{
    if (!currentQuery.isEmpty()) {
    }
}

void CoverDialog::sendQueryRequest(const QUrl &url)
{
    QNetworkReply *j=NetworkAccessManager::self()->get(QNetworkRequest(url));
    j->setProperty(constHostProperty, url.host());
    connect(j, SIGNAL(finished()), this, SLOT(queryJobFinished()));
    currentQuery.insert(j);
}

QNetworkReply * CoverDialog::downloadImage(const QString &url)
{
    QNetworkReply *j=NetworkAccessManager::self()->get(QNetworkRequest(QUrl(url)));
    connect(j, SIGNAL(finished()), this, SLOT(downloadJobFinished()));
    currentQuery.insert(j);
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
            QNetworkReply *j=downloadImage(thumbUrl);
            j->setProperty(constHostProperty, constLastFmHost);
            j->setProperty(constLargeProperty, largeUrl);
            j->setProperty(constThumbProperty, thumbUrl);
        }
    }
}
