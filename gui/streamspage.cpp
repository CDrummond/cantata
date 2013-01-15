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

#include "streamspage.h"
#include "streamdialog.h"
#include "streamcategorydialog.h"
#include "mpdconnection.h"
#include "messagebox.h"
#include "localize.h"
#include "icons.h"
#include "mainwindow.h"
#include "action.h"
#include "actioncollection.h"
#include "networkaccessmanager.h"
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KFileDialog>
#else
#include <QtGui/QFileDialog>
#include <QtCore/QDir>
#endif
#include <QtGui/QAction>
#include <QtGui/QMenu>
#include <QtXml/QXmlStreamReader>

static QString webStreamName(StreamsPage::WebStream type)
{
    switch (type) {
    default:
    case StreamsPage::WS_IceCast:
        return i18n("IceCast");
    case StreamsPage::WS_SomaFm:
        return i18n("SomaFM");
    }
}

static QUrl webStreamUrl(StreamsPage::WebStream type)
{
    switch (type) {
    default:
    case StreamsPage::WS_IceCast:
        return QUrl("http://dir.xiph.org/yp.xml");
    case StreamsPage::WS_SomaFm:
        return QUrl("http://somafm.com/channels.xml");
    }
}

static QString fixGenre(const QString &g)
{
    if (g.length()) {
        QString genre=g.toLower();
        genre[0]=genre[0].toUpper();
        int pos=genre.indexOf('|');

        if (pos>0) {
            genre=genre.left(pos);
        }

        genre=genre.trimmed();
        if (genre.length() < 3 ||
            QLatin1String("The")==genre || QLatin1String("All")==genre ||
            QLatin1String("Various")==genre || QLatin1String("Unknown")==genre ||
            QLatin1String("Misc")==genre || QLatin1String("Mix")==genre || QLatin1String("100%")==genre ||
            genre.contains("ÃÂ") || // Broken unicode.
            genre.contains(QRegExp("^#x[0-9a-f][0-9a-f]"))) { // Broken XML entities.
                return QString();
        }

        if (genre==QLatin1String("R b") || genre==QLatin1String("R b")|| genre==QLatin1String("Rnb")) {
            return QLatin1String("R&B");
        }
        if (genre==QLatin1String("Classic") || genre==QLatin1String("Classical")) {
            return QLatin1String("Classical");
        }
        if (genre==QLatin1String("Christian") || genre.startsWith(QLatin1String("Christian "))) {
            return QLatin1String("Christian");
        }
        if (genre==QLatin1String("Rock") || genre.startsWith(QLatin1String("Rock "))) {
            return QLatin1String("Rock");
        }
        if (genre==QLatin1String("Electronic") || genre==QLatin1String("Electronica") || genre==QLatin1String("Electric")) {
            return QLatin1String("Electronic");
        }
        if (genre==QLatin1String("Easy") || genre==QLatin1String("Easy listening")) {
            return QLatin1String("Easy listening");
        }
        if (genre==QLatin1String("Hit") || genre==QLatin1String("Hits") || genre==QLatin1String("Easy listening")) {
            return QLatin1String("Hits");
        }
        if (genre==QLatin1String("Hip") || genre==QLatin1String("Hiphop") || genre==QLatin1String("Hip hop") || genre==QLatin1String("Hop hip")) {
            return QLatin1String("Hip hop");
        }
        if (genre==QLatin1String("News") || genre==QLatin1String("News talk")) {
            return QLatin1String("News");
        }
        if (genre==QLatin1String("Top40") || genre==QLatin1String("Top 40") || genre==QLatin1String("40top") || genre==QLatin1String("40 top")) {
            return QLatin1String("Top 40");
        }

        // Convert XX's to XXs
        if (genre.contains(QRegExp("^[0-9]0's$"))) {
            genre=genre.remove('\'');
        }
        if (genre.length()>25 && (0==genre.indexOf(QRegExp("^[0-9]0s ")) || 0==genre.indexOf(QRegExp("^[0-9]0 ")))) {
            int pos=genre.indexOf(' ');
            if (pos>1) {
                genre=genre.left(pos);
            }
        }
        // Convert 80 -> 80s.
        return genre.contains(QRegExp("^[0-9]0$")) ? genre + 's' : genre;
    }
    return g;
}

