/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/pagewidget.h"
#include "support/lineedit.h"
#include "network/networkaccessmanager.h"
#include "opmlparser.h"
#include "widgets/icons.h"
#include "support/spinner.h"
#include "widgets/basicitemdelegate.h"
#include "podcastservice.h"
#include "support/utils.h"
#include "support/action.h"
#include "support/monoicon.h"
#include "widgets/textbrowser.h"
#include "support/messagewidget.h"
#include "support/flattoolbutton.h"
#include "gui/covers.h"
#include "rssparser.h"
#include "podcastservice.h"
#include "config.h"
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QGridLayout>
#include <QBoxLayout>
#include <QHeaderView>
#include <QStringList>
#include <QFile>
#include <QFileDialog>
#include <QXmlStreamReader>
#include <QCryptographicHash>
#include <QCache>
#include <QImage>
#include <QBuffer>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>

static int iCount=0;

static QCache<QUrl, QImage> imageCache(200*1024);
static int maxImageSize=-1;
static const char * constOrigUrlProperty="orig-url";

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
    ITunesSearchPage(QWidget *p)
        : PodcastSearchPage(p,
                            QLatin1String("iTunes"),
                            FontAwesome::apple,
                            QUrl(QLatin1String("http://ax.phobos.apple.com.edgesuite.net/WebObjects/MZStoreServices.woa/wa/wsSearch")),
                            QLatin1String("term"),
                            QStringList() << QLatin1String("country") << QLatin1String("US") << QLatin1String("media") << QLatin1String("podcast"))
    {
    }

    void parse(const QVariant &data) override
    {
        for (const QVariant &resultVariant: data.toMap()[QLatin1String("results")].toList()) {
            QVariantMap result(resultVariant.toMap());
            if (result[QLatin1String("kind")].toString() != QLatin1String("podcast")) {
                continue;
            }

            addPodcast(result[QLatin1String("trackName")].toString(),
                       result[QLatin1String("feedUrl")].toUrl(),
                       result[QLatin1String("artworkUrl100")].toUrl(),
                       QString(),
                       result[QLatin1String("collectionViewUrl")].toString(),
                       nullptr);
        }
    }
};

class GPodderSearchPage : public PodcastSearchPage
{
public:
    GPodderSearchPage(QWidget *p)
        : PodcastSearchPage(p,
                            QLatin1String("GPodder"),
                            FontAwesome::podcast,
                            QUrl(QLatin1String("http://gpodder.net/search.json")),
                            QLatin1String("q"))
    {
    }

    void parse(const QVariant &data) override
    {
        QVariantList list=data.toList();
        for (const QVariant &var: list) {
            QVariantMap map=var.toMap();
            addPodcast(map[QLatin1String("title")].toString(),
                       map[QLatin1String("url")].toUrl(),
                       map[QLatin1String("logo_url")].toUrl(),
                       map[QLatin1String("description")].toString(),
                       map[QLatin1String("website")].toString(),
                       nullptr);
        }
    }
};

PodcastPage::PodcastPage(QWidget *p, const QString &n)
    : QWidget(p)
    , pageName(n)
    , job(nullptr)
    , imageJob(nullptr)
{
    tree = new QTreeWidget(this);
    tree->setItemDelegate(new BasicItemDelegate(tree));
    tree->header()->setVisible(false);
    text=new TextBrowser(this);
    spinner=new Spinner(this);
    spinner->setWidget(tree);
    imageSpinner=new Spinner(this);
    imageSpinner->setWidget(text);
    connect(tree, SIGNAL(itemSelectionChanged()), SLOT(selectionChanged()));
    tree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    text->setOpenLinks(false);
    connect(text, SIGNAL(anchorClicked(QUrl)), SLOT(openLink(QUrl)));
    updateText();
}

QUrl PodcastPage::currentRss() const
{
    QList<QTreeWidgetItem *> selection=tree->selectedItems();
    return selection.isEmpty() ? QUrl() : selection.at(0)->data(0, UrlRole).toUrl();
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
    imageJob=NetworkAccessManager::self()->get(url, 5000);
    imageJob->setProperty(constOrigUrlProperty, url);
    connect(imageJob, SIGNAL(finished()), this, SLOT(imageJobFinished()));
}

