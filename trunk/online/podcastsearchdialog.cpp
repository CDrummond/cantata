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

#include "podcastsearchdialog.h"
#include "pagewidget.h"
#include "lineedit.h"
#include "networkaccessmanager.h"
#include "messagebox.h"
#include "opmlparser.h"
#include "icons.h"
#include "spinner.h"
#include "qjson/parser.h"
#include "basicitemdelegate.h"
#include "onlineservicesmodel.h"
#include "utils.h"
#include "action.h"
#include "textbrowser.h"
#include <QPushButton>
#include <QTreeWidget>
#include <QGridLayout>
#include <QBoxLayout>
#include <QHeaderView>
#include <QStringList>
#include <QFile>
#include <QXmlStreamReader>
#include <QCryptographicHash>
#include <QProcess>
#include <QCache>
#include <QImage>
#include <QBuffer>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

static int iCount=0;

static QCache<QUrl, QImage> imageCache(200*1024);
static int maxImageSize=-1;

enum Roles {
    IsPodcastRole = Qt::UserRole,
    UrlRole,
    ImageUrlRole,
    DescriptionRole,
    WebPageUrlRole
};

QString PodcastSearchDialog::constCacheDir=QLatin1String("podcast-directories");
QString PodcastSearchDialog::constExt=QLatin1String(".opml");

static QString generateCacheFileName(const QUrl &url, bool create)
{
    QString hash=QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Md5).toHex();
    QString dir=Utils::cacheDir(PodcastSearchDialog::constCacheDir, create);
    return dir.isEmpty() ? QString() : (dir+hash+PodcastSearchDialog::constExt);
}

class ITunesSearchPage : public PodcastSearchPage
{
public:
    ITunesSearchPage(QWidget *p) : PodcastSearchPage(p) { }

    void doSearch()
    {
        QString text=search->text().trimmed();
        if (text.isEmpty()) {
            return;
        }

        QUrl url(QLatin1String("http://ax.phobos.apple.com.edgesuite.net/WebObjects/MZStoreServices.woa/wa/wsSearch"));
        #if QT_VERSION < 0x050000
        QUrl &query=url;
        #else
        QUrlQuery query;
        #endif
        query.addQueryItem(QLatin1String("country"), QLatin1String("US"));
        query.addQueryItem(QLatin1String("media"), QLatin1String("podcast"));
        query.addQueryItem(QLatin1String("term"), text);
        #if QT_VERSION >= 0x050000
        url.setQuery(query);
        #endif
        fetch(url);
    }

    void parseResonse(QIODevice *dev)
    {
        if (!dev) {
            MessageBox::error(this, i18n("Failed to fetch podcasts from iTunes"));
            return;
        }
        QJson::Parser parser;
        QVariant data = parser.parse(dev);
        if (data.isNull()) {
            MessageBox::error(this, i18n("There was a problem parsing the response from the iTunes Store"));
            return;
        }

        foreach (const QVariant &resultVariant, data.toMap()[QLatin1String("results")].toList()) {
            QVariantMap result(resultVariant.toMap());
            if (result[QLatin1String("kind")].toString() != QLatin1String("podcast")) {
                continue;
            }

            addPodcast(result[QLatin1String("trackName")].toString(),
                       result[QLatin1String("feedUrl")].toUrl(),
                       result[QLatin1String("artworkUrl100")].toUrl(),
                       QString(),
                       result[QLatin1String("collectionViewUrl")].toString(),
                       0);
        }
    }
};

class GPodderSearchPage : public PodcastSearchPage
{
public:
    GPodderSearchPage(QWidget *p) : PodcastSearchPage(p) { }

    void doSearch()
    {
        QString text=search->text().trimmed();
        if (text.isEmpty()) {
            return;
        }

        QUrl url(QLatin1String("http://gpodder.net/search.json"));
        #if QT_VERSION < 0x050000
        QUrl &query=url;
        #else
        QUrlQuery query;
        #endif
        query.addQueryItem(QLatin1String("q"), text);
        #if QT_VERSION >= 0x050000
        url.setQuery(query);
        #endif
        fetch(url);
    }

    void parseResonse(QIODevice *dev)
    {
        if (!dev) {
            MessageBox::error(this, i18n("Failed to fetch podcasts from GPodder"));
            return;
        }
        QJson::Parser parser;
        QVariant data = parser.parse(dev);
        if (data.isNull()) {
            MessageBox::error(this, i18n("There was a problem parsing the response from GPodder"));
            return;
        }
        QVariantList list=data.toList();
        foreach (const QVariant &var, list) {
            QVariantMap map=var.toMap();
            addPodcast(map[QLatin1String("title")].toString(),
                       map[QLatin1String("url")].toUrl(),
                       map[QLatin1String("logo_url")].toUrl(),
                       map[QLatin1String("description")].toString(),
                       map[QLatin1String("website")].toString(),
                       0);
        }
    }
};

