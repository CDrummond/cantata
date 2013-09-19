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
#include <QPushButton>
#include <QTreeWidget>
#include <QGridLayout>
#include <QBoxLayout>
#include <QHeaderView>
#include <QStringList>
#include <QFile>
#include <QXmlStreamReader>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

static int iCount=0;

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
            MessageBox::error(this, i18n("Failed to fetch podcasts"));
            return;
        }
        QJson::Parser parser;
        QVariant data = parser.parse(dev);
        if (data.isNull()) {
            MessageBox::error(this, i18n("There was a problem parsing the response from the iTunes Store"));
            return;
        }
        if (data.toMap().contains(QLatin1String("errorMessage"))) {
            MessageBox::error(this, data.toMap()[QLatin1String("errorMessage")].toString());
            return;
        }
        foreach (const QVariant& resultVariant, data.toMap()[QLatin1String("results")].toList()) {
            QVariantMap result(resultVariant.toMap());
            if (result[QLatin1String("kind")].toString() != QLatin1String("podcast")) {
                continue;
            }

            addPodcast(result[QLatin1String("trackName")].toString(), result[QLatin1String("feedUrl")].toUrl(), QString(), 0);
        }
    }
};

class OpmlBrowsePage : public PodcastPage
{
public:
    OpmlBrowsePage(QWidget *p, const QUrl &u)
        : PodcastPage(p)
        , loaded(false)
        , url(u)
    {
        QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
        layout->addWidget(tree);
        layout->setMargin(0);
    }
    virtual ~OpmlBrowsePage() { }

    void showEvent(QShowEvent *e)
    {
        if (!loaded) {
            fetch(url);
        }
        QWidget::showEvent(e);
    }

private:
    void parseResonse(QIODevice *dev)
    {
        if (!dev) {
            MessageBox::error(this, i18n("Failed to download directory listing"));
            return;
        }
        OpmlParser::Category parsed=OpmlParser::parse(dev);
        if (parsed.categories.isEmpty() && parsed.podcasts.isEmpty()) {
            MessageBox::error(this, i18n("Failed to parse directory listing"));
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
    }

    void addCategory(const OpmlParser::Category &cat, QTreeWidgetItem *p)
    {
        if (cat.categories.isEmpty() && cat.podcasts.isEmpty()) {
            return;
        }

        QTreeWidgetItem *catItem=p ? new QTreeWidgetItem(p, QStringList() << cat.name)
                                   : new QTreeWidgetItem(tree, QStringList() << cat.name);

        catItem->setIcon(0, Icons::self()->folderIcon);
        foreach (const OpmlParser::Podcast &pod, cat.podcasts) {
            addPodcast(pod, catItem);
        }
        foreach (const OpmlParser::Category &cat, cat.categories) {
            addCategory(cat, catItem);
        }
    }

    void addPodcast(const OpmlParser::Podcast &pod, QTreeWidgetItem *p)
    {
        PodcastPage::addPodcast(pod.name, pod.url, pod.description, p);
    }

private:
    bool loaded;
    QUrl url;
};

PodcastPage::PodcastPage(QWidget *p)
    : QWidget(p)
    , job(0)
{
    tree = new QTreeWidget(this);
    tree->setItemDelegate(new BasicItemDelegate(tree));
    tree->header()->setVisible(false);
    spinner=new Spinner(this);
    spinner->setWidget(tree->viewport());
    connect(tree, SIGNAL(itemSelectionChanged()), SLOT(selectionChanged()));
    tree->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void PodcastPage::fetch(const QUrl &url)
{
    cancel();
    spinner->start();
    job=NetworkAccessManager::self()->get(url);
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
}

void PodcastPage::cancel()
{
    if (spinner) {
        spinner->stop();
    }
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        job->deleteLater();
        job=0;
    }
}

void PodcastPage::addPodcast(const QString &name, const QUrl &url, const QString &description, QTreeWidgetItem *p)
{
    QTreeWidgetItem *podItem=p ? new QTreeWidgetItem(p, QStringList() << name)
                               : new QTreeWidgetItem(tree, QStringList() << name);

    podItem->setData(0, Qt::UserRole, url);
    podItem->setToolTip(0, description);
    podItem->setIcon(0, Icons::self()->audioFileIcon);
}

void PodcastPage::selectionChanged()
{
    QList<QTreeWidgetItem *> selection=tree->selectedItems();
    emit rssSelected(selection.isEmpty() ? QUrl() : selection.at(0)->data(0, Qt::UserRole).toUrl());
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

PodcastSearchPage::PodcastSearchPage(QWidget *p)
    : PodcastPage(p)
{
    search=new LineEdit(p);
    search->setPlaceholderText(i18n("Enter search term..."));
    searchButton=new QPushButton(i18n("Search"), p);
    QWidget::setTabOrder(search, searchButton);
    QWidget::setTabOrder(searchButton, tree);
    QGridLayout *layout=new QGridLayout(this);
    layout->addWidget(search, 0, 0, 1, 1);
    layout->addWidget(searchButton, 0, 1, 1, 1);
    layout->addWidget(tree, 1, 0, 1, 2);
    layout->setMargin(0);
    connect(search, SIGNAL(returnPressed()), SLOT(doSearch()));
    connect(searchButton, SIGNAL(clicked()), SLOT(doSearch()));
}

int PodcastSearchDialog::instanceCount()
{
    return iCount;
}

PodcastSearchDialog::PodcastSearchDialog(QWidget *parent)
    : Dialog(parent, "PodcastSearchDialog", QSize(800, 600))
{
    iCount++;
    setButtons(User1|Close);
    setButtonText(User1, i18n("Subscribe"));

    PageWidget *widget = new PageWidget(this);
    QList<PodcastPage *> pages;

    ITunesSearchPage *itunes=new ITunesSearchPage(this);
    Icon itunesIcon;
    itunesIcon.addFile(":itunes");
    if (itunesIcon.isNull()) {
        itunesIcon=Icon("folder");
    }
    widget->addPage(itunes, i18n("Search iTunes"), itunesIcon, i18n("Search for podcasts on iTunes"));
    pages << itunes;

    QFile file(":podcast_directories.xml");
    if (file.open(QIODevice::ReadOnly)) {
        QXmlStreamReader reader(&file);
        while (!reader.atEnd()) {
            reader.readNext();

            if (reader.isStartElement() && QLatin1String("directory")==reader.name()) {
                QString name=reader.attributes().value(QLatin1String("name")).toString();
                QString icon=reader.attributes().value(QLatin1String("icon")).toString();
                Icon icn;
                if (!icon.isEmpty()) {
                    icn.addFile(":"+icon);
                }
                if (icn.isNull()) {
                    icn=Icon("folder");
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
}

PodcastSearchDialog::~PodcastSearchDialog()
{
    iCount--;
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
