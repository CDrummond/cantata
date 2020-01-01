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

#include "streamspage.h"
#include "streamdialog.h"
#include "streamssettings.h"
#include "mpd-interface/mpdconnection.h"
#include "support/messagebox.h"
#include "widgets/icons.h"
#include "gui/stdactions.h"
#include "support/actioncollection.h"
#include "network/networkaccessmanager.h"
#include "support/configuration.h"
#include "support/monoicon.h"
#include "gui/settings.h"
#include "widgets/menubutton.h"
#include "widgets/itemview.h"
#include "widgets/servicestatuslabel.h"
#include "models/digitallyimported.h"
#include "models/playqueuemodel.h"
#include "digitallyimportedsettings.h"
#include <QToolButton>
#include <QFileDialog>
#include <QDir>
#include <QMenu>
#include <QFileInfo>
#include <QUrlQuery>

static const int constMsgDisplayTime=1500;
static const char *constNameProperty="name";

StreamsPage::StreamsPage(QWidget *p)
    : StackedPageWidget(p)
{
    qRegisterMetaType<StreamItem>("StreamItem");
    qRegisterMetaType<QList<StreamItem> >("QList<StreamItem>");

    browse=new StreamsBrowsePage(this);
    addWidget(browse);
    connect(browse, SIGNAL(close()), this, SIGNAL(close()));
    connect(browse, SIGNAL(searchForStreams()), this, SLOT(searchForStreams()));
    search=new StreamSearchPage(this);
    addWidget(search);
    connect(search, SIGNAL(close()), this, SLOT(closeSearch()));

    disconnect(browse, SIGNAL(add(const QStringList &, int, quint8, bool)), MPDConnection::self(), SLOT(add(const QStringList &, int, quint8, bool)));
    disconnect(search, SIGNAL(add(const QStringList &, int, quint8, bool)), MPDConnection::self(), SLOT(add(const QStringList &, int, quint8, bool)));
    connect(browse, SIGNAL(add(const QStringList &, int, quint8, bool)), PlayQueueModel::self(), SLOT(addItems(const QStringList &, int, quint8, bool)));
    connect(search, SIGNAL(add(const QStringList &, int, quint8, bool)), PlayQueueModel::self(), SLOT(addItems(const QStringList &, int, quint8, bool)));
    connect(StreamsModel::self()->addToFavouritesAct(), SIGNAL(triggered()), this, SLOT(addToFavourites()));
    connect(search, SIGNAL(addToFavourites(QList<StreamItem>)), browse, SLOT(addToFavourites(QList<StreamItem>)));
}

StreamsPage:: ~StreamsPage()
{
}

void StreamsPage::searchForStreams()
{
    setCurrentWidget(search);
}

void StreamsPage::closeSearch()
{
    setCurrentWidget(browse);
}

void StreamsPage::addToFavourites()
{
    QWidget *w=currentWidget();
    if (browse==w) {
        browse->addToFavourites();
    } else if (search==w) {
        search->addToFavourites();
    }
}

StreamsBrowsePage::StreamsBrowsePage(QWidget *p)
    : SinglePageWidget(p)
    , settings(nullptr)
{
    QColor iconCol=Utils::monoIconColor();
    importAction = new Action(MonoIcon::icon(FontAwesome::arrowright, iconCol), tr("Import Streams Into Favorites"), this);
    exportAction = new Action(MonoIcon::icon(FontAwesome::arrowleft, iconCol), tr("Export Favorite Streams"), this);
    addAction = ActionCollection::get()->createAction("addstream", tr("Add New Stream To Favorites"));
    editAction = new Action(Icons::self()->editIcon, tr("Edit"), this);
    searchAction = new Action(Icons::self()->searchIcon, tr("Seatch For Streams"), this);
    connect(searchAction, SIGNAL(triggered()), this, SIGNAL(searchForStreams()));
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered()), this, SLOT(addStream()));
    connect(StreamsModel::self()->addBookmarkAct(), SIGNAL(triggered()), this, SLOT(addBookmark()));
    connect(StreamsModel::self()->configureDiAct(), SIGNAL(triggered()), this, SLOT(configureDi()));
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
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    StreamsModel::self()->configureDiAct()->setEnabled(false);

    proxy.setSourceModel(StreamsModel::self());
    view->setModel(&proxy);
    view->setDeleteAction(StdActions::self()->removeAction);
    view->setSearchResetLevel(1);
    view->alwaysShowHeader();

    Configuration config(metaObject()->className());
    view->setMode(ItemView::Mode_DetailedTree);
    view->load(config);

    MenuButton *menuButton=new MenuButton(this);
    Action *configureAction=new Action(Icons::self()->configureIcon, tr("Configure"), this);
    connect(configureAction, SIGNAL(triggered()), SLOT(configure()));
    menuButton->addAction(createViewMenu(QList<ItemView::Mode>()  << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                                  << ItemView::Mode_DetailedTree << ItemView::Mode_List));
    menuButton->addAction(configureAction);
    menuButton->addAction(StreamsModel::self()->configureDiAct());
    menuButton->addSeparator();
    menuButton->addAction(addAction);
    menuButton->addAction(StdActions::self()->removeAction);
    menuButton->addAction(editAction);
    menuButton->addAction(StreamsModel::self()->reloadAct());
    menuButton->addSeparator();
    menuButton->addAction(importAction);
    menuButton->addAction(exportAction);

    diStatusLabel=new ServiceStatusLabel(this);
    diStatusLabel->setText("DI", tr("Digitally Imported", "Service name"));
    connect(diStatusLabel, SIGNAL(clicked()), SLOT(diSettings()));
    updateDiStatus();  
    ToolButton *searchButton=new ToolButton(this);
    searchButton->setDefaultAction(searchAction);
    init(ReplacePlayQueue, QList<QWidget *>() << menuButton << diStatusLabel, QList<QWidget *>() << searchButton);

    view->addAction(editAction);
    view->addAction(StdActions::self()->removeAction);
    view->addAction(StreamsModel::self()->addToFavouritesAct());
    view->addAction(StreamsModel::self()->addBookmarkAct());
    view->addAction(StreamsModel::self()->reloadAct());
}

