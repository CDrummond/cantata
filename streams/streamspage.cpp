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
#include "streamcategorydialog.h"
#include "mpdconnection.h"
#include "messagebox.h"
#include "localize.h"
#include "icons.h"
#include "mainwindow.h"
#include "action.h"
#include "actioncollection.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "streamsmodel.h"
#include "webstreams.h"
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

static const char * constUrlProperty("url");

StreamsPage::StreamsPage(MainWindow *p)
    : QWidget(p)
    , enabled(false)
    , modelIsDownloading(false)
    , mw(p)
{
    setupUi(this);
    importAction = ActionCollection::get()->createAction("importstreams", i18n("Import Streams"), "document-import");
    exportAction = ActionCollection::get()->createAction("exportstreams", i18n("Export Streams"), "document-export");
    addAction = ActionCollection::get()->createAction("addstream", i18n("Add Stream"), "list-add");
    editAction = ActionCollection::get()->createAction("editstream", i18n("Edit"), Icons::editIcon);

    QMenu *importMenu=new QMenu(this);
    importFileAction=importMenu->addAction("From Cantata File");
    QList<WebStream *> webStreams=WebStream::getAll();
    QAction *radioAction=0;
    QMenu *radioMenu=0;
    QMap<QString, QMenu *> regions;

    foreach (const WebStream *ws, webStreams) {
        if (dynamic_cast<const RadioWebStream *>(ws)) {
            if (!radioAction) {
                radioAction=importMenu->addAction(i18n("Radio Stations"));
                radioMenu=new QMenu(this);
                radioAction->setMenu(radioMenu);
            }
            QMenu *menu=radioMenu;
            if (!ws->getRegion().isEmpty()) {
                if (!regions.contains(ws->getRegion())) {
                    QAction *regionAction=radioMenu->addAction(ws->getRegion());
                    regions.insert(ws->getRegion(), new QMenu(this));
                    regionAction->setMenu(regions[ws->getRegion()]);
                }
                menu=regions[ws->getRegion()];
            }
            QAction *act=menu->addAction(ws->getName());
            act->setProperty(constUrlProperty, ws->getUrl());
            connect(act, SIGNAL(triggered(bool)), this, SLOT(importWebStreams()));
        } else {
            QAction *act=importMenu->addAction(i18n("From %1").arg(ws->getName()));
            act->setProperty(constUrlProperty, ws->getUrl());
            connect(act, SIGNAL(triggered(bool)), this, SLOT(importWebStreams()));
        }
        connect(ws, SIGNAL(error(const QString &)), mw, SLOT(showError(QString)));
        connect(ws, SIGNAL(finished()), this, SLOT(checkIfBusy()));
    }

    importAction->setMenu(importMenu);
    replacePlayQueue->setDefaultAction(p->replacePlayQueueAction);
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importFileAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(StreamsModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    connect(StreamsModel::self(), SIGNAL(error(const QString &)), mw, SLOT(showError(QString)));
    connect(StreamsModel::self(), SIGNAL(downloading(bool)), this, SLOT(downloading(bool)));
    connect(MPDConnection::self(), SIGNAL(dirChanged()), SLOT(mpdDirChanged()));
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
    view->setUniformRowHeights(true);
    view->setAcceptDrops(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);
//     view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlayQueueAction);
    view->addAction(p->addWithPriorityAction);
    view->addAction(editAction);
    view->addAction(exportAction);
    view->addAction(p->removeAction);
    proxy.setSourceModel(StreamsModel::self());
    view->setModel(&proxy);
    view->setDeleteAction(p->removeAction);
    view->init(p->replacePlayQueueAction, 0);

    infoLabel->hide();
    infoIcon->hide();
    int iconSize=Icon::stdSize(QApplication::fontMetrics().height());
    infoIcon->setPixmap(Icon("locked").pixmap(iconSize, iconSize));
}

StreamsPage::~StreamsPage()
{
    foreach (WebStream *ws, WebStream::getAll()) {
        ws->cancelDownload();
    }
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

void StreamsPage::checkWriteable()
{
    bool isHttp=StreamsModel::dir().startsWith("http:/");
    bool dirWritable=!isHttp && QFileInfo(StreamsModel::dir()).isWritable();
    if (dirWritable) {
        infoLabel->hide();
        infoIcon->hide();
    } else {
        infoLabel->setVisible(true);
        infoLabel->setText(isHttp ? i18n("Streams from HTTP server") : i18n("Music folder not writeable."));
        infoIcon->setVisible(true);
    }
    if (dirWritable!=StreamsModel::self()->isWritable()) {
        StreamsModel::self()->setWritable(dirWritable);
        controlActions();
    }
}

void StreamsPage::refresh()
{
    if (enabled) {
        checkWriteable();
        view->setLevel(0);
        StreamsModel::self()->reload();
        exportAction->setEnabled(StreamsModel::self()->rowCount()>0);
    }
}

void StreamsPage::save()
{
    StreamsModel::self()->save(true);
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

void StreamsPage::downloading(bool dl)
{
    modelIsDownloading=dl;
    checkIfBusy();
}

void StreamsPage::checkIfBusy()
{
    bool busy=modelIsDownloading;
    if (!busy) {
        foreach (WebStream *ws, WebStream::getAll()) {
            if (ws->isDownloading()) {
                busy=true;
                break;
            }
        }
    }
    if (busy) {
        view->showSpinner();
    } else {
        view->hideSpinner();
    }
}

void StreamsPage::importXml()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), i18n("*.cantata|Cantata Streams"), this, i18n("Import Streams"));
    #else
    QString fileName=QFileDialog::getOpenFileName(this, i18n("Import Streams"), QDir::homePath(), i18n("Cantata Streams (*.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!StreamsModel::self()->import(fileName)) {
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

    if (!StreamsModel::self()->save(fileName, categories+items)) {
        MessageBox::error(this, i18n("Failed to create <b>%1</b>!").arg(fileName));
    }
}

void StreamsPage::add()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }
    StreamDialog dlg(getCategories(), getGenres(), this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();
        QString cat=dlg.category();
        QString existing=StreamsModel::self()->name(cat, url);

        if (!existing.isEmpty()) {
            MessageBox::error(this, i18n("Stream already exists!<br/><b>%1</b>").arg(existing));
            return;
        }

        if (!StreamsModel::self()->add(cat, name, dlg.genre(), dlg.icon(), url)) {
            MessageBox::error(this, i18n("A stream named <b>%1</b> already exists!").arg(name));
        }
    }
    exportAction->setEnabled(StreamsModel::self()->rowCount()>0);
}

