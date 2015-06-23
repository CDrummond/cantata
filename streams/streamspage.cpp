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

#include "streamspage.h"
#include "streamdialog.h"
#include "mpd-interface/mpdconnection.h"
#include "support/messagebox.h"
#include "support/localize.h"
#include "widgets/icons.h"
#include "gui/stdactions.h"
#include "support/actioncollection.h"
#include "network/networkaccessmanager.h"
#include "gui/settings.h"
#include "widgets/statuslabel.h"
#include "widgets/menubutton.h"
#include "widgets/itemview.h"
#include "widgets/servicestatuslabel.h"
#include "models/digitallyimported.h"
#include "digitallyimportedsettings.h"
#include <QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KFileDialog>
#else
#include <QFileDialog>
#include <QDir>
#endif
#include <QMenu>
#include <QFileInfo>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

static const int constMsgDisplayTime=1500;
static const char *constNameProperty="name";

StreamsPage::StreamsPage(QWidget *p)
    : SinglePageWidget(p)
{
    importAction = new Action(Icon("document-import"), i18n("Import Streams Into Favorites"), this);
    exportAction = new Action(Icon("document-export"), i18n("Export Favorite Streams"), this);
    addAction = ActionCollection::get()->createAction("addstream", i18n("Add New Stream To Favorites"), Icons::self()->addRadioStreamIcon);
    editAction = new Action(Icons::self()->editIcon, i18n("Edit"), this);
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered()), this, SLOT(add()));
    connect(StreamsModel::self()->addBookmarkAct(), SIGNAL(triggered()), this, SLOT(addBookmark()));
    connect(StreamsModel::self()->addToFavouritesAct(), SIGNAL(triggered()), this, SLOT(addToFavourites()));
    connect(StreamsModel::self()->configureAct(), SIGNAL(triggered()), this, SLOT(configureStreams()));
    connect(StreamsModel::self()->reloadAct(), SIGNAL(triggered()), this, SLOT(reload()));
    connect(editAction, SIGNAL(triggered()), this, SLOT(edit()));
    connect(importAction, SIGNAL(triggered()), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered()), this, SLOT(exportXml()));
    connect(StreamsModel::self(), SIGNAL(error(const QString &)), this, SIGNAL(error(const QString &)));
    connect(StreamsModel::self(), SIGNAL(loading()), view, SLOT(showSpinner()));
    connect(StreamsModel::self(), SIGNAL(loaded()), view, SLOT(hideSpinner()));
    connect(StreamsModel::self(), SIGNAL(categoriesChanged()), view, SLOT(closeSearch()));
    connect(StreamsModel::self(), SIGNAL(favouritesLoaded()), SLOT(expandFavourites()));
    connect(StreamsModel::self(), SIGNAL(addedToFavourites(QString)), SLOT(addedToFavourites(QString)));
    connect(DigitallyImported::self(), SIGNAL(loginStatus(bool,QString)), SLOT(updateDiStatus()));
    connect(DigitallyImported::self(), SIGNAL(updated()), SLOT(updateDiStatus()));
    QMenu *menu=new QMenu(this);
    menu->addAction(addAction);
    menu->addAction(StdActions::self()->removeAction);
    menu->addAction(editAction);
    menu->addAction(StreamsModel::self()->reloadAct());
    menu->addSeparator();
    menu->addAction(importAction);
    menu->addAction(exportAction);
    MenuButton *menuButton=new MenuButton(this);
    menuButton->setMenu(menu);
    view->setUniformRowHeights(true);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(editAction);
    view->addAction(StdActions::self()->removeAction);
    view->addAction(StreamsModel::self()->addToFavouritesAct());
    view->addAction(StreamsModel::self()->addBookmarkAct());
    view->addAction(StreamsModel::self()->reloadAct());
    proxy.setSourceModel(StreamsModel::self());
    view->setModel(&proxy);
    view->setDeleteAction(StdActions::self()->removeAction);
    view->setSearchResetLevel(1);

    diStatusLabel=new ServiceStatusLabel(this);
    diStatusLabel->setText("DI", i18nc("Service name", "Digitally Imported"));
    connect(diStatusLabel, SIGNAL(clicked()), SLOT(diSettings()));
    updateDiStatus();  
    init(ReplacePlayQueue|Configure, QList<QWidget *>() << menuButton << diStatusLabel);
}