StreamsBrowsePage::~StreamsBrowsePage()
{
    for (NetworkJob *job: resolveJobs) {
        disconnect(job, SIGNAL(finished()), this, SLOT(tuneInResolved()));
        job->deleteLater();
    }
    resolveJobs.clear();
    Configuration config(metaObject()->className());
    view->save(config);
}

void StreamsBrowsePage::showEvent(QShowEvent *e)
{
    view->focusView();
    QWidget::showEvent(e);
}

void StreamsBrowsePage::addSelectionToPlaylist(const QString &name, int action, quint8 priority, bool decreasePriority)
{
    Q_UNUSED(name)
    addItemsToPlayQueue(view->selectedIndexes(), action, priority, decreasePriority);
}

void StreamsBrowsePage::addItemsToPlayQueue(const QModelIndexList &indexes, int action, quint8 priority, bool decreasePriority)
{
    if (indexes.isEmpty()) {
        return;
    }
    QModelIndexList mapped;
    for (const QModelIndex &idx: indexes) {
        mapped.append(proxy.mapToSource(idx));
    }

    QStringList files=StreamsModel::self()->filenames(mapped, true);

    if (!files.isEmpty()) {
        emit add(files, action, priority, decreasePriority);
        view->clearSelection();
    }
}

void StreamsBrowsePage::itemDoubleClicked(const QModelIndex &index)
{
    if (!static_cast<StreamsModel::Item *>(proxy.mapToSource(index).internalPointer())->isCategory()) {
        QModelIndexList indexes;
        indexes.append(index);
        addItemsToPlayQueue(indexes, false);
    }
}

void StreamsBrowsePage::configure()
{
    if (!settings) {
        settings=new StreamsSettings(this);
    }
    if (!settings->isVisible()) {
        settings->load();
    }
    settings->show();
    settings->raiseWindow();
}

void StreamsBrowsePage::configureDi()
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.count()) {
        return;
    }

    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(selected.first()).internalPointer());
    if (item->isCategory() && static_cast<const StreamsModel::CategoryItem *>(item)->isDi()) {
        diSettings();
    }
}

void StreamsBrowsePage::diSettings()
{
    DigitallyImportedSettings(this).show();
}

void StreamsBrowsePage::importXml()
{
    QString fileName=QFileDialog::getOpenFileName(this, tr("Import Streams"), QDir::homePath(),
                                                  tr("XML Streams (*.xml *.xml.gz *.cantata)"));

    if (fileName.isEmpty()) {
        return;
    }
    StreamsModel::self()->importIntoFavourites(fileName);
}

void StreamsBrowsePage::exportXml()
{
    QLatin1String ext(".xml.gz");
    QString fileName=QFileDialog::getSaveFileName(this, tr("Export Streams"), QDir::homePath()+QLatin1String("/streams")+ext, tr("XML Streams (*.xml.gz)"));

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(ext)) {
        fileName+=ext;
    }

    if (!StreamsModel::self()->exportFavourites(fileName)) {
        MessageBox::error(this, tr("Failed to create '%1'!").arg(fileName));
    }
}