static void trimGenres(QMultiHash<QString, StreamsModel::StreamItem *> &genres)
{
    QSet<QString> genreSet = genres.keys().toSet();
    foreach (const QString &genre, genreSet) {
        if (genres.count(genre) < 3) {
            const QList<StreamsModel::StreamItem *> &smallGnre = genres.values(genre);
            foreach (StreamsModel::StreamItem* s, smallGnre) {
                s->genre = QString();
            }
        }
    }
}

static QList<StreamsModel::StreamItem *> parseIceCast(const QByteArray &data)
{
    QList<StreamsModel::StreamItem *> streams;
    QString name;
    QUrl url;
    QString genre;
    QSet<QString> names;
    QMultiHash<QString, StreamsModel::StreamItem *> genres;
    int level=0;
    QXmlStreamReader doc(QString::fromUtf8(data));

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            ++level;
            if (2==level && QLatin1String("entry")==doc.name()) {
                name=QString();
                url=QUrl();
                genre=QString();
            } else if (3==level) {
                if (QLatin1String("server_name")==doc.name()) {
                    name=doc.readElementText();
                    --level;
                } else if (QLatin1String("genre")==doc.name()) {
                    genre=fixGenre(doc.readElementText());
                    --level;
                } else if (QLatin1String("listen_url")==doc.name()) {
                    url=QUrl(doc.readElementText());
                    --level;
                }
            }
        } else if (doc.isEndElement()) {
            if (2==level && QLatin1String("entry")==doc.name() && !name.isEmpty() && url.isValid() && !names.contains(name)) {
                StreamsModel::StreamItem *item=new StreamsModel::StreamItem(name, genre, QString(), url);
                streams.append(item);
                genres.insert(item->genre, item);
                names.insert(item->name);
            }
            --level;
        }
    }
    trimGenres(genres);
    return streams;
}

static QList<StreamsModel::StreamItem *> parseSomaFm(const QByteArray &data)
{
    QList<StreamsModel::StreamItem *> streams;
    QSet<QString> names;
    QString streamFormat;
    QMultiHash<QString, StreamsModel::StreamItem *> genres;
    QString name;
    QUrl url;
    QString genre;
    int level=0;
    QXmlStreamReader doc(QString::fromUtf8(data));

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            ++level;
            if (2==level && QLatin1String("channel")==doc.name()) {
                name=QString();
                url=QUrl();
                genre=QString();
                streamFormat=QString();
            } else if (3==level) {
                if (QLatin1String("title")==doc.name()) {
                    name=doc.readElementText();
                    --level;
                } else if (QLatin1String("genre")==doc.name()) {
                    genre=fixGenre(doc.readElementText());
                    --level;
                } else if (QLatin1String("fastpls")==doc.name()) {
                    if (streamFormat.isEmpty() || QLatin1String("mp3")!=streamFormat) {
                        streamFormat=doc.attributes().value("format").toString();
                        url=QUrl(doc.readElementText());
                        --level;
                    }
                }
            }
        } else if (doc.isEndElement()) {
            if (2==level && QLatin1String("channel")==doc.name() && !name.isEmpty() && url.isValid() && !names.contains(name)) {
                StreamsModel::StreamItem *item=new StreamsModel::StreamItem(name, genre, QString(), url);
                streams.append(item);
                genres.insert(item->genre, item);
                names.insert(item->name);
            }
            --level;
        }
    }

    trimGenres(genres);
    return streams;
}