StreamsPage::~StreamsPage()
{
    foreach (NetworkJob *job, resolveJobs) {
        disconnect(job, SIGNAL(finished()), this, SLOT(tuneInResolved()));
        job->deleteLater();
    }
    resolveJobs.clear();
}

void StreamsPage::showEvent(QShowEvent *e)
{
    view->focusView();
    QWidget::showEvent(e);
}

void StreamsPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    Q_UNUSED(name)
    addItemsToPlayQueue(view->selectedIndexes(), replace, priorty);
}

void StreamsPage::addItemsToPlayQueue(const QModelIndexList &indexes, bool replace, quint8 priorty)
{
    if (indexes.isEmpty()) {
        return;
    }
    QModelIndexList mapped;
    foreach (const QModelIndex &idx, indexes) {
        mapped.append(proxy.mapToSource(idx));
    }

    QStringList files=StreamsModel::self()->filenames(mapped, true);

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

void StreamsPage::configureStreams()
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.count()) {
        return;
    }

    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(selected.first()).internalPointer());
    if (item->isCategory() && static_cast<const StreamsModel::CategoryItem *>(item)->canConfigure()) {
        // TODO: In future might need to confirm category type, at the moment only digitially imported can be configured...
        diSettings();
    }
}

void StreamsPage::diSettings()
{
    DigitallyImportedSettings(this).show();
}

void StreamsPage::importXml()
{
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), i18n("*.xml *.xml.gz *.cantata|XML Streams"), this, i18n("Import Streams"));
    #else
    QString fileName=QFileDialog::getOpenFileName(this, i18n("Import Streams"), QDir::homePath(),
                                                  i18n("XML Streams (*.xml *.xml.gz *.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }
    StreamsModel::self()->importIntoFavourites(fileName);
}

void StreamsPage::exportXml()
{
    QLatin1String ext(".xml.gz");
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getSaveFileName(QLatin1String("streams")+ext, i18n("*.xml|XML Streams"), this, i18n("Export Streams"));
    #else
    QString fileName=QFileDialog::getSaveFileName(this, i18n("Export Streams"), QDir::homePath()+QLatin1String("/streams")+ext, i18n("XML Streams (*.xml.gz)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(ext)) {
        fileName+=ext;
    }

    if (!StreamsModel::self()->exportFavourites(fileName)) {
        MessageBox::error(this, i18n("Failed to create '%1'!", fileName));
    }
}

void StreamsPage::add()
{
    StreamDialog dlg(this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();
        QString existingNameForUrl=StreamsModel::self()->favouritesNameForUrl(url);

        if (!existingNameForUrl.isEmpty()) {
            MessageBox::error(this, i18n("Stream '%1' already exists!", existingNameForUrl));
        } else if (StreamsModel::self()->nameExistsInFavourites(name)) {
            MessageBox::error(this, i18n("A stream named '%1' already exists!", name));
        } else {
            StreamsModel::self()->addToFavourites(url, name);
        }
    }
}

void StreamsPage::addBookmark()
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }

    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(selected.first()).internalPointer());

    // TODO: In future, if other categories support bookmarking, then we will need to calculate parent here!!!
    if (StreamsModel::self()->addBookmark(item->url, item->name, 0)) {
        view->showMessage(i18n("Bookmark added"), constMsgDisplayTime);
    } else {
        view->showMessage(i18n("Already bookmarked"), constMsgDisplayTime);
    }
}