PodcastPage::PodcastPage(QWidget *p)
    : QWidget(p)
    , job(0)
    , imageJob(0)
{
    tree = new QTreeWidget(this);
    tree->setItemDelegate(new BasicItemDelegate(tree));
    tree->header()->setVisible(false);
    text=new TextBrowser(this);
    spinner=new Spinner(this);
    spinner->setWidget(tree->viewport());
    imageSpinner=new Spinner(this);
    imageSpinner->setWidget(text->viewport());
    connect(tree, SIGNAL(itemSelectionChanged()), SLOT(selectionChanged()));
    tree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    text->setOpenLinks(false);
    connect(text, SIGNAL(anchorClicked(QUrl)), SLOT(openLink(QUrl)));
    updateText();
}

void PodcastPage::fetch(const QUrl &url)
{
    cancel();
    tree->clear();
    spinner->start();
    job=NetworkAccessManager::self()->get(url);
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
}

void PodcastPage::fetchImage(const QUrl &url)
{
    cancelImage();
    imageSpinner->start();
    imageJob=NetworkAccessManager::self()->get(url);
    connect(imageJob, SIGNAL(finished()), this, SLOT(imageJobFinished()));
}

void PodcastPage::cancel()
{
    spinner->stop();
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        job->deleteLater();
        job=0;
    }
}

void PodcastPage::cancelImage()
{
    imageSpinner->stop();
    if (imageJob) {
        disconnect(imageJob, SIGNAL(finished()), this, SLOT(jobFinished()));
        imageJob->deleteLater();
        imageJob=0;
    }
}

void PodcastPage::addPodcast(const QString &name, const QUrl &url, const QUrl &image, const QString &description, const QString &webPage, QTreeWidgetItem *p)
{
    if (name.isEmpty() || url.isEmpty()) {
        return;
    }

    QTreeWidgetItem *podItem=p ? new QTreeWidgetItem(p, QStringList() << name)
                               : new QTreeWidgetItem(tree, QStringList() << name);

    podItem->setData(0, IsPodcastRole, true);
    podItem->setData(0, UrlRole, url);
    podItem->setData(0, ImageUrlRole, image);
    podItem->setData(0, DescriptionRole, description);
    podItem->setData(0, WebPageUrlRole, webPage);
    podItem->setIcon(0, Icons::self()->audioFileIcon);
}

static QString encode(const QImage &img)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return QString("<br/><img src=\"data:image/png;base64,%1\"><br/>").arg(QString(buffer.data().toBase64()));
}

void PodcastPage::updateText()
{
    QList<QTreeWidgetItem *> selection=tree->selectedItems();
    if (!selection.isEmpty()) {
        QTreeWidgetItem *item=selection.at(0);
        if (item->data(0, IsPodcastRole).toBool()) {
            QUrl url=item->data(0, UrlRole).toUrl();
            QUrl imageUrl=item->data(0, ImageUrlRole).toUrl();
            QString descr=item->data(0, DescriptionRole).toString();
            QString web=item->data(0, WebPageUrlRole).toString();
            QString str="<b>"+item->text(0)+"</b><br/>";
            if (!imageUrl.isEmpty()) {
                QImage *img=imageCache.object(imageUrl);
                if (img) {
                    str+=encode(*img);
                } else {
                    fetchImage(imageUrl);
                }
            }
            if (!descr.isEmpty()) {
                str+="<p>"+descr+"</p><br/>";
            }
            str+="<table><tr><td><b>"+i18n("RSS:")+"</b></td><td><a href=\""+url.toString()+"\">"+url.toString()+"</a></td></tr>";
            if (!web.isEmpty()) {
                str+="<tr><td><b>"+i18n("Website:")+"</b></td><td><a href=\""+web+"\">"+web+"</a></td></tr>";
            }
            str+="</table>";
            text->setHtml(str);
            return;
        }
    }
    text->setHtml("<b>"+i18n("Podcast details")+"</b><p><i>"+i18n("Select a podcast to display its details")+"</i></p>");
}

void PodcastPage::selectionChanged()
{
    cancelImage();
    updateText();
    QList<QTreeWidgetItem *> selection=tree->selectedItems();
    emit rssSelected(selection.isEmpty() ? QUrl() : selection.at(0)->data(0, UrlRole).toUrl());
}

