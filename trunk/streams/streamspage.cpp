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
#include "mpd/mpdconnection.h"
#include "support/messagebox.h"
#include "support/localize.h"
#include "widgets/icons.h"
#include "gui/stdactions.h"
#include "support/actioncollection.h"
#include "network/networkaccessmanager.h"
#include "gui/settings.h"
#include "widgets/statuslabel.h"
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

StreamsPage::StreamsPage(QWidget *p)
    : QWidget(p)
    , searching(false)
    , proxy(&streamsProxy)
{
    setupUi(this);
    importAction = ActionCollection::get()->createAction("importstreams", i18n("Import Streams Into Favorites"), "document-import");
    exportAction = ActionCollection::get()->createAction("exportstreams", i18n("Export Favorite Streams"), "document-export");
    addAction = ActionCollection::get()->createAction("addstream", i18n("Add New Stream To Favorites"), Icons::self()->addRadioStreamIcon);
    editAction = ActionCollection::get()->createAction("editstream", i18n("Edit"), Icons::self()->editIcon);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchIsActive(bool)), this, SLOT(controlSearch(bool)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(searchView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(searchView, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(searchView, SIGNAL(searchIsActive(bool)), this, SLOT(controlSearch(bool)));
    connect(searchView, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(StreamsModel::self()->addBookmarkAct(), SIGNAL(triggered(bool)), this, SLOT(addBookmark()));
    connect(StreamsModel::self()->addToFavouritesAct(), SIGNAL(triggered(bool)), this, SLOT(addToFavourites()));
    connect(StreamsModel::self()->configureAct(), SIGNAL(triggered(bool)), this, SLOT(configureStreams()));
    connect(StreamsModel::self()->reloadAct(), SIGNAL(triggered(bool)), this, SLOT(reload()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    connect(StreamsModel::self(), SIGNAL(error(const QString &)), this, SIGNAL(error(const QString &)));
    connect(StreamsModel::self(), SIGNAL(loading()), view, SLOT(showSpinner()));
    connect(StreamsModel::self(), SIGNAL(loaded()), view, SLOT(hideSpinner()));
    connect(StreamsModel::self(), SIGNAL(categoriesChanged()), view, SLOT(closeSearch()));
    connect(StreamsModel::self(), SIGNAL(favouritesLoaded()), SLOT(expandFavourites()));
    connect(StreamsModel::self(), SIGNAL(addedToFavourites(QString)), SLOT(addedToFavourites(QString)));
    connect(&searchModel, SIGNAL(loading()), searchView, SLOT(showSpinner()));
    connect(&searchModel, SIGNAL(loaded()), searchView, SLOT(hideSpinner()));
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
    menu->addSeparator();
    QAction *configAction=new QAction(Icons::self()->configureIcon, i18n("Configure..."), this);
    menu->addAction(configAction);
    connect(configAction, SIGNAL(triggered(bool)), this, SLOT(showPreferencesPage()));
    menuButton->setMenu(menu);
    Icon::init(replacePlayQueue);

    view->setUniformRowHeights(true);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(editAction);
    view->addAction(StdActions::self()->removeAction);
    view->addAction(StreamsModel::self()->addToFavouritesAct());
    view->addAction(StreamsModel::self()->addBookmarkAct());
    view->addAction(StreamsModel::self()->reloadAct());
    streamsProxy.setSourceModel(StreamsModel::self());
    view->setModel(&streamsProxy);
    view->setDeleteAction(StdActions::self()->removeAction);
    view->setSearchResetLevel(1);

    searchView->setUniformRowHeights(true);
    searchView->addAction(StdActions::self()->replacePlayQueueAction);
    searchView->addAction(StreamsModel::self()->addToFavouritesAct());
    searchView->addAction(StreamsModel::self()->addBookmarkAct());
    searchProxy.setSourceModel(&searchModel);
    searchView->setModel(&searchProxy);

    diStatusLabel->setText("DI", i18nc("Service name", "Digitally Imported"));
    connect(diStatusLabel, SIGNAL(clicked()), SLOT(diSettings()));
    updateDiStatus();  
}

StreamsPage::~StreamsPage()
{
}

void StreamsPage::focusSearch()
{
    if (view->searchVisible()) {
        view->clearSearchText();
        searchView->clearSearchText();
        view->focusSearch();
        searchView->focusSearch();
        controlSearch(true);
    } else {
        view->focusSearch();
        searchView->focusSearch();
    }
}

void StreamsPage::showEvent(QShowEvent *e)
{
    view->focusView();
    QWidget::showEvent(e);
}

void StreamsPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    Q_UNUSED(name)
    addItemsToPlayQueue(itemView()->selectedIndexes(), replace, priorty);
}

void StreamsPage::addItemsToPlayQueue(const QModelIndexList &indexes, bool replace, quint8 priorty)
{
    if (indexes.isEmpty()) {
        return;
    }
    QModelIndexList mapped;
    foreach (const QModelIndex &idx, indexes) {
        mapped.append(proxy->mapToSource(idx));
    }

    QStringList files=searching ? searchModel.filenames(mapped, true) : StreamsModel::self()->filenames(mapped, true);

    if (!files.isEmpty()) {
        emit add(files, replace, priorty);
        itemView()->clearSelection();
    }
}

void StreamsPage::itemDoubleClicked(const QModelIndex &index)
{
    if (!static_cast<StreamsModel::Item *>(proxy->mapToSource(index).internalPointer())->isCategory()) {
        QModelIndexList indexes;
        indexes.append(index);
        addItemsToPlayQueue(indexes, false);
    }
}

void StreamsPage::configureStreams()
{
    if (searching) {
        return;
    }

    QModelIndexList selected = itemView()->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.count()) {
        return;
    }

    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy->mapToSource(selected.first()).internalPointer());
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
    if (searching) {
        return;
    }
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
    if (searching) {
        return;
    }

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
    if (searching) {
        return;
    }
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
    QModelIndexList selected = itemView()->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }

    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy->mapToSource(selected.first()).internalPointer());

    // TODO: In future, if other categories support bookmarking, then we will need to calculate parent here!!!
    if (StreamsModel::self()->addBookmark(item->url, item->name, 0)) {
        itemView()->showMessage(i18n("Bookmark added"), constMsgDisplayTime);
    } else {
        itemView()->showMessage(i18n("Already bookmarked"), constMsgDisplayTime);
    }
}