void PodcastPage::cancel()
{
    spinner->stop();
    if (job) {
        job->cancelAndDelete();
        job=nullptr;
    }
}

void PodcastPage::cancelImage()
{
    imageSpinner->stop();
    if (imageJob) {
        imageJob->cancelAndDelete();
        imageJob=nullptr;
    }
}

void PodcastPage::addPodcast(const QString &title, const QUrl &url, const QUrl &image, const QString &description, const QString &webPage, QTreeWidgetItem *p)
{
    if (title.isEmpty() || url.isEmpty()) {
        return;
    }

    QTreeWidgetItem *podItem=p ? new QTreeWidgetItem(p, QStringList() << title)
                               : new QTreeWidgetItem(tree, QStringList() << title);

    podItem->setData(0, IsPodcastRole, true);
    podItem->setData(0, UrlRole, url);
    podItem->setData(0, ImageUrlRole, image);
    podItem->setData(0, DescriptionRole, description);
    podItem->setData(0, WebPageUrlRole, webPage);
    podItem->setIcon(0, Icons::self()->audioListIcon);
}

void PodcastPage::addCategory(const OpmlParser::Category &cat, QTreeWidgetItem *p)
{
    if (cat.categories.isEmpty() && cat.podcasts.isEmpty()) {
        return;
    }

    QTreeWidgetItem *catItem=p ? new QTreeWidgetItem(p, QStringList() << cat.name)
                               : new QTreeWidgetItem(tree, QStringList() << cat.name);

    catItem->setData(0, IsPodcastRole, false);
    catItem->setIcon(0, Icons::self()->folderListIcon);
    for (const OpmlParser::Podcast &pod: cat.podcasts) {
        addPodcast(pod, catItem);
    }
    for (const OpmlParser::Category &subCat: cat.categories) {
        addCategory(subCat, catItem);
    }
}