void StreamsPage::removeItems()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }
    QStringList streams;
    QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    bool haveStreams=false;
    bool haveCategories=false;
    foreach(const QModelIndex &index, selected) {
        if (static_cast<StreamsModel::Item *>(proxy.mapToSource(index).internalPointer())->isCategory()) {
            haveCategories=true;
        } else {
             haveStreams=true;
        }

        if (haveStreams && haveCategories) {
            break;
        }
    }

    QModelIndex firstIndex=proxy.mapToSource(selected.first());
    QString firstName=StreamsModel::self()->data(firstIndex, Qt::DisplayRole).toString();
    QString message;

    if (selected.size()>1) {
        if (haveStreams && haveCategories) {
            message=i18n("Are you sure you wish to remove the selected categories &streams?");
        } else if (haveStreams) {
            message=i18n("Are you sure you wish to remove the %1 selected streams?").arg(selected.size());
        } else {
            message=i18n("Are you sure you wish to remove the %1 selected categories (and their streams)?").arg(selected.size());
        }
    } else if (haveStreams) {
        message=i18n("Are you sure you wish to remove <b>%1</b>?").arg(firstName);
    } else {
        message=i18n("Are you sure you wish to remove the <b>%1</b> category (and its streams)?").arg(firstName);
    }

    if (MessageBox::No==MessageBox::warningYesNo(this, message, i18n("Remove?"))) {
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
        StreamsModel::self()->removeCategory(i);
    }

    foreach (StreamsModel::StreamItem *i, removeStreams) {
        StreamsModel::self()->removeStream(i);
    }
    StreamsModel::self()->updateGenres();
    exportAction->setEnabled(StreamsModel::self()->rowCount()>0);
}

void StreamsPage::edit()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }
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
            StreamsModel::self()->editCategory(index, dlg.name(), dlg.icon());
        }
        return;
    }

    StreamDialog dlg(getCategories(), getGenres(), this);
    StreamsModel::StreamItem *stream=static_cast<StreamsModel::StreamItem *>(item);
    QString url=stream->url.toString();
    QString cat=stream->parent->name;
    QString genre=stream->genreString();

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
        QString existingNameForUrl=newUrl!=url ? StreamsModel::self()->name(newCat, newUrl) : QString();
//
        if (!existingNameForUrl.isEmpty()) {
            MessageBox::error(this, i18n("Stream already exists!<br/><b>%1 (%2)</b>").arg(existingNameForUrl).arg(newCat));
        } else if (newName!=name && StreamsModel::self()->entryExists(newCat, newName)) {
            MessageBox::error(this, i18n("A stream named <b>%1 (%2)</b> already exists!").arg(newName).arg(newCat));
        } else {
            StreamsModel::self()->editStream(index, cat, newCat, newName, newGenre, newIcon, newUrl);
        }
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    editAction->setEnabled(1==selected.size() && StreamsModel::self()->isWritable());
    mw->removeAction->setEnabled(selected.count() && StreamsModel::self()->isWritable());
    addAction->setEnabled(StreamsModel::self()->isWritable());
    exportAction->setEnabled(StreamsModel::self()->rowCount());
    importAction->setEnabled(StreamsModel::self()->isWritable());
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
    for(int i=0; i<StreamsModel::self()->rowCount(); ++i) {
        QModelIndex idx=StreamsModel::self()->index(i, 0, QModelIndex());
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

void StreamsPage::importWebStreams()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }

    QObject *obj=qobject_cast<QObject *>(sender());
    if (!obj) {
        return;
    }

    QUrl url=obj->property(constUrlProperty).toUrl();
    if (!url.isValid()) {
        return;
    }

    WebStream *ws=WebStream::get(url);
    if (!ws) {
        return;
    }

    if (ws->isDownloading()) {
        MessageBox::error(this, i18n("Download from %1 is already in progress!").arg(ws->getName()));
        return;
    }

    if (getCategories().contains(ws->getName())) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Update streams from %1?\n(This will replace any existing streams in this category)").arg(ws->getName()),
                                                     i18n("Update %1").arg(ws->getName()))) {
            return;
        }
    } else {
        if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Download streams from %1?").arg(ws->getName()), i18n("Download %1").arg(ws->getName()))) {
            return;
        }
    }
    ws->download();
    view->showSpinner();
}
