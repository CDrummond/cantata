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

#include "streamspage.h"
#include "streamdialog.h"
#include "mpdconnection.h"
#include "messagebox.h"
#include "localize.h"
#include "icons.h"
#include "stdactions.h"
#include "actioncollection.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "streamsmodel.h"
#include "statuslabel.h"
#include "digitallyimported.h"
#include "digitallyimportedsettings.h"
#include <QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KFileDialog>
#else
#include <QFileDialog>
#include <QDir>
#endif
#include <QAction>
#include <QMenu>
#include <QFileInfo>

StreamsPage::StreamsPage(QWidget *p)
    : QWidget(p)
    , enabled(false)
    , searching(false)
{
    setupUi(this);
    importAction = ActionCollection::get()->createAction("importstreams", i18n("Import Streams Into Favourites"), "document-import");
    exportAction = ActionCollection::get()->createAction("exportstreams", i18n("Export Favourite Streams"), "document-export");
    addAction = ActionCollection::get()->createAction("addstream", i18n("Add New Stream To Favourites"), Icons::self()->addRadioStreamIcon);
    reloadAction = ActionCollection::get()->createAction("reloadstreams", i18n("Reload"), Icon("view-refresh"));
    editAction = ActionCollection::get()->createAction("editstream", i18n("Edit"), Icons::self()->editIcon);
    Action *settingsAct = new Action(i18n("Digitally Imported Settings"), this);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
    searchButton->setDefaultAction(StdActions::self()->searchAction);
    searchButton->setToolTip(i18n("Search TuneIn For Streams"));
    searchButton->setIcon(StreamsModel::self()->tuneInIcon());
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(searchIsActive(bool)), this, SLOT(controlSearch(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(searchModel.addToFavouritesAct(), SIGNAL(triggered(bool)), this, SLOT(addToFavourites()));
    connect(reloadAction, SIGNAL(triggered(bool)), this, SLOT(reload()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    connect(settingsAct, SIGNAL(triggered(bool)), this, SLOT(diSettings()));
    connect(StreamsModel::self(), SIGNAL(error(const QString &)), this, SIGNAL(error(const QString &)));
    connect(StreamsModel::self(), SIGNAL(loading()), view, SLOT(showSpinner()));
    connect(StreamsModel::self(), SIGNAL(loaded()), view, SLOT(hideSpinner()));
    connect(&searchModel, SIGNAL(loading()), view, SLOT(showSpinner()));
    connect(&searchModel, SIGNAL(loaded()), view, SLOT(hideSpinner()));
    connect(MPDConnection::self(), SIGNAL(dirChanged()), SLOT(mpdDirChanged()));
    connect(DigitallyImported::self(), SIGNAL(loginStatus(bool,QString)), SLOT(updateDiStatus()));
    QMenu *menu=new QMenu(this);
    menu->addAction(addAction);
    menu->addAction(StdActions::self()->removeAction);
    menu->addAction(editAction);
    menu->addAction(reloadAction);
    menu->addSeparator();
    menu->addAction(importAction);
    menu->addAction(exportAction);
    menu->addSeparator();
    menu->addAction(settingsAct);
    menuButton->setMenu(menu);
    Icon::init(replacePlayQueue);

    view->setUniformRowHeights(true);
    view->setAcceptDrops(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);
//     view->addAction(p->addToPlaylistAction);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addWithPriorityAction);
    view->addAction(editAction);
    view->addAction(StdActions::self()->removeAction);
    view->addAction(searchModel.addToFavouritesAct());
    view->addAction(reloadAction);
    proxy.setSourceModel(StreamsModel::self());
    view->setModel(&proxy);
    view->setDeleteAction(StdActions::self()->removeAction);

    infoLabel->hide();
    infoLabel->setType(StatusLabel::Locked);
    diStatusLabel->setText("DI");
    updateDiStatus();
    view->setSearchLabelText(i18n("Search TuneIn:"));
}

StreamsPage::~StreamsPage()
{
}

void StreamsPage::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;
}

void StreamsPage::mpdDirChanged()
{
    if (Settings::self()->storeStreamsInMpdDir()) {
        refresh();
    }
}

void StreamsPage::checkWritable()
{
    bool wasWriteable=StreamsModel::self()->isFavoritesWritable();
    bool nowWriteable=StreamsModel::self()->checkFavouritesWritable();

    if (nowWriteable) {
        infoLabel->hide();
    } else {
        infoLabel->setVisible(true);
        infoLabel->setText(StreamsModel::favouritesDir().startsWith("http:/") ? i18n("Streams from HTTP server") : i18n("Read only."));
    }
    if (wasWriteable!=nowWriteable) {
        controlActions();
    }
}

void StreamsPage::refresh()
{
    if (enabled) {
        checkWritable();
        view->setLevel(0);
        StreamsModel::self()->reloadFavourites();
        exportAction->setEnabled(StreamsModel::self()->rowCount()>0);
        view->expand(proxy.mapFromSource(StreamsModel::self()->favouritesIndex()));
    }
}

void StreamsPage::save()
{
    StreamsModel::self()->saveFavourites(true);
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

    QStringList files=searching ? searchModel.filenames(mapped, true) : StreamsModel::self()->filenames(mapped, true);

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

void StreamsPage::diSettings()
{
    DigitallyImportedSettings(this).show();
    updateDiStatus();
}

void StreamsPage::importXml()
{
    if (searching || !StreamsModel::self()->isFavoritesWritable()) {
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

    if (!StreamsModel::self()->importIntoFavourites(fileName)) {
        MessageBox::error(this, i18n("Failed to import <b>%1</b>!<br/>Please check this is of the correct type.").arg(fileName));
    }
}

void StreamsPage::exportXml()
{
    if (searching) {
        return;
    }

    QLatin1String ext(".xml");
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getSaveFileName(QLatin1String("Cantata")+ext, i18n("*.xml|XML Streams"), this, i18n("Export Streams"));
    #else
    QString fileName=QFileDialog::getSaveFileName(this, i18n("Export Streams"), QLatin1String("Cantata")+ext, i18n("XML Streams (*.xml)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!fileName.endsWith(ext)) {
        fileName+=ext;
    }

    if (!StreamsModel::self()->exportFavourites(fileName)) {
        MessageBox::error(this, i18n("Failed to create <b>%1</b>!").arg(fileName));
    }
}

void StreamsPage::add()
{
    if (searching || !StreamsModel::self()->isFavoritesWritable()) {
        return;
    }
    StreamDialog dlg(this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();
        QString existingNameForUrl=StreamsModel::self()->favouritesNameForUrl(url);

        if (!existingNameForUrl.isEmpty()) {
            MessageBox::error(this, i18n("Stream already exists!<br/><b>%1</b>").arg(existingNameForUrl));
        } else if (StreamsModel::self()->nameExistsInFavourites(name)) {
            MessageBox::error(this, i18n("A stream named <b>%1</b> already exists!").arg(name));
        } else {
            StreamsModel::self()->addToFavourites(url, name);
        }
    }
    exportAction->setEnabled(StreamsModel::self()->haveFavourites());
}

void StreamsPage::addToFavourites()
{
    if (!StreamsModel::self()->isFavoritesWritable()) {
        return;
    }

    QModelIndexList selected = view->selectedIndexes();

    QList<const StreamsModel::Item *> items;

    foreach (const QModelIndex &i, selected) {
        QModelIndex mapped=proxy.mapToSource(i);
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
        if (!item->isCategory() && item->parent && !item->parent->isFavourites) {
            items.append(item);
        }
    }

    foreach (const StreamsModel::Item *item, items) {
        StreamsModel::self()->addToFavourites(item->url, item->name);
    }
}

void StreamsPage::reload()
{
    if (searching) {
        return;
    }

    QModelIndexList selected = view->selectedIndexes();
    if (1!=selected.count()) {
        return;
    }

    QModelIndex mapped=proxy.mapToSource(selected.first());
    const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
    if (!item->isCategory()) {
        return;
    }
    const StreamsModel::CategoryItem *cat=static_cast<const StreamsModel::CategoryItem *>(item);
    if (!StreamsModel::self()->isTopLevel(cat)) {
        return;
    }
    if (cat->children.isEmpty() || MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Reload <b>%1</b> streams?").arg(cat->name))) {
        StreamsModel::self()->reload(mapped);
    }
}

void StreamsPage::removeItems()
{
    if (searching || !StreamsModel::self()->isFavoritesWritable()) {
        return;
    }

    QModelIndexList selected = view->selectedIndexes();
    QModelIndexList useable;

    foreach (const QModelIndex &i, selected) {
        QModelIndex mapped=proxy.mapToSource(i);
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(mapped.internalPointer());
        if (!item->isCategory() && item->parent && item->parent->isFavourites) {
            useable.append(mapped);
        }
    }

    if (useable.isEmpty()) {
        return;
    }

    if (useable.size()>1) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the %1 selected streams?").arg(useable.size()))) {
            return;
        }
    } else {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove <b>%1</b>?")
                                                 .arg(StreamsModel::self()->data(useable.first(), Qt::DisplayRole).toString()))) {
            return;
        }
    }

    foreach (const QModelIndex &i, useable) {
        StreamsModel::self()->removeFromFavourites(i);
    }

    exportAction->setEnabled(StreamsModel::self()->haveFavourites());
}

void StreamsPage::edit()
{
    if (searching || !StreamsModel::self()->isFavoritesWritable()) {
        return;
    }

    QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    QModelIndex index=proxy.mapToSource(selected.first());
    StreamsModel::Item *item=static_cast<StreamsModel::Item *>(index.internalPointer());
    if (item->isCategory() || !item->parent || !item->parent->isFavourites) {
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
            MessageBox::error(this, i18n("Stream already exists!<br/><b>%1</b>").arg(existingNameForUrl));
        } else if (newName!=name && StreamsModel::self()->nameExistsInFavourites(newName)) {
            MessageBox::error(this, i18n("A stream named <b>%1</b> already exists!").arg(newName));
        } else {
            item->name=newName;
            item->url=newUrl;
            StreamsModel::self()->updateFavouriteStream(item);
        }
    }
}

void StreamsPage::searchItems()
{
    if (!searching) {
        return;
    }
    searchModel.search(view->searchText().trimmed(), false);
}

void StreamsPage::controlSearch(bool on)
{
    if (on!=searching) {
        searching=on;
        if (!searching) {
            view->setModel(&proxy);
            proxy.setSourceModel(StreamsModel::self());
            searchModel.clear();
        } else {
            proxy.setSourceModel(&searchModel);
        }
        controlActions();
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    bool haveSelection=!selected.isEmpty();
    bool enableAddToFav=true;
    bool onlyStreamsSelected=true;
    bool favWriteable=StreamsModel::self()->isFavoritesWritable();

    if (searching) {
        foreach (const QModelIndex &idx, selected) {
            const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(idx).internalPointer());
            if (item->isCategory()) {
                enableAddToFav=false;
                onlyStreamsSelected=false;
                break;
            }
        }
        addAction->setEnabled(false);
        exportAction->setEnabled(false);
        importAction->setEnabled(false);
        reloadAction->setEnabled(false);
        StdActions::self()->removeAction->setEnabled(false);
        searchModel.addToFavouritesAct()->setEnabled(favWriteable && haveSelection && enableAddToFav);
    } else {
        editAction->setEnabled(false);
        reloadAction->setEnabled(false);

        bool enableRemove=true;
        foreach (const QModelIndex &idx, selected) {
            const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(idx).internalPointer());
            if (item->isCategory() || (item->parent && !item->parent->isFavourites)) {
                enableRemove=false;
            }
            if (item->isCategory() || (item->parent && item->parent->isFavourites)) {
                enableAddToFav=false;
            }
            if (item->isCategory()) {
                onlyStreamsSelected=false;
            }
            if (!enableRemove && !enableAddToFav && !onlyStreamsSelected) {
                break;
            }
        }

        StdActions::self()->removeAction->setEnabled(favWriteable && haveSelection && enableRemove);
        searchModel.addToFavouritesAct()->setEnabled(favWriteable && haveSelection && enableAddToFav);

        if (1==selected.size()) {
            const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(selected.first()).internalPointer());
            if (StreamsModel::self()->isFavoritesWritable() && !item->isCategory() && item->parent && item->parent->isFavourites) {
                editAction->setEnabled(true);
            }
            reloadAction->setEnabled(item->isCategory() && StreamsModel::self()->isTopLevel(static_cast<const StreamsModel::CategoryItem *>(item)));
        }

        addAction->setEnabled(favWriteable);
        exportAction->setEnabled(StreamsModel::self()->haveFavourites());
        importAction->setEnabled(favWriteable);
    }
    StdActions::self()->replacePlayQueueAction->setEnabled(haveSelection && onlyStreamsSelected);
    StdActions::self()->addWithPriorityAction->setEnabled(haveSelection && onlyStreamsSelected);
    menuButton->controlState();
}

void StreamsPage::updateDiStatus()
{
    if (DigitallyImported::self()->user().isEmpty() || DigitallyImported::self()->pass().isEmpty()) {
        diStatusLabel->setVisible(false);
    } else {
        bool loggedIn=DigitallyImported::self()->loggedIn();
        diStatusLabel->setVisible(true);
        diStatusLabel->setToolTip(loggedIn ? i18n("Logged into Digitally Imported") : i18n("<b>NOT</b> logged into Digitally Imported"));
        QString col=loggedIn
                        ? palette().highlight().color().name()
                        : palette().color(QPalette::Disabled, QPalette::WindowText).name();

        int margin=style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, this);
        if (margin<2) {
            margin=2;
        }
        diStatusLabel->setStyleSheet(QString("QLabel { color : %1; border-radius: %4px; border: 2px solid %2; margin: %3px}")
                                     .arg(col).arg(col).arg(margin).arg(margin*2));
    }
}