void PodcastPage::addPodcast(const OpmlParser::Podcast &pod, QTreeWidgetItem *p)
{
    addPodcast(pod.name, pod.url, pod.image, pod.description, pod.htmlUrl, p);
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
            str+="<table><tr><td><b>"+tr("RSS:")+"</b></td><td><a href=\""+url.toString()+"\">"+url.toString()+"</a></td></tr>";
            if (!web.isEmpty()) {
                str+="<tr><td><b>"+tr("Website:")+"</b></td><td><a href=\""+web+"\">"+web+"</a></td></tr>";
            }
            str+="</table>";
            text->setHtml(str);
            return;
        }
    }
    text->setHtml("<b>"+tr("Podcast details")+"</b><p><i>"+tr("Select a podcast to display its details")+"</i></p>");
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
    QByteArray data=imageJob->readAll();
    QImage img=QImage::fromData(data, Covers::imageFormat(data));
    if (!img.isNull()) {
        if (img.width()>maxImageSize || img.height()>maxImageSize) {
            img=img.scaled(maxImageSize, maxImageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        imageCache.insert(imageJob->property(constOrigUrlProperty).toUrl(), new QImage(img), img.byteCount());
        updateText();
    }
    imageJob=nullptr;
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
    parseResonse(j->ok() ? j->actualJob() : nullptr);
    job=nullptr;
}

void PodcastPage::openLink(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

PodcastSearchPage::PodcastSearchPage(QWidget *p, const QString &n, int i, const QUrl &qu, const QString &qk, const QStringList &other)
    : PodcastPage(p, n)
    , queryUrl(qu)
    , queryKey(qk)
    , otherArgs(other)
{
    QBoxLayout *searchLayout=new QBoxLayout(QBoxLayout::LeftToRight);
    QBoxLayout *viewLayout=new QBoxLayout(QBoxLayout::LeftToRight);
    QBoxLayout *mainLayout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    searchLayout->setMargin(0);
    viewLayout->setMargin(0);
    mainLayout->setMargin(0);
    search=new LineEdit(p);
    search->setPlaceholderText(tr("Enter search term..."));
    searchButton=new QPushButton(tr("Search"), p);
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
    icn=MonoIcon::icon((FontAwesome::icon)i, Utils::monoIconColor());
}

void PodcastSearchPage::showEvent(QShowEvent *e)
{
    search->setFocus();
    QWidget::showEvent(e);
}

void PodcastSearchPage::doSearch()
{
    QString text=search->text().trimmed();
    if (text.isEmpty() || text==currentSearch) {
        return;
    }

    currentSearch=text;
    QUrl url=queryUrl;
    QUrlQuery query;
    query.addQueryItem(queryKey, text);
    if (otherArgs.size()>1) {
        for (int i=0; i<otherArgs.size()-1; i+=2) {
            query.addQueryItem(otherArgs.at(i), otherArgs.at(i+1));
        }
    }
    url.setQuery(query);
    fetch(url);
}

void PodcastSearchPage::parseResonse(QIODevice *dev)
{
    if (!dev) {
        emit error(tr("Failed to fetch podcasts from %1").arg(name()));
        return;
    }
    QVariant data=QJsonDocument::fromJson(dev->readAll()).toVariant();
    if (data.isNull()) {
        emit error(tr("There was a problem parsing the response from %1").arg(name()));
        return;
    }
    parse(data);
}

OpmlBrowsePage::OpmlBrowsePage(QWidget *p, const QString &n, const QIcon &i, const QUrl &u)
    : PodcastPage(p, n)
    , loaded(false)
    , url(u)
{
    QBoxLayout *mainLayout=new QBoxLayout(QBoxLayout::LeftToRight, this);
    mainLayout->setMargin(0);
    mainLayout->addWidget(tree, 1);
    mainLayout->addWidget(text, 0);
    Action *act=new Action(tr("Reload"), this);
    tree->addAction(act);
    connect(act, SIGNAL(triggered()), this, SLOT(reload()));
    tree->setContextMenuPolicy(Qt::ActionsContextMenu);
    icn=i;
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
                    loaded=true;
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
        if (!isLoadingFromCache) {
            emit error(tr("Failed to download directory listing"));
        }
        return;
    }
    QByteArray data=dev->readAll();
    OpmlParser::Category parsed=OpmlParser::parse(data);
    if (parsed.categories.isEmpty() && parsed.podcasts.isEmpty()) {
        if (!isLoadingFromCache) {
            emit error(tr("Failed to parse directory listing"));
        }
        return;
    }

    if (1==parsed.categories.count() && parsed.podcasts.isEmpty()) {
        parsed=parsed.categories.at(0);
    }
    for (const OpmlParser::Category &cat: parsed.categories) {
        addCategory(cat, nullptr);
    }
    for (const OpmlParser::Podcast &pod: parsed.podcasts) {
        addPodcast(pod, nullptr);
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

PodcastUrlPage::PodcastUrlPage(QWidget *p)
    : PodcastPage(p, tr("URL"))
{
    QBoxLayout *searchLayout=new QBoxLayout(QBoxLayout::LeftToRight);
    QBoxLayout *viewLayout=new QBoxLayout(QBoxLayout::LeftToRight);
    QBoxLayout *mainLayout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    searchLayout->setMargin(0);
    viewLayout->setMargin(0);
    mainLayout->setMargin(0);
    urlEntry=new LineEdit(p);
    urlEntry->setPlaceholderText(tr("Enter podcast URL..."));
    QPushButton *loadButton=new QPushButton(tr("Load"), p);
    FlatToolButton *pathReq=new FlatToolButton(p);
    pathReq->setIcon(MonoIcon::icon(FontAwesome::foldero, Utils::monoIconColor()));
    pathReq->setToolTip(tr("Load local podcast file"));
    QWidget::setTabOrder(urlEntry, loadButton);
    QWidget::setTabOrder(loadButton, tree);
    searchLayout->addWidget(urlEntry);
    searchLayout->addWidget(loadButton);
    searchLayout->addWidget(pathReq);
    viewLayout->addWidget(tree, 1);
    viewLayout->addWidget(text, 0);
    mainLayout->addWidget(new QLabel(tr("Enter podcast URL below, and press 'Load', or press the folder icon to load a local podcast file."), this));
    mainLayout->addLayout(searchLayout);
    mainLayout->addLayout(viewLayout);
    connect(urlEntry, SIGNAL(returnPressed()), SLOT(loadUrl()));
    connect(loadButton, SIGNAL(clicked()), SLOT(loadUrl()));
    connect(pathReq, SIGNAL(clicked()), SLOT(openPath()));
    icn=Icons::self()->rssListIcon;
}

void PodcastUrlPage::showEvent(QShowEvent *e)
{
    urlEntry->setFocus();
    QWidget::showEvent(e);
}

void PodcastUrlPage::loadUrl()
{
    QString text=urlEntry->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    QUrl url(PodcastService::fixUrl(text));
    if (url==currentUrl) {
        return;
    }

    if (!PodcastService::isUrlOk(url)) {
        emit error(tr("Invalid URL!"));
    } else {
        currentUrl=url;
        fetch(url);
    }
}

void PodcastUrlPage::openPath()
{
    QString path = QFileDialog::getOpenFileName(this, QString(), QDir::homePath(), tr("Podcasts (*.xml, *.rss, *.opml"));

    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            parse(&f);
        } else {
            emit error(tr("Failed to read file!"));
        }
    }
}
void PodcastUrlPage::parseResonse(QIODevice *dev)
{
    if (!dev) {
        emit error(tr("Failed to fetch podcast!"));
        return;
    }
    parse(dev);
}

void PodcastUrlPage::parse(QIODevice *dev)
{
    auto data = dev->readAll();
    QBuffer buf(&data);
    buf.open(QIODevice::ReadOnly|QIODevice::Text);
    RssParser::Channel ch=RssParser::parse(&buf, false, true);
    if (ch.isValid()) {
        if (ch.video) {
            emit error(tr("Cantata only supports audio podcasts! The URL entered contains only video podcasts."));
        } else {
            addPodcast(ch.name, currentUrl, ch.image, ch.description, QString(), nullptr);
        }
    } else {
        OpmlParser::Category parsed=OpmlParser::parse(data);
        if (parsed.categories.isEmpty() && parsed.podcasts.isEmpty()) {
            emit error(tr("Failed to parse podcast."));
        } else {
            if (1==parsed.categories.count() && parsed.podcasts.isEmpty()) {
                parsed=parsed.categories.at(0);
            }
            for (const OpmlParser::Category &cat: parsed.categories) {
                addCategory(cat, nullptr);
            }
            for (const OpmlParser::Podcast &pod: parsed.podcasts) {
                addPodcast(pod, nullptr);
            }
        }
    }
}

int PodcastSearchDialog::instanceCount()
{
    return iCount;
}

PodcastSearchDialog::PodcastSearchDialog(PodcastService *s, QWidget *parent)
    : Dialog(parent, "PodcastSearchDialog", QSize(800, 600))
    , service(s)
{
    Utils::clearOldCache(constCacheDir, 2);

    iCount++;
    setButtons(User1|Close);
    setButtonText(User1, tr("Subscribe"));

    QWidget *mainWidget = new QWidget(this);
    messageWidget = new MessageWidget(mainWidget);
    spacer = new QWidget(this);
    spacer->setFixedSize(Utils::layoutSpacing(this), 0);
    QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom, mainWidget);
    layout->setMargin(0);
    layout->setSpacing(0);

    pageWidget = new PageWidget(mainWidget);
    QList<PodcastPage *> pages;

    layout->addWidget(messageWidget);
    layout->addWidget(spacer);
    layout->addWidget(pageWidget);

    PodcastUrlPage *urlPage=new PodcastUrlPage(pageWidget);
    pageWidget->addPage(urlPage, tr("Enter URL"), urlPage->icon(), tr("Manual podcast URL"));
    pages << urlPage;

    ITunesSearchPage *itunes=new ITunesSearchPage(pageWidget);
    pageWidget->addPage(itunes, tr("Search %1").arg(itunes->name()), itunes->icon(), tr("Search for podcasts on %1").arg(itunes->name()));
    pages << itunes;

    GPodderSearchPage *gpodder=new GPodderSearchPage(pageWidget);
    pageWidget->addPage(gpodder, tr("Search %1").arg(gpodder->name()), gpodder->icon(), tr("Search for podcasts on %1").arg(gpodder->name()));
    pages << gpodder;

    QSet<QString> loaded;
    pages << loadDirectories(Utils::dataDir(), false, loaded);
    pages << loadDirectories(QString(), true, loaded);

    for (PodcastPage *p: pages) {
        connect(p, SIGNAL(rssSelected(QUrl)), SLOT(rssSelected(QUrl)));
        connect(p, SIGNAL(error(QString)), SLOT(showError(QString)));
    }

    setCaption(tr("Add Podcast Subscription"));
    setMainWidget(mainWidget);
    setAttribute(Qt::WA_DeleteOnClose);
    enableButton(User1, false);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    if (-1==maxImageSize) {
        maxImageSize=fontMetrics().height()*8;
    }
    connect(service, SIGNAL(newError(QString)), this, SLOT(showError(QString)));
    connect(messageWidget, SIGNAL(visible(bool)), SLOT(msgWidgetVisible(bool)));
    connect(pageWidget, SIGNAL(currentPageChanged()), this, SLOT(pageChanged()));
    messageWidget->hide();
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

void PodcastSearchDialog::showError(const QString &msg)
{
    messageWidget->setError(msg);
}

void PodcastSearchDialog::showInfo(const QString &msg)
{
    messageWidget->setInformation(msg);
}

void PodcastSearchDialog::msgWidgetVisible(bool v)
{
    spacer->setFixedSize(spacer->width(), v ? spacer->width() : 0);
}

void PodcastSearchDialog::pageChanged()
{
    PageWidgetItem *pwi=pageWidget->currentPage();
    PodcastPage *page=pwi ? qobject_cast<PodcastPage *>(pwi->widget()) : nullptr;
    rssSelected(page ? page->currentRss() : QUrl());
}

QList<PodcastPage *> PodcastSearchDialog::loadDirectories(const QString &dir, bool isSystem, QSet<QString> &loaded)
{
    QList<PodcastPage *> pages;

    if (dir.isEmpty() && !isSystem) {
        return pages;
    }

    QFile file(isSystem ? QLatin1String(":podcast_directories.xml") : (dir+QLatin1String("/podcast_directories.xml")));

    if (file.open(QIODevice::ReadOnly)) {
        QXmlStreamReader reader(&file);
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isStartElement() && QLatin1String("directory")==reader.name()) {
                QString url=reader.attributes().value(QLatin1String("url")).toString();
                if (!loaded.contains(url)) {
                    QString iconName=reader.attributes().value(QLatin1String("icon")).toString();
                    QIcon icon;

                    if (iconName.isEmpty()) {
                        icon=MonoIcon::icon(FontAwesome::rsssquare, Utils::monoIconColor());
                    } else if (iconName.startsWith(":")) {
                        icon= MonoIcon::icon(iconName, Utils::monoIconColor());
                    } else {
                        icon.addFile(iconName);
                    }

                    if (icon.isNull()) {
                        icon=MonoIcon::icon(FontAwesome::rsssquare, Utils::monoIconColor());
                    }

                    OpmlBrowsePage *page=new OpmlBrowsePage(pageWidget,
                                                            reader.attributes().value(QLatin1String("name")).toString(),
                                                            icon,
                                                            QUrl(url));
                    pageWidget->addPage(page, tr("Browse %1").arg(page->name()), page->icon(), tr("Browse %1 podcasts").arg(page->name()));
                    pages << page;
                    loaded.insert(url);
                }
            }
        }
    }
    return pages;
}

void PodcastSearchDialog::slotButtonClicked(int button)
{
    switch (button) {
    case User1: {
        QUrl fixed=PodcastService::fixUrl(currentUrl);
        if (service->subscribedToUrl(fixed)) {
            showError(tr("You are already subscribed to this podcast!"));
        } else {
            service->addUrl(fixed);
            showInfo(tr("Subscription added"));
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

#include "moc_podcastsearchdialog.cpp"
