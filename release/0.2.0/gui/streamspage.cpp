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
#include "mpdconnection.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtGui/QInputDialog>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KMessageBox>
#include <KDE/KFileDialog>
#else
#include <QtGui/QAction>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <QtCore/QDir>
#endif

StreamsPage::StreamsPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    importAction = p->actionCollection()->addAction("importstreams");
    importAction->setText(i18n("Import Streams"));
    exportAction = p->actionCollection()->addAction("exportstreams");
    exportAction->setText(i18n("Export Streams"));
    addAction = p->actionCollection()->addAction("addstream");
    addAction->setText(i18n("Add Stream"));
    removeAction = p->actionCollection()->addAction("removestream");
    removeAction->setText(i18n("Remove Stream/Category"));
    editAction = p->actionCollection()->addAction("editstream");
    editAction->setText(i18n("Edit Stream/Category"));
    #else
    importAction = new QAction(tr("Import Streams"), this);
    exportAction = new QAction(tr("Export Streams"), this);
    addAction = new QAction(tr("Add Stream"), this);
    removeAction = new QAction(tr("Remove Stream/Category"), this);
    editAction = new QAction(tr("Edit Stream/Category"), this);
    #endif
    importAction->setIcon(QIcon::fromTheme("document-import"));
    exportAction->setIcon(QIcon::fromTheme("document-export"));
    addAction->setIcon(QIcon::fromTheme("list-add"));
    removeAction->setIcon(QIcon::fromTheme("list-remove"));
    editAction->setIcon(QIcon::fromTheme("document-edit"));
    importStreams->setDefaultAction(importAction);
    exportStreams->setDefaultAction(exportAction);
    addStream->setDefaultAction(addAction);
    removeStream->setDefaultAction(removeAction);
    editStream->setDefaultAction(editAction);
//     addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(removeAction, SIGNAL(triggered(bool)), this, SLOT(remove()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    importStreams->setAutoRaise(true);
    exportStreams->setAutoRaise(true);
    addStream->setAutoRaise(true);
    removeStream->setAutoRaise(true);
    editStream->setAutoRaise(true);
//     addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    removeStream->setEnabled(false);
    editStream->setEnabled(false);
//     addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

    #ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Streams"));
    #else
    view->setTopText(tr("Streams"));
    #endif
//     view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(removeAction);
    view->addAction(editAction);
    view->addAction(removeAction);
    view->addAction(exportAction);
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    view->setDeleteAction(removeAction);
    view->init(p->replacePlaylistAction, 0);
}

StreamsPage::~StreamsPage()
{
}

void StreamsPage::refresh()
{
    model.reload();
    exportAction->setEnabled(model.rowCount()>0);
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
    QModelIndexList indexes;
    indexes.append(index);
    addItemsToPlayQueue(indexes);
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

    if (selected.isEmpty()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    QString name;

    if (1==mapped.count()) {
        name=static_cast<StreamsModel::StreamItem*>(mapped.first().internalPointer())->name+QLatin1String(".cantata");
    }
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getSaveFileName(name, i18n("*.cantata|Cantata Streams"), this, i18n("Export Streams"));
    #else
    QString fileName=QFileDialog::getSaveFileName(this, tr("Export Streams"), name, tr("Cantata Streams (*.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!model.save(fileName, mapped)) {
        #ifdef ENABLE_KDE_SUPPORT
        KMessageBox::error(this, i18n("Failed to create <b>%1</b>!", fileName));
        #else
        QMessageBox::critical(this, tr("Error"), tr("Failed to create <b>%1</b>!").arg(fileName));
        #endif
    }
}

void StreamsPage::add()
{
    StreamDialog dlg(getCategories(), this);

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

        if (!model.add(cat, name, url)) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("A stream named <b>%1</b> already exists!", name));
            #else
            QMessageBox::critical(this, tr("Error"), tr("A stream named <b>%1</b> already exists!").arg(name));
            #endif
        }
    }
    exportAction->setEnabled(model.rowCount()>0);
}

void StreamsPage::remove()
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
    QSet<StreamsModel::Item *> selectedCategories;
    QModelIndexList remove;
    foreach(QModelIndex index, selected) {
        QModelIndex idx=proxy.mapToSource(index);
        StreamsModel::Item *item=static_cast<StreamsModel::Item *>(idx.internalPointer());

        if (item->isCategory()) {
            selectedCategories.insert(item);
            remove.append(idx);
        } else if (!selectedCategories.contains(static_cast<StreamsModel::StreamItem*>(item)->parent)) {
            remove.append(idx);
        }
    }

    foreach (const QModelIndex &idx, remove) {
        model.remove(idx);
    }
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

    if (item->isCategory()) {
        for(;;) {
            #ifdef ENABLE_KDE_SUPPORT
            QString name = QInputDialog::getText(this, i18n("Category Name"), i18n("Enter a name for the category:"));
            #else
            QString name = QInputDialog::getText(this, tr("Category Name"), tr("Enter a name for the category:"));
            #endif

            if (name.isEmpty()) {
                break;
            }

            if (getCategories().contains(name)) {
                #ifdef ENABLE_KDE_SUPPORT
                KMessageBox::error(this, i18n("A category named <b>%1</b> already exists!").arg(name));
                #else
                QMessageBox::critical(this, tr("Error"), tr("A category named <b>%1</b> already exists!").arg(name));
                #endif
            } else {
                model.editCategory(index, name);
                break;
            }
        }
        return;
    }

    StreamDialog dlg(getCategories(), this);
    StreamsModel::StreamItem *stream=static_cast<StreamsModel::StreamItem *>(item);
    QString name=stream->name;
    QString url=stream->url.toString();
    QString cat=stream->parent->name;

    dlg.setEdit(cat, name, url);

    if (QDialog::Accepted==dlg.exec()) {
        QString newName=dlg.name();
        QString newUrl=dlg.url();
        QString newCat=dlg.category();
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
            model.editStream(index, cat, newCat, newName, newUrl);
        }
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    editAction->setEnabled(1==selected.size());
    replacePlaylist->setEnabled(selected.count());
    removeStream->setEnabled(selected.count());
    editStream->setEnabled(1==selected.size());

    bool enableExport=selected.size()>0;
    if (selected.size()) {
        foreach (const QModelIndex &idx, selected) {
            QModelIndex index=proxy.mapToSource(idx);
            if(!static_cast<StreamsModel::Item *>(index.internalPointer())->isCategory()) {
                enableExport=false;
                break;
            }
        }
    }
    exportAction->setEnabled(enableExport);
}

void StreamsPage::searchItems()
{
    proxy.setFilterRegExp(view->searchText());
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