void StreamsPage::addToFavourites()
{
    QModelIndexList selected = itemView()->selectedIndexes();

    QList<const StreamsModel::Item *> items;

    foreach (const QModelIndex &i, selected) {
        QModelIndex mapped=proxy->mapToSource(i);
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
        if (StreamsModel::self()->addToFavourites(urlStr, item->modifiedName())) {
            added++;
        }
    }

    if (!added) {
        itemView()->showMessage(i18n("Already in favorites"), constMsgDisplayTime);
    }
}

void StreamsPage::reload()
{
    if (searching) {
        return;
    }

    QModelIndexList selected = itemView()->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.count()) {
        return;
    }

    QModelIndex mapped=proxy->mapToSource(selected.first());
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
    if (searching) {
        return;
    }

    QModelIndexList selected = itemView()->selectedIndexes();
    
    if (1==selected.count()) {
        QModelIndex mapped=proxy->mapToSource(selected.first());
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
        QModelIndex mapped=proxy->mapToSource(i);
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
    if (searching) {
        return;
    }

    QModelIndexList selected = itemView()->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.size()) {
        return;
    }

    QModelIndex index=proxy->mapToSource(selected.first());
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

void StreamsPage::searchItems()
{
    if (!searching) {
        return;
    }
    if (proxy==&searchProxy) {
        searchModel.search(searchView->searchText().trimmed(), false);
    } else {
        QString text=view->searchText().trimmed();
        if (!view->isSearchActive()) {
            proxy->setFilterItem(0);
        }
        proxy->update(view->isSearchActive() ? text : QString(), QString());
        if (proxy->enabled() && !text.isEmpty()) {
            view->expandAll(proxy->filterItem()
                            ? proxy->mapFromSource(StreamsModel::self()->categoryIndex(static_cast<const StreamsModel::CategoryItem *>(proxy->filterItem())))
                            : QModelIndex());
        }
    }
}

StreamsModel::CategoryItem * StreamsPage::getSearchCategory()
{
    bool srch=searching;
    searching=false;
    QModelIndexList selected = itemView()->selectedIndexes(false); // Dont need sorted selection here...
    searching=srch;

    if (1!=selected.size()) {
        return StreamsModel::self()->tuneInCat();
    }

    QModelIndex index=proxy->mapToSource(selected.first());
    StreamsModel::Item *item=static_cast<StreamsModel::Item *>(index.internalPointer());
    if (!item) {
        return StreamsModel::self()->tuneInCat();
    }
    StreamsModel::CategoryItem *cat=item->getTopLevelCategory();
    return 0==cat ? StreamsModel::self()->tuneInCat() : cat;
}