void StreamsBrowsePage::addStream()
{
    StreamDialog dlg(this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();
        QString existingNameForUrl=StreamsModel::self()->favouritesNameForUrl(url);

        if (!existingNameForUrl.isEmpty()) {
            MessageBox::error(this, tr("Stream '%1' already exists!").arg(existingNameForUrl));
        } else if (StreamsModel::self()->nameExistsInFavourites(name)) {
            MessageBox::error(this, tr("A stream named '%1' already exists!").arg(name));
        } else {
            StreamsModel::self()->addToFavourites(url, name);
        }
    }
}

void StreamsBrowsePage::addBookmark()
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1!=selected.count()) {
        return;
    }

    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(selected.first()).internalPointer());

    // TODO: In future, if other categories support bookmarking, then we will need to calculate parent here!!!
    if (StreamsModel::self()->addBookmark(item->url, item->name, nullptr)) {
        view->showMessage(tr("Bookmark added"), constMsgDisplayTime);
    } else {
        view->showMessage(tr("Already bookmarked"), constMsgDisplayTime);
    }
}

void StreamsBrowsePage::addToFavourites()
{
    QModelIndexList selected = view->selectedIndexes();

    QList<const StreamsModel::Item *> items;

    for (const QModelIndex &i: selected) {
        QModelIndex mapped=proxy.mapToSource(i);
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
        if (!item->isCategory() && item->parent && !item->parent->isFavourites()) {
            items.append(item);
        }
    }
    QList<StreamItem> itemsToAdd;
    for (const StreamsModel::Item *item: items) {
        itemsToAdd.append(StreamItem(item->url, item->modifiedName()));
    }
    addToFavourites(itemsToAdd);
}

void StreamsBrowsePage::addToFavourites(const QList<StreamItem> &items)
{
    int added=0;
    for (const StreamItem item: items) {
        QUrl url(item.url);
        QUrlQuery query(url);
        query.removeQueryItem(QLatin1String("locale"));
        if (!query.isEmpty()) {
            url.setQuery(query);
        }
        QString urlStr=url.toString();
        if (urlStr.endsWith('&')) {
            urlStr=urlStr.left(urlStr.length()-1);
        }
        if (urlStr.startsWith(QLatin1String("http://opml.radiotime.com/Tune.ashx"))) {
            NetworkJob *job=NetworkAccessManager::self()->get(urlStr, 5000);
            job->setProperty(constNameProperty, item.modifiedName);
            connect(job, SIGNAL(finished()), this, SLOT(tuneInResolved()));
            resolveJobs.insert(job);
            added++;
        } else if (StreamsModel::self()->addToFavourites(urlStr, item.modifiedName)) {
            added++;
        }
    }

    if (!added) {
        view->showMessage(tr("Already in favorites"), constMsgDisplayTime);
    }
}

void StreamsBrowsePage::tuneInResolved()
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
        view->showMessage(tr("Already in favorites"), constMsgDisplayTime);
    }
}

void StreamsBrowsePage::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void StreamsBrowsePage::reload()
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

    if (cat->children.isEmpty() || cat->cacheName.isEmpty() || MessageBox::Yes==MessageBox::questionYesNo(this, tr("Reload '%1' streams?").arg(cat->name))) {
        StreamsModel::self()->reload(mapped);
    }
}

void StreamsBrowsePage::removeItems()
{
    QModelIndexList selected = view->selectedIndexes();
    
    if (1==selected.count()) {
        QModelIndex mapped=proxy.mapToSource(selected.first());
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
        if (item->isCategory() && item->parent) {
            if (item->parent->isBookmarks) {
                if (MessageBox::No==MessageBox::warningYesNo(this, tr("Are you sure you wish to remove bookmark to '%1'?").arg(item->name))) {
                    return;
                }
                StreamsModel::self()->removeBookmark(mapped);
                return;
            } else if (static_cast<const StreamsModel::CategoryItem *>(item)->isBookmarks) {
                if (MessageBox::No==MessageBox::warningYesNo(this, tr("Are you sure you wish to remove all '%1' bookmarks?").arg(item->parent->name))) {
                    return;
                }
                StreamsModel::self()->removeAllBookmarks(mapped);
                return;
            }
        }
    }
        
    QModelIndexList useable;

    for (const QModelIndex &i: selected) {
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
        if (MessageBox::No==MessageBox::warningYesNo(this, tr("Are you sure you wish to remove the %1 selected streams?").arg(useable.size()))) {
            return;
        }
    } else {
        if (MessageBox::No==MessageBox::warningYesNo(this, tr("Are you sure you wish to remove '%1'?").arg(
                                                     StreamsModel::self()->data(useable.first(), Qt::DisplayRole).toString()))) {
            return;
        }
    }

    StreamsModel::self()->removeFromFavourites(useable);
}

