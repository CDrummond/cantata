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
    , modelIsDownloading(false)
{
    setupUi(this);
    importAction = ActionCollection::get()->createAction("importstreams", i18n("Import Streams Into Favourites"), "document-import");
    exportAction = ActionCollection::get()->createAction("exportstreams", i18n("Export Favourite Streams"), "document-export");
    addAction = ActionCollection::get()->createAction("addstream", i18n("Add New Stream To Favourites"), Icons::self()->addRadioStreamIcon);
    addToFavouritesAction = ActionCollection::get()->createAction("addtofavourites", i18n("Add Stream To Favourites"), Icons::self()->addRadioStreamIcon);
    editAction = ActionCollection::get()->createAction("editstream", i18n("Edit"), Icons::self()->editIcon);
    Action *settingsAct = new Action(i18n("Digitally Imported Settings"), this);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(addToFavouritesAction, SIGNAL(triggered(bool)), this, SLOT(addToFavourites()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    connect(settingsAct, SIGNAL(triggered(bool)), this, SLOT(diSettings()));
    connect(StreamsModel::self(), SIGNAL(error(const QString &)), this, SIGNAL(error(const QString &)));
    connect(StreamsModel::self(), SIGNAL(loading()), view, SLOT(showSpinner()));
    connect(StreamsModel::self(), SIGNAL(loaded()), view, SLOT(hideSpinner()));
    connect(MPDConnection::self(), SIGNAL(dirChanged()), SLOT(mpdDirChanged()));
    connect(DigitallyImported::self(), SIGNAL(loginStatus(bool,QString)), SLOT(updateDiStatus()));
    QMenu *menu=new QMenu(this);
    menu->addAction(addAction);
    menu->addAction(StdActions::self()->removeAction);
    menu->addAction(editAction);
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
    view->addAction(addToFavouritesAction);
    proxy.setSourceModel(StreamsModel::self());
    view->setModel(&proxy);
    view->setDeleteAction(StdActions::self()->removeAction);

    infoLabel->hide();
    infoLabel->setType(StatusLabel::Locked);
    updateDiStatus();
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

void StreamsPage::diSettings()
{
    DigitallyImportedSettings(this).show();
    updateDiStatus();
}

void StreamsPage::importXml()
{
    if (!StreamsModel::self()->isFavoritesWritable()) {
        return;
    }
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), i18n("*.cantata|Cantata Streams\n*.xml|XML Streams"), this, i18n("Import Streams"));
    #else
    QString fileName=QFileDialog::getOpenFileName(this, i18n("Import Streams"), QDir::homePath(),
                                                  i18n("Cantata Streams (*.cantata)\nXML Streams (*.xml)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!StreamsModel::self()->importXml(fileName)) {
        MessageBox::error(this, i18n("Failed to import <b>%1</b>!<br/>Please check this is of the correct type.").arg(fileName));
    }
}

void StreamsPage::exportXml()
{
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

    if (!StreamsModel::self()->saveXml(fileName, QList<StreamsModel::Item *>())) {
        MessageBox::error(this, i18n("Failed to create <b>%1</b>!").arg(fileName));
    }
}

void StreamsPage::add()
{
    if (!StreamsModel::self()->isFavoritesWritable()) {
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

void StreamsPage::removeItems()
{
    if (!StreamsModel::self()->isFavoritesWritable()) {
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
    if (!StreamsModel::self()->isFavoritesWritable()) {
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
    QString text=view->searchText().trimmed();
    proxy.update(text, QString());
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll();
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    editAction->setEnabled(false);
    addToFavouritesAction->setEnabled(false);
    if (1==selected.size() && StreamsModel::self()->isFavoritesWritable()) {
        const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(selected.first()).internalPointer());
        if (!item->isCategory() && item->parent && item->parent->isFavourites) {
            editAction->setEnabled(true);
        }
    }
    StdActions::self()->removeAction->setEnabled(false);
    if (!selected.isEmpty() && StreamsModel::self()->isFavoritesWritable()) {
        bool enableRemove=true;
        bool enableAddToFav=true;
        foreach (const QModelIndex &idx, selected) {
            const StreamsModel::Item *item=static_cast<const StreamsModel::Item *>(proxy.mapToSource(idx).internalPointer());
            if (item->isCategory() || (item->parent && !item->parent->isFavourites)) {
                enableRemove=false;
                break;
            }
            if (item->isCategory() || (item->parent && item->parent->isFavourites)) {
                enableAddToFav=false;
                break;
            }
            if (!enableRemove && !enableAddToFav) {
                break;
            }
        }
        StdActions::self()->removeAction->setEnabled(enableRemove);
        addToFavouritesAction->setEnabled(enableAddToFav);
    }
    addAction->setEnabled(StreamsModel::self()->isFavoritesWritable());
    exportAction->setEnabled(StreamsModel::self()->haveFavourites());
    importAction->setEnabled(StreamsModel::self()->isFavoritesWritable());
    StdActions::self()->replacePlayQueueAction->setEnabled(selected.count());
    StdActions::self()->addWithPriorityAction->setEnabled(selected.count());
    menuButton->controlState();
}

void StreamsPage::updateDiStatus()
{
    if (DigitallyImported::self()->user().isEmpty() || DigitallyImported::self()->pass().isEmpty()) {
        diStatusLabel->setVisible(false);
    } else {
        diStatusLabel->setVisible(true);
        diStatusLabel->setText(DigitallyImported::self()->loggedIn() ? i18n("DI: Logged In") : i18n("DI: Not Logged In"));
    }
}