void PodcastPage::imageJobFinished()
{
    NetworkJob *j=qobject_cast<NetworkJob *>(sender());
    if (!j) {
        return;
    }
    j->deleteLater();
    if (j!=imageJob) {
        return;
    }
    if (imageSpinner) {
        imageSpinner->stop();
    }
    QImage img=QImage::fromData(imageJob->readAll());
    if (!img.isNull()) {
        if (img.width()>maxImageSize || img.height()>maxImageSize) {
            img=img.scaled(maxImageSize, maxImageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        imageCache.insert(imageJob->url(), new QImage(img), img.byteCount());
        updateText();
    }
    imageJob=0;
}

void PodcastPage::jobFinished()
{
    NetworkJob *j=qobject_cast<NetworkJob *>(sender());
    if (!j) {
        return;
    }
    j->deleteLater();
    if (j!=job) {
        return;
    }
    if (spinner) {
        spinner->stop();
    }
    parseResonse(j->ok() ? j->actualJob() : 0);
    job=0;
}

void PodcastPage::openLink(const QUrl &url)
{
    QProcess::startDetached(QLatin1String("xdg-open"), QStringList() << url.toString());
}

PodcastSearchPage::PodcastSearchPage(QWidget *p)
    : PodcastPage(p)
{
    QBoxLayout *searchLayout=new QBoxLayout(QBoxLayout::LeftToRight);
    QBoxLayout *viewLayout=new QBoxLayout(QBoxLayout::LeftToRight);
    QBoxLayout *mainLayout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    searchLayout->setMargin(0);
    viewLayout->setMargin(0);
    mainLayout->setMargin(0);
    search=new LineEdit(p);
    search->setPlaceholderText(i18n("Enter search term..."));
    searchButton=new QPushButton(i18n("Search"), p);
    QWidget::setTabOrder(search, searchButton);
    QWidget::setTabOrder(searchButton, tree);
    searchLayout->addWidget(search);
    searchLayout->addWidget(searchButton);
    viewLayout->addWidget(tree, 1);
    viewLayout->addWidget(text, 0);
    mainLayout->addLayout(searchLayout);
    mainLayout->addLayout(viewLayout);
    connect(search, SIGNAL(returnPressed()), SLOT(doSearch()));
    connect(searchButton, SIGNAL(clicked()), SLOT(doSearch()));
}

void PodcastSearchPage::showEvent(QShowEvent *e)
{
    search->setFocus();
    QWidget::showEvent(e);
}

OpmlBrowsePage::OpmlBrowsePage(QWidget *p, const QUrl &u)
    : PodcastPage(p)
    , loaded(false)
    , url(u)
{
    QBoxLayout *mainLayout=new QBoxLayout(QBoxLayout::LeftToRight, this);
    mainLayout->setMargin(0);
    mainLayout->addWidget(tree, 1);
    mainLayout->addWidget(text, 0);
    Action *act=new Action(i18n("Reload"), this);
    tree->addAction(act);
    connect(act, SIGNAL(triggered(bool)), this, SLOT(reload()));
    tree->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void OpmlBrowsePage::showEvent(QShowEvent *e)
{
    if (!loaded) {
        QString cacheFile=generateCacheFileName(url, false);
        if (!cacheFile.isEmpty() && QFile::exists(cacheFile)) {
            QFile f(cacheFile);
            if (f.open(QIODevice::ReadOnly)) {
                parseResonse(&f);
                if (tree->topLevelItemCount()>0) {
                    Utils::touchFile(cacheFile);
                    return;
                }
            }
        }
        fetch(url);
        loaded=true;
    }
    QWidget::showEvent(e);
}

void OpmlBrowsePage::reload()
{
    QString cacheFile=generateCacheFileName(url, false);
    if (!cacheFile.isEmpty() && QFile::exists(cacheFile)) {
        QFile::remove(cacheFile);
    }
    fetch(url);
}

void OpmlBrowsePage::parseResonse(QIODevice *dev)
{
    bool isLoadingFromCache=dynamic_cast<QFile *>(dev) ? true : false;

    if (!dev) {
        if (!isLoadingFromCache) MessageBox::error(this, i18n("Failed to download directory listing"));
        return;
    }
    QByteArray data=dev->readAll();
    OpmlParser::Category parsed=OpmlParser::parse(data);
    if (parsed.categories.isEmpty() && parsed.podcasts.isEmpty()) {
        if (!isLoadingFromCache) MessageBox::error(this, i18n("Failed to parse directory listing"));
        return;
    }

    if (1==parsed.categories.count() && parsed.podcasts.isEmpty()) {
        parsed=parsed.categories.at(0);
    }
    foreach (const OpmlParser::Category &cat, parsed.categories) {
        addCategory(cat, 0);
    }
    foreach (const OpmlParser::Podcast &pod, parsed.podcasts) {
        addPodcast(pod, 0);
    }
    if (!isLoadingFromCache && tree->topLevelItemCount()>0) {
        QString cacheFile=generateCacheFileName(url, true);
        if (!cacheFile.isEmpty()) {
            QFile f(cacheFile);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(data);
            }
        }
    }
}

void OpmlBrowsePage::addCategory(const OpmlParser::Category &cat, QTreeWidgetItem *p)
{
    if (cat.categories.isEmpty() && cat.podcasts.isEmpty()) {
        return;
    }

    QTreeWidgetItem *catItem=p ? new QTreeWidgetItem(p, QStringList() << cat.name)
                               : new QTreeWidgetItem(tree, QStringList() << cat.name);

    catItem->setData(0, IsPodcastRole, false);
    catItem->setIcon(0, Icons::self()->folderIcon);
    foreach (const OpmlParser::Podcast &pod, cat.podcasts) {
        addPodcast(pod, catItem);
    }
    foreach (const OpmlParser::Category &cat, cat.categories) {
        addCategory(cat, catItem);
    }
}

void OpmlBrowsePage::addPodcast(const OpmlParser::Podcast &pod, QTreeWidgetItem *p)
{
    PodcastPage::addPodcast(pod.name, pod.url, pod.image, pod.description, pod.htmlUrl, p);
}

int PodcastSearchDialog::instanceCount()
{
    return iCount;
}

PodcastSearchDialog::PodcastSearchDialog(QWidget *parent)
    : Dialog(parent, "PodcastSearchDialog", QSize(800, 600))
{
    Utils::clearOldCache(constCacheDir, 2);

    iCount++;
    setButtons(User1|Close);
    setButtonText(User1, i18n("Subscribe"));

    PageWidget *widget = new PageWidget(this);
    QList<PodcastPage *> pages;

    ITunesSearchPage *itunes=new ITunesSearchPage(this);
    Icon itunesIcon;
    itunesIcon.addFile(":itunes");
    widget->addPage(itunes, i18n("Search iTunes"), itunesIcon, i18n("Search for podcasts on iTunes"));
    pages << itunes;

    GPodderSearchPage *gpodder=new GPodderSearchPage(this);
    Icon gpodderIcon;
    gpodderIcon.addFile(":gpodder");
    widget->addPage(gpodder, i18n("Search GPodder"), gpodderIcon, i18n("Search for podcasts on GPodder.net"));
    pages << gpodder;

    QFile file(":podcast_directories.xml");
    if (file.open(QIODevice::ReadOnly)) {
        QXmlStreamReader reader(&file);
        while (!reader.atEnd()) {
            reader.readNext();

            if (reader.isStartElement() && QLatin1String("directory")==reader.name()) {
                QString name=reader.attributes().value(QLatin1String("name")).toString();
                QString icon=reader.attributes().value(QLatin1String("icon")).toString();
                Icon icn;
                if (icon.isEmpty()) {
                    icn=Icon("folder");
                } else {
                    icn.addFile(":"+icon);
                }
                OpmlBrowsePage *page=new OpmlBrowsePage(this, QUrl(reader.attributes().value(QLatin1String("url")).toString()));
                widget->addPage(page, i18n("Browse %1", name), icn, i18n("Browse %1 podcasts", name));
                pages << page;
            }
        }
    }

    foreach (PodcastPage *p, pages) {
        connect(p, SIGNAL(rssSelected(QUrl)), SLOT(rssSelected(QUrl)));
    }

    setCaption(i18n("Search For Podcasts"));
    setMainWidget(widget);
    setAttribute(Qt::WA_DeleteOnClose);
    enableButton(User1, false);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    if (-1==maxImageSize) {
        maxImageSize=fontMetrics().height()*8;
    }
}

PodcastSearchDialog::~PodcastSearchDialog()
{
    iCount--;
    imageCache.clear();
}

void PodcastSearchDialog::rssSelected(const QUrl &url)
{
    currentUrl=url;
    enableButton(User1, currentUrl.isValid());
}

void PodcastSearchDialog::slotButtonClicked(int button)
{
    switch (button) {
    case User1:
        if (OnlineServicesModel::self()->subscribePodcast(currentUrl)) {
            MessageBox::information(this, i18n("Subscription added"));
        } else {
            MessageBox::error(this, i18n("You are already subscribed to this URL!"));
        }
        break;
    case Close:
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}