void StreamsPage::addToFavourites()
{
    QModelIndexList selected = view->selectedIndexes();

    QList<const StreamsModel::Item *> items;

    foreach (const QModelIndex &i, selected) {
        QModelIndex mapped=proxy.mapToSource(i);
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
        if (!item->isCategory() && item->parent && !item->parent->isFavourites()) {
            items.append(item);
        }
    }

    int added=0;
    foreach (const StreamsModel::Item *item, items) {
        QUrl url(item->url);
        #if QT_VERSION < 0x050000
        QUrl &query=url;
        #else
        QUrlQuery query(url);
        #endif
        query.removeQueryItem(QLatin1String("locale"));
        #if QT_VERSION >= 0x050000
        if (!query.isEmpty()) {
            url.setQuery(query);
        }
        #endif
        QString urlStr=url.toString();
        if (urlStr.endsWith('&')) {
            urlStr=urlStr.left(urlStr.length()-1);
        }
        if (urlStr.startsWith(QLatin1String("http://opml.radiotime.com/Tune.ashx"))) {
            NetworkJob *job=NetworkAccessManager::self()->get(urlStr, 5000);
            job->setProperty(constNameProperty, item->modifiedName());
            connect(job, SIGNAL(finished()), this, SLOT(tuneInResolved()));
            resolveJobs.insert(job);
            added++;
        } else if (StreamsModel::self()->addToFavourites(urlStr, item->modifiedName())) {
            added++;
        }
    }

    if (!added) {
        view->showMessage(i18n("Already in favorites"), constMsgDisplayTime);
    }
}

void StreamsPage::tuneInResolved()
{
    NetworkJob *job=qobject_cast<NetworkJob *>(sender());
    if (!job) {
        return;
    }
    job->deleteLater();
    if (!resolveJobs.contains(job)) {
        return;
    }
    resolveJobs.remove(job);
    QString url=job->readAll().split('\n').first();
    QString name=job->property(constNameProperty).toString();
    if (!url.isEmpty() && !name.isEmpty() && !StreamsModel::self()->addToFavourites(url, name)) {
        view->showMessage(i18n("Already in favorites"), constMsgDisplayTime);
    }
}

void StreamsPage::reload()
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.count()) {
        return;
    }

    QModelIndex mapped=proxy.mapToSource(selected.first());
    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
    if (!item->isCategory()) {
        return;
    }
    const StreamsModel::CategoryItem *cat=static_cast<const StreamsModel::CategoryItem *>(item);
    if (!cat->canReload()) {
        return;
    }

    if (cat->children.isEmpty() || cat->cacheName.isEmpty() || MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Reload '%1' streams?", cat->name))) {
        StreamsModel::self()->reload(mapped);
    }
}

void StreamsPage::removeItems()
{
    QModelIndexList selected = view->selectedIndexes();
    
    if (1==selected.count()) {
        QModelIndex mapped=proxy.mapToSource(selected.first());
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
        if (item->isCategory() && item->parent) {
            if (item->parent->isBookmarks) {
                if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove bookmark to '%1'?", item->name))) {
                    return;
                }
                StreamsModel::self()->removeBookmark(mapped);
                return;
            } else if (static_cast<const StreamsModel::CategoryItem *>(item)->isBookmarks) {
                if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove all '%1' bookmarks?", item->parent->name))) {
                    return;
                }
                StreamsModel::self()->removeAllBookmarks(mapped);
                return;
            }
        }
    }
        
    QModelIndexList useable;

    foreach (const QModelIndex &i, selected) {
        QModelIndex mapped=proxy.mapToSource(i);
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
        if (!item->isCategory() && item->parent && item->parent->isFavourites()) {
            useable.append(mapped);
        }
    }

    if (useable.isEmpty()) {
        return;
    }

    if (useable.size()>1) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the %1 selected streams?", useable.size()))) {
            return;
        }
    } else {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove '%1'?",
                                                     StreamsModel::self()->data(useable.first(), Qt::DisplayRole).toString()))) {
            return;
        }
    }

    StreamsModel::self()->removeFromFavourites(useable);
}