void StreamsPage::controlSearch(bool on)
{
    StreamsModel::CategoryItem *cat=0;
    StreamSearchModel::Category searchCat=StreamSearchModel::TuneIn;

    if (on) {
        cat=getSearchCategory();
        searchCat=StreamsModel::self()->isTuneIn(cat)
            ? StreamSearchModel::TuneIn
            : StreamsModel::self()->isShoutCast(cat)
                ? StreamSearchModel::ShoutCast
                    : StreamsModel::self()->isDirble(cat)
                    ? StreamSearchModel::Dirble
                    : StreamSearchModel::Filter;
    }

    if (on!=searching || StreamSearchModel::Filter==searchCat) {
        searching=on;
        if (searching) {
            if (StreamSearchModel::Filter==searchCat) {
                proxy=&streamsProxy;
                searchModel.clear();
                viewStack->setCurrentIndex(0);
                view->setSearchLabelText(i18n("Search %1:", cat->name));
                view->setBackgroundImage(cat->icon);
                proxy->setFilterItem(cat);
                proxy->update(QString(), QString());
                QModelIndex filterIndex=cat ? StreamsModel::self()->categoryIndex(cat) : QModelIndex();
                if (filterIndex.isValid()) {
                    view->expand(proxy->mapFromSource(filterIndex), true);
                }
                // We need to call focusSearch via 'invokeMethod' as we need this to occur at the next event-loop iteration.
                // This is due to the fact that we have 2 views - search (for TuneIn/ShoutCast) and standard. Our focusSearch()
                // calls focusSearch on both - which triggers this controlSearch().
                QMetaObject::invokeMethod(view, "focusSearch", Qt::QueuedConnection);
            } else {
                proxy=&searchProxy;
                searchModel.setCat(searchCat);
                viewStack->setCurrentIndex(1);
                searchView->setSearchLabelText(i18n("Search %1:", cat->name));
                searchView->setBackgroundImage(cat->icon);
            }
            view->clearSelection();
        } else {
            proxy->setFilterItem(0);
            proxy->update(QString(), QString());
            proxy=&streamsProxy;
            searchModel.clear();
            view->setSearchVisible(false);
            viewStack->setCurrentIndex(0);
            view->setBackgroundImage(QIcon());
        }
        controlActions();
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=itemView()->selectedIndexes(false); // Dont need sorted selection here...
    bool haveSelection=!selected.isEmpty();
    bool enableAddToFav=true;
    bool onlyStreamsSelected=true;
    StreamsModel::self()->addBookmarkAct()->setEnabled(false);
    if (searching) {
        foreach (const QModelIndex &idx, selected) {
            const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy->mapToSource(idx).internalPointer());
            if (item->isCategory()) {
                enableAddToFav=false;
                onlyStreamsSelected=false;
                break;
            }
        }
        StreamsModel::self()->reloadAct()->setEnabled(false);
        StdActions::self()->removeAction->setEnabled(false);
        StreamsModel::self()->addToFavouritesAct()->setEnabled(haveSelection && enableAddToFav);
        StreamsModel::self()->configureAct()->setEnabled(false);
        if (1==selected.size()) {
            StreamsModel::self()->addBookmarkAct()->setEnabled(static_cast<const StreamsModel::Item *>(proxy->mapToSource(selected.first()).internalPointer())
                                                                    ->isCategory());
        }
    } else {
        editAction->setEnabled(false);
        StreamsModel::self()->reloadAct()->setEnabled(false);

        bool enableRemove=true;
        foreach (const QModelIndex &idx, selected) {
            const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy->mapToSource(idx).internalPointer());
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
            const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy->mapToSource(selected.first()).internalPointer());
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
    }
    StdActions::self()->replacePlayQueueAction->setEnabled(haveSelection && onlyStreamsSelected);
    menuButton->controlState();
}

void StreamsPage::updateDiStatus()
{
    if (DigitallyImported::self()->user().isEmpty() || DigitallyImported::self()->pass().isEmpty()) {
        diStatusLabel->setVisible(false);
    } else {
        diStatusLabel->setStatus(DigitallyImported::self()->loggedIn());
    }
}

void StreamsPage::showPreferencesPage()
{
    emit showPreferencesPage(QLatin1String("streams"));
}

void StreamsPage::expandFavourites()
{
    view->expand(proxy->mapFromSource(StreamsModel::self()->favouritesIndex()), true);
}

void StreamsPage::addedToFavourites(const QString &name)
{
    itemView()->showMessage(i18n("Added '%1'' to favorites", name), constMsgDisplayTime);
}