void StreamsBrowsePage::edit()
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
            MessageBox::error(this, tr("Stream '%1' already exists!").arg(existingNameForUrl));
        } else if (newName!=name && StreamsModel::self()->nameExistsInFavourites(newName)) {
            MessageBox::error(this, tr("A stream named '%1' already exists!").arg(newName));
        } else {
            StreamsModel::self()->updateFavouriteStream(newUrl, newName, index);
        }
    }
}

void StreamsBrowsePage::doSearch()
{
    QString text=view->searchText().trimmed();
    if (!view->isSearchActive()) {
        proxy.setFilterItem(nullptr);
    }
    proxy.update(view->isSearchActive() ? text : QString());
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll(proxy.filterItem()
                        ? proxy.mapFromSource(StreamsModel::self()->categoryIndex(static_cast<const StreamsModel::CategoryItem *>(proxy.filterItem())))
                        : QModelIndex());
    }
}

void StreamsBrowsePage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool haveSelection=!selected.isEmpty();
    bool enableAddToFav=true;
    bool onlyStreamsSelected=true;
    StreamsModel::self()->addBookmarkAct()->setEnabled(false);

    editAction->setEnabled(false);
    StreamsModel::self()->reloadAct()->setEnabled(false);

    bool enableRemove=true;
    for (const QModelIndex &idx: selected) {
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
        StreamsModel::self()->configureDiAct()->setEnabled(item->isCategory() && static_cast<const StreamsModel::CategoryItem *>(item)->isDi());
    } else {
        StreamsModel::self()->configureDiAct()->setEnabled(false);
    }

    StdActions::self()->replacePlayQueueAction->setEnabled(haveSelection && onlyStreamsSelected);
}

void StreamsBrowsePage::updateDiStatus()
{
    if (DigitallyImported::self()->user().isEmpty() || DigitallyImported::self()->pass().isEmpty()) {
        diStatusLabel->setVisible(false);
    } else {
        diStatusLabel->setStatus(DigitallyImported::self()->loggedIn());
    }
}

void StreamsBrowsePage::expandFavourites()
{
    view->expand(proxy.mapFromSource(StreamsModel::self()->favouritesIndex()), true);
}

void StreamsBrowsePage::addedToFavourites(const QString &name)
{
    view->showMessage(tr("Added '%1'' to favorites").arg(name), constMsgDisplayTime);
}

StreamSearchPage::StreamSearchPage(QWidget *p)
    : SinglePageWidget(p)
{
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    view->alwaysShowHeader();
    view->setPermanentSearch();
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    view->setMode(ItemView::Mode_DetailedTree);
    init(ReplacePlayQueue);
    connect(StreamsModel::self(), SIGNAL(addedToFavourites(QString)), SLOT(addedToFavourites(QString)));
}

StreamSearchPage::~StreamSearchPage()
{

}

void StreamSearchPage::showEvent(QShowEvent *e)
{
    SinglePageWidget::showEvent(e);
    view->focusSearch();
}

void StreamSearchPage::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void StreamSearchPage::doSearch()
{
    model.search(view->searchText().trimmed(), false);
}

void StreamSearchPage::addSelectionToPlaylist(const QString &name, int action, quint8 priority, bool decreasePriority)
{
    Q_UNUSED(name)
    QModelIndexList indexes=view->selectedIndexes();
    if (indexes.isEmpty()) {
        return;
    }
    QModelIndexList mapped;
    for (const QModelIndex &idx: indexes) {
        mapped.append(proxy.mapToSource(idx));
    }

    QStringList files=StreamsModel::self()->filenames(mapped, true);

    if (!files.isEmpty()) {
        emit add(files, action, priority, decreasePriority);
        view->clearSelection();
    }
}

void StreamSearchPage::addToFavourites()
{
    QModelIndexList selected = view->selectedIndexes();

    QList<const StreamsModel::Item *> items;

    for (const QModelIndex &i: selected) {
        QModelIndex mapped=proxy.mapToSource(i);
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
        if (!item->isCategory() && item->parent && !item->parent->isFavourites()) {
            items.append(item);
        }
    }
    QList<StreamItem> itemsToAdd;
    for (const StreamsModel::Item *item: items) {
        itemsToAdd.append(StreamItem(item->url, item->modifiedName()));
    }
    emit addToFavourites(itemsToAdd);
}

void StreamSearchPage::addedToFavourites(const QString &name)
{
    view->showMessage(tr("Added '%1'' to favorites").arg(name), constMsgDisplayTime);
}

#include "moc_streamspage.cpp"
