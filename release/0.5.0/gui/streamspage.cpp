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
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KMessageBox>
#include <KDE/KFileDialog>
#include <KDE/KInputDialog>
#else
#include <QtGui/QInputDialog>
#include <QtGui/QAction>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QtCore/QDir>
#endif

StreamsPage::StreamsPage(MainWindow *p)
    : QWidget(p)
    , enabled(false)
    , mw(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    importAction = p->actionCollection()->addAction("importstreams");
    importAction->setText(i18n("Import Streams"));
    exportAction = p->actionCollection()->addAction("exportstreams");
    exportAction->setText(i18n("Export Streams"));
    addAction = p->actionCollection()->addAction("addstream");
    addAction->setText(i18n("Add Stream"));
    editAction = p->actionCollection()->addAction("editstream");
    editAction->setText(i18n("Edit"));
    #else
    importAction = new QAction(tr("Import Streams"), this);
    exportAction = new QAction(tr("Export Streams"), this);
    addAction = new QAction(tr("Add Stream"), this);
    editAction = new QAction(tr("Edit"), this);
    #endif
    importAction->setIcon(QIcon::fromTheme("document-import"));
    exportAction->setIcon(QIcon::fromTheme("document-export"));
    addAction->setIcon(QIcon::fromTheme("list-add"));
    editAction->setIcon(QIcon::fromTheme("document-edit"));
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(&model, SIGNAL(updateGenres(const QSet<QString> &)), SLOT(updateGenres(const QSet<QString> &)));
    MainWindow::initButton(menuButton);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu=new QMenu(this);
    menu->addAction(addAction);
    menu->addAction(p->removeAction);
    menu->addAction(editAction);
    menu->addAction(importAction);
    menu->addAction(exportAction);
    menuButton->setMenu(menu);
    menuButton->setIcon(QIcon::fromTheme("system-run"));
    MainWindow::initButton(replacePlaylist);
    replacePlaylist->setEnabled(false);

    #ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Streams"));
    #else
    view->setTopText(tr("Streams"));
    #endif
//     view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(editAction);
    view->addAction(exportAction);
    view->addAction(p->removeAction);
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    view->setDeleteAction(p->removeAction);
    view->init(p->replacePlaylistAction, 0);
    updateGenres(QSet<QString>());
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

    if (enabled) {
        refresh();
    }
}

void StreamsPage::refresh()
{
    if (enabled) {
        model.reload();
        exportAction->setEnabled(model.rowCount()>0);
    }
}

void StreamsPage::save()
{
    model.save(true);
}

void StreamsPage::addSelectionToPlaylist()
{
    addItemsToPlayQueue(view->selectedIndexes());
}

void StreamsPage::addItemsToPlayQueue(const QModelIndexList &indexes)
{
    if (0==indexes.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, indexes) {
        mapped.append(proxy.mapToSource(idx));
    }

    QStringList files=model.filenames(mapped);

    if (!files.isEmpty()) {
        emit add(files);
        view->clearSelection();
    }
}

void StreamsPage::itemDoubleClicked(const QModelIndex &index)
{
    if (!static_cast<StreamsModel::Item *>(proxy.mapToSource(index).internalPointer())->isCategory()) {
        QModelIndexList indexes;
        indexes.append(index);
        addItemsToPlayQueue(indexes);
    }
}

void StreamsPage::updateGenres(const QSet<QString> &g)
{
    if (genreCombo->count() && g==genres) {
        return;
    }

    genres=g;
    QStringList entries=g.toList();
    qSort(entries);
    #ifdef ENABLE_KDE_SUPPORT
    entries.prepend(i18n("All Genres"));
    #else
    entries.prepend(tr("All Genres"));
    #endif

    QString currentFilter = genreCombo->currentIndex() ? genreCombo->currentText() : QString();

    genreCombo->clear();
    genreCombo->addItems(entries);
    if (0==genres.count()) {
        genreCombo->setCurrentIndex(0);
    } else {
        if (!currentFilter.isEmpty()) {
            bool found=false;
            for (int i=1; i<genreCombo->count() && !found; ++i) {
                if (genreCombo->itemText(i) == currentFilter) {
                    genreCombo->setCurrentIndex(i);
                    found=true;
                }
            }
            if (!found) {
                genreCombo->setCurrentIndex(0);
            }
        }
    }
}