StreamsPage::StreamsPage(MainWindow *p)
    : QWidget(p)
    , enabled(false)
    , mw(p)
{
    setupUi(this);
    importAction = ActionCollection::get()->createAction("importstreams", i18n("Import Streams"), "document-import");
    exportAction = ActionCollection::get()->createAction("exportstreams", i18n("Export Streams"), "document-export");
    addAction = ActionCollection::get()->createAction("addstream", i18n("Add Stream"), "list-add");
    editAction = ActionCollection::get()->createAction("editstream", i18n("Edit"), Icons::editIcon);

    QMenu *importMenu=new QMenu(this);
    importFileAction=importMenu->addAction("From Cantata File");
    importIceCastAction=importMenu->addAction("From IceCast");
    importSomaFmCastAction=importMenu->addAction("From SomaFM");
    importAction->setMenu(importMenu);
    replacePlayQueue->setDefaultAction(p->replacePlayQueueAction);
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importFileAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(importIceCastAction, SIGNAL(triggered(bool)), this, SLOT(importIceCast()));
    connect(importSomaFmCastAction, SIGNAL(triggered(bool)), this, SLOT(importSomaFm()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(&model, SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    Icon::init(menuButton);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu=new QMenu(this);
    menu->addAction(addAction);
    menu->addAction(p->removeAction);
    menu->addAction(editAction);
    menu->addAction(importAction);
    menu->addAction(exportAction);
    menuButton->setMenu(menu);
    menuButton->setIcon(Icons::menuIcon);
    menuButton->setToolTip(i18n("Other Actions"));
    Icon::init(replacePlayQueue);

    view->setTopText(i18n("Streams"));
//     view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlayQueueAction);
    view->addAction(p->addWithPriorityAction);
    view->addAction(editAction);
    view->addAction(exportAction);
    view->addAction(p->removeAction);
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    view->setDeleteAction(p->removeAction);
    view->init(p->replacePlayQueueAction, 0);

    memset(jobs, 0, sizeof(QNetworkReply *)*WS_Count);
}

StreamsPage::~StreamsPage()
{
    for (int i=0; i<WS_Count; ++i) {
        if (jobs[i]) {
            disconnect(jobs[i], SIGNAL(finished()), this, SLOT(downloadFinished()));
            jobs[i]->deleteLater();
        }
    }
}

void StreamsPage::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;
}

void StreamsPage::refresh()
{
    if (enabled) {
        view->setLevel(0);
        model.reload();
        exportAction->setEnabled(model.rowCount()>0);
    }
}

void StreamsPage::save()
{
    model.save(true);
}

void StreamsPage::addSelectionToPlaylist(bool replace, quint8 priorty)
{
    addItemsToPlayQueue(view->selectedIndexes(), replace, priorty);
}

void StreamsPage::addItemsToPlayQueue(const QModelIndexList &indexes, bool replace, quint8 priorty)
{
    if (0==indexes.size()) {
        return;
    }

    QModelIndexList sorted=indexes;
    qSort(sorted);

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, sorted) {
        mapped.append(proxy.mapToSource(idx));
    }

    QStringList files=model.filenames(mapped, true);

    if (!files.isEmpty()) {
        emit add(files, replace, priorty);
        view->clearSelection();
    }
}

void StreamsPage::itemDoubleClicked(const QModelIndex &index)
{
    if (!static_cast<StreamsModel::Item *>(proxy.mapToSource(index).internalPointer())->isCategory()) {
        QModelIndexList indexes;
        indexes.append(index);
        addItemsToPlayQueue(indexes, false);
    }
}

void StreamsPage::downloadFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    WebStream type=WS_Count;
    bool busy=false;

    for (int i=0; i<WS_Count; ++i) {
        if (jobs[i]==reply) {
            type=(WebStream)i;
        } else if (jobs[i]) {
            busy=true;
        }
    }

    if (type!=WS_Count) {
        if(QNetworkReply::NoError==reply->error()) {
            QList<StreamsModel::StreamItem *> streams;
            switch (type) {
            case WS_IceCast:
                streams=parseIceCast(reply->readAll());
                break;
            case WS_SomaFm:
                streams=parseSomaFm(reply->readAll());
                break;
            default:
                break;
            }

            if (streams.isEmpty()) {
                MessageBox::error(this, i18n("No streams downloaded from %1").arg(webStreamName(type)));
            } else {
                model.add(webStreamName(type), streams);
            }
        } else {
            MessageBox::error(this, i18n("Failed to download streams from %1").arg(webStreamName(type)));
        }
        jobs[type]=0;
    }

    if (!busy) {
        view->hideSpinner();
    }
    reply->deleteLater();
}

