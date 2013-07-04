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

#include "searchdialog.h"
#include "localize.h"
#include "listview.h"
#include "networkaccessmanager.h"
#include "spinner.h"
#include "streamsmodel.h"
#include "streamspage.h"
#include "icons.h"
#include "treeview.h"
#include <QXmlStreamReader>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QBuffer>
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

static int iCount=0;

static QString constRadioTimeSearchUrl=QLatin1String("http://opml.radiotime.com/Search.ashx");

int SearchDialog::instanceCount()
{
    return iCount;
}

enum Roles {
    Role_Url = Qt::UserRole,
    Role_ImageUrl
};

SearchDialog::SearchDialog(StreamsPage *parent)
    : Dialog(parent, "SearchDialog")
    , spinner(0)
    , job(0)
{
    iCount++;
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    setAttribute(Qt::WA_DeleteOnClose);
    setButtons(User1|User2|Close);
    setButtonText(User1, i18n("Add to Favourites"));
    setButtonText(User2, i18n("Play"));
    enableButton(Ok, false);
    connect(list, SIGNAL(itemSelectionChanged()), SLOT(checkStatus()));
    connect(search, SIGNAL(clicked()), SLOT(performSearch()));
    connect(query, SIGNAL(returnPressed()), SLOT(performSearch()));
    connect(this, SIGNAL(add(QStringList,bool,quint8)), parent, SIGNAL(add(QStringList,bool,quint8)));
    list->setResizeMode(QListView::Adjust);
    list->setSortingEnabled(true);
    enableButton(User1, false);
    enableButton(User2, false);
    setWindowTitle(i18n("Search for Radio Stations via TuneIn"));

    int h=fontMetrics().height();
    if (!configuredSize().isValid()) {
        adjustSize();
        resize(qMax(width(), h*40), qMax(height(), h*20));
    }
    int iconSize=h>22 ? 96 : 48;
    list->setIconSize(QSize(iconSize, iconSize));
    list->setItemDelegate(new SimpleTreeViewDelegate(list));
}

SearchDialog::~SearchDialog()
{
    iCount--;
    cancelSearch();
}

void SearchDialog::performSearch()
{
    QString q=query->text().trimmed();

    if (q.isEmpty()) {
        return;
    }
    list->clear();
    checkStatus();
    cancelSearch();
    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(list->viewport());
    }
    spinner->start();
    
    QUrl url(constRadioTimeSearchUrl);
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif

    query.addQueryItem("types", "station");
    query.addQueryItem("query", q);
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    job=NetworkAccessManager::self()->get(QNetworkRequest(url));
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
}

void SearchDialog::jobFinished()
{
    QNetworkReply *reply=dynamic_cast<QNetworkReply *>(sender());

    if (!reply || reply!=job) {
        return;
    }
    cancelSearch();

    QXmlStreamReader doc(reply);
    while (!doc.atEnd()) {
        doc.readNext();
        if (doc.isStartElement() && QLatin1String("outline")==doc.name()) {
            while (!doc.atEnd()) {
                if (doc.isStartElement()) {
                    QString text=doc.attributes().value("text").toString();
                    if (!text.isEmpty()) {
                        QString url=doc.attributes().value("URL").toString();
                        if (QLatin1String("audio")==doc.attributes().value("type").toString()) {
                            QListWidgetItem *item=new QListWidgetItem(text, list);
                            item->setData(Role_Url, url);
                            QString image=doc.attributes().value("image").toString();
                            if (!image.isEmpty()) {
                                itemUrlMap.insert(image, item);
                                item->setData(Role_ImageUrl, image);
                                QNetworkReply *imageJob=NetworkAccessManager::self()->get(QNetworkRequest(image));
                                imageRequests.append(imageJob);
                                connect(imageJob, SIGNAL(finished()), this, SLOT(imageJobFinished()));
                            }
                            item->setIcon(Icons::self()->radioStreamIcon);
                        }
                    }
                }
                doc.readNext();
                if (doc.isEndElement() && QLatin1String("outline")==doc.name()) {
                    break;
                }
            }
        }
    }
}

//static QString encode(const QImage &img)
//{
//    QByteArray bytes;
//    QBuffer buffer(&bytes);
//    buffer.open(QIODevice::WriteOnly);
//    QImage copy(img);

//    if (copy.size().width()>Covers::constMaxSize.width() || copy.size().height()>Covers::constMaxSize.height()) {
//        copy=copy.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
//    }
//    copy.save(&buffer, "PNG");
//    return QString("<img src=\"data:image/png;base64,%1\">").arg(QString(buffer.data().toBase64()));
//}

void SearchDialog::imageJobFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply || !imageRequests.contains(reply)) {
        return;
    }
    reply->deleteLater();
    imageRequests.removeAll(reply);
    if(QNetworkReply::NoError==reply->error()) {
        QListWidgetItem *item=itemUrlMap[reply->url().toString()];
        if (item) {
            QImage img=QImage::fromData(reply->readAll());
            if (!img.isNull()) {
                QImage scaled=img.scaled(list->iconSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                if (scaled.size()!=list->iconSize()) {
                    QImage fixed(list->iconSize(), QImage::Format_ARGB32);
                    fixed.fill(Qt::transparent);
                    QPainter p(&fixed);
                    p.drawImage((fixed.width()-scaled.width())/2, (fixed.height()-scaled.height())/2, scaled);
                    p.end();
                    scaled=fixed;
                }
                item->setIcon(QIcon(QPixmap::fromImage(scaled)));
//                if (img.width()>(iconSize*6) || img.height()>(iconSize*6)) {
//                    img=img.scaled(QSize(iconSize*6, iconSize*6), Qt::KeepAspectRatio, Qt::SmoothTransformation);
//                }
//                item->setToolTip("<b>"+item->text()+"</b><br>"+encode(img));
            }
        }
    }
}

void SearchDialog::checkStatus()
{
    QList<QListWidgetItem *> items=list->selectedItems();

    enableButton(User1, items.count() && StreamsModel::self()->isFavoritesWritable());
    enableButton(User2, 1==items.count());
}

void SearchDialog::cancelSearch()
{
    if (spinner) {
        spinner->stop();
    }

    if (job) {
        job->deleteLater();
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        job=0;
    }

    foreach (QNetworkReply *j, imageRequests) {
        j->deleteLater();
        disconnect(job, SIGNAL(finished()), this, SLOT(imageJobFinished()));
    }
    imageRequests.clear();
    itemUrlMap.clear();
}

void SearchDialog::slotButtonClicked(int button)
{
    switch (button) {
    case User1: {
        QList<QListWidgetItem *> items=list->selectedItems();
        foreach (const QListWidgetItem *item, items) {
            StreamsModel::self()->addToFavourites(item->data(Role_Url).toString(), item->text());
        }
        break;
    }
    case User2: {
        QList<QListWidgetItem *> items=list->selectedItems();
        if (1==items.count()) {
            QListWidgetItem *item=items.first();
            emit add(QStringList() << StreamsModel::modifyUrl(item->data(Role_Url).toString(), true, item->text()), true, 0);
        }
        break;
    }
    case Close:
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}