void StreamsPage::importXml()
{
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), i18n("*.cantata|Cantata Streams"), this, i18n("Import Streams"));
    #else
    QString fileName=QFileDialog::getOpenFileName(this, tr("Import Streams"), QDir::homePath(), tr("Cantata Streams (*.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!model.import(fileName)) {
        #ifdef ENABLE_KDE_SUPPORT
        KMessageBox::error(this, i18n("Failed to import <b>%1</b>!<br/>Please check this is of the correct type.", fileName));
        #else
        QMessageBox::critical(this, tr("Error"), tr("Failed to import <b>%1</b>!<br/>Please check this is of the correct type.").arg(fileName));
        #endif
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
    QString fileName=QFileDialog::getSaveFileName(this, tr("Export Streams"), name, tr("Cantata Streams (*.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!model.save(fileName, categories+items)) {
        #ifdef ENABLE_KDE_SUPPORT
        KMessageBox::error(this, i18n("Failed to create <b>%1</b>!", fileName));
        #else
        QMessageBox::critical(this, tr("Error"), tr("Failed to create <b>%1</b>!").arg(fileName));
        #endif
    }
}

void StreamsPage::add()
{
    StreamDialog dlg(getCategories(), getGenres(), model.urlHandlers(), this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();
        QString cat=dlg.category();
        QString existing=model.name(cat, url);

        if (!existing.isEmpty()) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("Stream already exists!<br/><b>%1</b>", existing));
            #else
            QMessageBox::critical(this, tr("Error"), tr("Stream already exists!<br/><b>%1</b>").arg(existing));
            #endif
            return;
        }

        if (!model.add(cat, name, dlg.genre(), dlg.icon(), url)) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("A stream named <b>%1</b> already exists!", name));
            #else
            QMessageBox::critical(this, tr("Error"), tr("A stream named <b>%1</b> already exists!").arg(name));
            #endif
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
    #ifdef ENABLE_KDE_SUPPORT
    if (KMessageBox::No==KMessageBox::warningYesNo(this, selected.size()>1 ? i18n("Are you sure you wish to remove the %1 selected streams?").arg(selected.size())
                                                                           : i18n("Are you sure you wish to remove <b>%1</b>?").arg(firstName),
                                                   selected.size()>1 ? i18n("Remove Streams?") : i18n("Remove Stream?"))) {
        return;
    }
    #else
    if (QMessageBox::No==QMessageBox::warning(this, selected.size()>1 ? tr("Remove Streams?") : tr("Remove Stream?"),
                                              selected.size()>1 ? tr("Are you sure you wish to remove the %1 selected streams?").arg(selected.size())
                                                                : tr("Are you sure you wish to remove <b>%1</b>?").arg(firstName),
                                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
        return;
    }
    #endif

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

    StreamDialog dlg(getCategories(), getGenres(), model.urlHandlers(), this);
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
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("Stream already exists!<br/><b>%1 (%2)</b>", existingNameForUrl, newCat));
            #else
            QMessageBox::critical(this, tr("Error"), tr("Stream already exists!<br/><b>%1 (%2)</b>").arg(existingNameForUrl).arg(newCat));
            #endif
        } else if (newName!=name && model.entryExists(newCat, newName)) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("A stream named <b>%1 (%2)</b> already exists!", newName, newCat));
            #else
            QMessageBox::critical(this, tr("Error"), tr("A stream named <b>%1 (%2)</b> already exists!").arg(newName).arg(newCat));
            #endif
        } else {
            model.editStream(index, cat, newCat, newName, newGenre, newIcon, newUrl);
        }
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    editAction->setEnabled(1==selected.size());
    replacePlaylist->setEnabled(selected.count());
    mw->removeAction->setEnabled(selected.count());
    exportAction->setEnabled(model.rowCount());
    mw->replacePlaylistAction->setEnabled(selected.count());
}

void StreamsPage::searchItems()
{
    QString genre=0==genreCombo->currentIndex() ? QString() : genreCombo->currentText();
    QString filter=view->searchText().trimmed();

    if (filter.isEmpty() && genre.isEmpty()) {
        proxy.setFilterEnabled(false);
        proxy.setFilterGenre(genre);
        if (!proxy.filterRegExp().isEmpty()) {
             proxy.setFilterRegExp(QString());
        } else {
            proxy.invalidate();
        }
    } else {
        proxy.setFilterEnabled(true);
        proxy.setFilterGenre(genre);
        if (filter!=proxy.filterRegExp().pattern()) {
            proxy.setFilterRegExp(filter);
        } else {
            proxy.invalidate();
        }
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
        #ifdef ENABLE_KDE_SUPPORT
        categories.append(i18n("General"));
        #else
        categories.append(tr("General"));
        #endif
    }

    qSort(categories);
    return categories;
}

QStringList StreamsPage::getGenres()
{
    QStringList g=genres.toList();
    qSort(g);
    return g;
}