void StreamsPage::importIceCast()
{
    importWebStreams(WS_IceCast);
}

void StreamsPage::importSomaFm()
{
    importWebStreams(WS_SomaFm);
}

void StreamsPage::importXml()
{
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), i18n("*.cantata|Cantata Streams"), this, i18n("Import Streams"));
    #else
    QString fileName=QFileDialog::getOpenFileName(this, i18n("Import Streams"), QDir::homePath(), i18n("Cantata Streams (*.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!model.import(fileName)) {
        MessageBox::error(this, i18n("Failed to import <b>%1</b>!<br/>Please check this is of the correct type.").arg(fileName));
    }
}

void StreamsPage::exportXml()
{
    QModelIndexList selected=view->selectedIndexes();
    QSet<StreamsModel::Item *> items;
    QSet<StreamsModel::Item *> categories;
    foreach (const QModelIndex &idx, selected) {
        QModelIndex i=proxy.mapToSource(idx);
        StreamsModel::Item *itm=static_cast<StreamsModel::Item *>(i.internalPointer());
        if (itm->isCategory()) {
            if (!categories.contains(itm)) {
                categories.insert(itm);
            }
            foreach (StreamsModel::StreamItem *s, static_cast<StreamsModel::CategoryItem *>(itm)->streams) {
                items.insert(s);
            }
        } else {
            items.insert(itm);
            categories.insert(static_cast<StreamsModel::StreamItem *>(itm)->parent);
        }
    }

    QString name;

    if (1==categories.count()) {
        name=static_cast<StreamsModel::StreamItem *>(*(categories.begin()))->name+QLatin1String(".cantata");
    }
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getSaveFileName(name, i18n("*.cantata|Cantata Streams"), this, i18n("Export Streams"));
    #else
    QString fileName=QFileDialog::getSaveFileName(this, i18n("Export Streams"), name, i18n("Cantata Streams (*.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!model.save(fileName, categories+items)) {
        MessageBox::error(this, i18n("Failed to create <b>%1</b>!").arg(fileName));
    }
}

void StreamsPage::add()
{
    StreamDialog dlg(getCategories(), getGenres(), this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();
        QString cat=dlg.category();
        QString existing=model.name(cat, url);

        if (!existing.isEmpty()) {
            MessageBox::error(this, i18n("Stream already exists!<br/><b>%1</b>").arg(existing));
            return;
        }

        if (!model.add(cat, name, dlg.genre(), dlg.icon(), url)) {
            MessageBox::error(this, i18n("A stream named <b>%1</b> already exists!").arg(name));
        }
    }
    exportAction->setEnabled(model.rowCount()>0);
}

void StreamsPage::removeItems()
{
    QStringList streams;
    QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndex firstIndex=proxy.mapToSource(selected.first());
    QString firstName=model.data(firstIndex, Qt::DisplayRole).toString();
    if (MessageBox::No==MessageBox::warningYesNo(this, selected.size()>1 ? i18n("Are you sure you wish to remove the %1 selected streams?").arg(selected.size())
                                                                         : i18n("Are you sure you wish to remove <b>%1</b>?").arg(firstName),
                                                 selected.size()>1 ? i18n("Remove Streams?") : i18n("Remove Stream?"))) {
        return;
    }

    // Ensure that if we have a category selected, we dont also try to remove one of its children
    QSet<StreamsModel::CategoryItem *> removeCategories;
    QList<StreamsModel::StreamItem *> removeStreams;
    QModelIndexList remove;
    //..obtain catagories to remove...
    foreach(QModelIndex index, selected) {
        QModelIndex idx=proxy.mapToSource(index);
        StreamsModel::Item *item=static_cast<StreamsModel::Item *>(idx.internalPointer());

        if (item->isCategory()) {
            removeCategories.insert(static_cast<StreamsModel::CategoryItem *>(item));
        }
    }
    // Obtain streams in non-selected categories...
    foreach(QModelIndex index, selected) {
        QModelIndex idx=proxy.mapToSource(index);
        StreamsModel::Item *item=static_cast<StreamsModel::Item *>(idx.internalPointer());

        if (!item->isCategory()) {
            removeStreams.append(static_cast<StreamsModel::StreamItem *>(item));
        }
    }

    foreach (StreamsModel::CategoryItem *i, removeCategories) {
        model.removeCategory(i);
    }

    foreach (StreamsModel::StreamItem *i, removeStreams) {
        model.removeStream(i);
    }
    model.updateGenres();
    exportAction->setEnabled(model.rowCount()>0);
}

void StreamsPage::edit()
{
    QStringList streams;
    QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    QModelIndex index=proxy.mapToSource(selected.first());
    StreamsModel::Item *item=static_cast<StreamsModel::Item *>(index.internalPointer());
    QString name=item->name;
    QString icon=item->icon;

    if (item->isCategory()) {
        StreamCategoryDialog dlg(getCategories(), this);
        dlg.setEdit(name, icon);
        if (QDialog::Accepted==dlg.exec()) {
            model.editCategory(index, dlg.name(), dlg.icon());
        }
        return;
    }

    StreamDialog dlg(getCategories(), getGenres(), this);
    StreamsModel::StreamItem *stream=static_cast<StreamsModel::StreamItem *>(item);
    QString url=stream->url.toString();
    QString cat=stream->parent->name;
    QString genre=stream->genre;

    dlg.setEdit(cat, name, genre, icon, url);

    if (QDialog::Accepted==dlg.exec()) {
        QString newName=dlg.name();
        #ifdef ENABLE_KDE_SUPPORT
        QString newIcon=dlg.icon();
        #else
        QString newIcon;
        #endif
        QString newUrl=dlg.url();
        QString newCat=dlg.category();
        QString newGenre=dlg.genre();
        QString existingNameForUrl=newUrl!=url ? model.name(newCat, newUrl) : QString();
//
        if (!existingNameForUrl.isEmpty()) {
            MessageBox::error(this, i18n("Stream already exists!<br/><b>%1 (%2)</b>").arg(existingNameForUrl).arg(newCat));
        } else if (newName!=name && model.entryExists(newCat, newName)) {
            MessageBox::error(this, i18n("A stream named <b>%1 (%2)</b> already exists!").arg(newName).arg(newCat));
        } else {
            model.editStream(index, cat, newCat, newName, newGenre, newIcon, newUrl);
        }
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    editAction->setEnabled(1==selected.size());
    mw->removeAction->setEnabled(selected.count());
    exportAction->setEnabled(model.rowCount());
    mw->replacePlayQueueAction->setEnabled(selected.count());
    mw->addWithPriorityAction->setEnabled(selected.count());
}

void StreamsPage::searchItems()
{
    QString text=view->searchText().trimmed();
    proxy.update(text, genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll();
    }
}

QStringList StreamsPage::getCategories()
{
    QStringList categories;
    for(int i=0; i<model.rowCount(); ++i) {
        QModelIndex idx=model.index(i, 0, QModelIndex());
        if (idx.isValid()) {
            categories.append(static_cast<StreamsModel::Item *>(idx.internalPointer())->name);
        }
    }

    if (categories.isEmpty()) {
        categories.append(i18n("General"));
    }

    qSort(categories);
    return categories;
}

QStringList StreamsPage::getGenres()
{
    QStringList g=genreCombo->entries().toList();
    qSort(g);
    return g;
}

void StreamsPage::importWebStreams(WebStream type)
{
    if (jobs[WS_IceCast]) {
        MessageBox::error(this, i18n("Download from %1 is already in progress!").arg(webStreamName(type)));
        return;
    }

    if (getCategories().contains(webStreamName(type))) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Update streams from %1?\n(This will replace any existing streams in this category)").arg(webStreamName(type)),
                                                     i18n("Update %1").arg(webStreamName(type)))) {
            return;
        }
    } else {
        if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Download streams from %1?").arg(webStreamName(type)), i18n("Download %1").arg(webStreamName(type)))) {
            return;
        }
    }

    jobs[type]=NetworkAccessManager::self()->get(QNetworkRequest(webStreamUrl(type)));
    connect(jobs[type], SIGNAL(finished()), this, SLOT(downloadFinished()));
    view->showSpinner();
}