void StreamsPage::edit()
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.size()) {
        return;
    }

    QModelIndex index=proxy.mapToSource(selected.first());
    StreamsModel::Item *item=static_cast<StreamsModel::Item *>(index.internalPointer());
    if (item->isCategory() || !item->parent || !item->parent->isFavourites()) {
        return;
    }

    QString name=item->name;
    QString url=item->url;

    StreamDialog dlg(this);
    dlg.setEdit(name, url);

    if (QDialog::Accepted==dlg.exec()) {
        QString newName=dlg.name();
        QString newUrl=dlg.url();
        QString existingNameForUrl=newUrl!=url ? StreamsModel::self()->favouritesNameForUrl(newUrl) : QString();

        if (!existingNameForUrl.isEmpty()) {
            MessageBox::error(this, i18n("Stream '%1' already exists!", existingNameForUrl));
        } else if (newName!=name && StreamsModel::self()->nameExistsInFavourites(newName)) {
            MessageBox::error(this, i18n("A stream named '%1' already exists!", newName));
        } else {
            StreamsModel::self()->updateFavouriteStream(newUrl, newName, index);
        }
    }
}

void StreamsPage::doSearch()
{
    QString text=view->searchText().trimmed();
    if (!view->isSearchActive()) {
        proxy.setFilterItem(0);
    }
    proxy.update(view->isSearchActive() ? text : QString());
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll(proxy.filterItem()
                        ? proxy.mapFromSource(StreamsModel::self()->categoryIndex(static_cast<const StreamsModel::CategoryItem *>(proxy.filterItem())))
                        : QModelIndex());
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool haveSelection=!selected.isEmpty();
    bool enableAddToFav=true;
    bool onlyStreamsSelected=true;
    StreamsModel::self()->addBookmarkAct()->setEnabled(false);

    editAction->setEnabled(false);
    StreamsModel::self()->reloadAct()->setEnabled(false);

    bool enableRemove=true;
    foreach (const QModelIndex &idx, selected) {
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(idx).internalPointer());
        if (item->isCategory() || (item->parent && !item->parent->isFavourites())) {
            enableRemove=false;
        }
        if (item->isCategory() || (item->parent && item->parent->isFavourites())) {
            enableAddToFav=false;
        }
        if (item->isCategory()) {
            onlyStreamsSelected=false;
        }
        if (!enableRemove && !enableAddToFav && !onlyStreamsSelected) {
            break;
        }
    }

    StdActions::self()->removeAction->setEnabled(haveSelection && enableRemove);
    StreamsModel::self()->addToFavouritesAct()->setEnabled(haveSelection && enableAddToFav);

    if (1==selected.size()) {
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(selected.first()).internalPointer());
        if (!item->isCategory() && item->parent && item->parent->isFavourites()) {
            editAction->setEnabled(true);
        }
        StreamsModel::self()->reloadAct()->setEnabled(item->isCategory() && static_cast<const StreamsModel::CategoryItem *>(item)->canReload());
        StreamsModel::self()->addBookmarkAct()->setEnabled(item->isCategory() && static_cast<const StreamsModel::CategoryItem *>(item)->canBookmark);
        if (!StdActions::self()->removeAction->isEnabled()) {
            StdActions::self()->removeAction->setEnabled(item->isCategory() && item->parent &&
                                                         (item->parent->isBookmarks || (static_cast<const StreamsModel::CategoryItem *>(item)->isBookmarks)));
        }
        StreamsModel::self()->configureAct()->setEnabled(item->isCategory() && static_cast<const StreamsModel::CategoryItem *>(item)->canConfigure());
    } else {
        StreamsModel::self()->configureAct()->setEnabled(false);
    }

    StdActions::self()->replacePlayQueueAction->setEnabled(haveSelection && onlyStreamsSelected);
}

void StreamsPage::updateDiStatus()
{
    if (DigitallyImported::self()->user().isEmpty() || DigitallyImported::self()->pass().isEmpty()) {
        diStatusLabel->setVisible(false);
    } else {
        diStatusLabel->setStatus(DigitallyImported::self()->loggedIn());
    }
}

void StreamsPage::configure()
{
    emit showPreferencesPage(QLatin1String("streams"));
}

void StreamsPage::expandFavourites()
{
    view->expand(proxy.mapFromSource(StreamsModel::self()->favouritesIndex()), true);
}

void StreamsPage::addedToFavourites(const QString &name)
{
    view->showMessage(i18n("Added '%1'' to favorites", name), constMsgDisplayTime);
}
