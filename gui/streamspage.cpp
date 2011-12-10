/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QtCore/QFile>
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
    removeAction->setText(i18n("Remove Stream"));
    editAction = p->actionCollection()->addAction("editstream");
    editAction->setText(i18n("Edit Stream"));
    #else
    importAction = new QAction(tr("Import Streams"), this);
    exportAction = new QAction(tr("Export Streams"), this);
    addAction = new QAction(tr("Add Stream"), this);
    removeAction = new QAction(tr("Remove Stream"), this);
    editAction = new QAction(tr("Rename Stream"), this);
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
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), removeStream, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), editStream, SLOT(setEnabled(bool)));
    connect(search, SIGNAL(returnPressed()), this, SLOT(searchItems()));
    connect(search, SIGNAL(textChanged(const QString)), this, SLOT(searchItems()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(removeAction, SIGNAL(triggered(bool)), this, SLOT(remove()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlEdit()));
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
    search->setPlaceholderText(i18n("Search streams..."));
#else
    search->setPlaceholderText(tr("Search streams..."));
#endif
    view->setPageDefaults();
//     view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(removeAction);
    view->addAction(editAction);
    view->addAction(removeAction);
    view->addAction(exportAction);
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
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
    model.save();
}

void StreamsPage::addSelectionToPlaylist()
{
    QStringList streams;

    const QModelIndexList selected = view->selectionModel()->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    foreach (const QModelIndex &idx, selected) {
        QString stream=model.data(proxy.mapToSource(idx), Qt::ToolTipRole).toString();

        if (!streams.contains(stream)) {
            streams << stream;
        }
    }

    if (!streams.isEmpty()) {
        emit add(streams);
        view->selectionModel()->clearSelection();
    }
}

void StreamsPage::importXml()
{
#ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), i18n("*.streams|Cantata Streams\n*.js|Amarok Script"), this, i18n("Import Streams"));
#else
    QString fileName=QFileDialog::getOpenFileName(this, tr("Import Streams"), QDir::homePath(), tr("Cantata Streams (*.streams);;Amarok Script (*.js)"));
#endif

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        #ifdef ENABLE_KDE_SUPPORT
        KMessageBox::error(this, i18n("Failed to open <i>%1</i>!", fileName));
        #else
        QMessageBox::critical(this, tr("Error"), tr("Failed to open <i>%1</i>!").arg(fileName));
        #endif
        return;
    }

    QString xml;
    QTextStream stream(&file);

    while (!stream.atEnd()) {
        xml+=stream.readLine()+'\n';
    }

//     int before=model.rowCount();
//     if (model.importXml(xml)) {
//         int now=model.rowCount();
//         if (now==before) {
//             #ifdef ENABLE_KDE_SUPPORT
//             KMessageBox::information(this, i18n("No new streams imported."));
//             #else
//             QMessageBox::information(this, tr("Information"), tr("No new streams imported."));
//             #endif
//         } else if (now==(before+1)) {
//             #ifdef ENABLE_KDE_SUPPORT
//             KMessageBox::information(this, i18n("1 new stream imported."));
//             #else
//             QMessageBox::information(this, tr("Information"), tr("1 new stream imported."));
//             #endif
//         } else if (now>before) {
//             #ifdef ENABLE_KDE_SUPPORT
//             KMessageBox::information(this, i18n("%1 new streams imported.").arg(now-before));
//             #else
//             QMessageBox::information(this, tr("Information"), tr("%1 new streams imported.").arg(now-before));
//             #endif
//         }
//     } else {

    if (!model.import(xml)) {
        #ifdef ENABLE_KDE_SUPPORT
        KMessageBox::error(this, i18n("Failed to import <i>%1</i>!<br/>Please check this is of the correct type.", fileName));
        #else
        QMessageBox::critical(this, tr("Error"), tr("Failed to import <i>%1</i>!<br/>Please check this is of the correct type.").arg(fileName));
        #endif
    }
}

void StreamsPage::exportXml()
{
#ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getSaveFileName(KUrl(), i18n("*.streams|Cantata Streams"), this, i18n("Export Streams"));
#else
    QString fileName=QFileDialog::getSaveFileName(this, tr("Export Streams"), QDir::homePath(), tr("Cantata Streams (*.streams)"));
#endif

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        #ifdef ENABLE_KDE_SUPPORT
        KMessageBox::error(this, i18n("Failed to create <i>%1</i>!", fileName));
        #else
        QMessageBox::critical(this, tr("Error"), tr("Failed to create <i>%1</i>!").arg(fileName));
        #endif
        return;
    }

    QTextStream(&file) << model.toXml();
}

void StreamsPage::add()
{
    StreamDialog dlg(this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();

        QString existing=model.name(url);
        if (!existing.isEmpty()) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("Stream already exists!<br/><i>%1</i>", existing));
            #else
            QMessageBox::critical(this, tr("Error"), tr("Stream already exists!<br/><i>%1</i>").arg(existing));
            #endif
            return;
        }

        if (!model.add(name, url)) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("A stream named <i>%1</i> already exists!", name));
            #else
            QMessageBox::critical(this, tr("Error"), tr("A stream named <i>%1</i> already exists!").arg(name));
            #endif
        }
    }
    exportAction->setEnabled(model.rowCount()>0);
}

void StreamsPage::remove()
{
    QStringList streams;

    const QModelIndexList selected = view->selectionModel()->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndex firstIndex=proxy.mapToSource(selected.first());
    QString firstName=model.data(firstIndex, Qt::DisplayRole).toString();
    #ifdef ENABLE_KDE_SUPPORT
    if (KMessageBox::No==KMessageBox::warningYesNo(this, selected.size()>1 ? i18n("Are you sure you wish to remove the %1 selected streams?").arg(selected.size())
                                                                           : i18n("Are you sure you wish to remove <i>%1</i>?").arg(firstName),
                                                   selected.size()>1 ? i18n("Remove Streams?") : i18n("Remove Stream?"))) {
        return;
    }
    #else
    if (QMessageBox::No==QMessageBox::warning(this, selected.size()>1 ? tr("Remove Streams?") : tr("Remove Stream?"),
                                              selected.size()>1 ? tr("Are you sure you wish to remove the %1 selected streams?").arg(selected.size())
                                                                : tr("Are you sure you wish to remove <i>%1</i>?").arg(firstName),
                                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
        return;
    }
    #endif

    foreach (const QModelIndex &idx, selected) {
        model.remove(proxy.mapToSource(idx));
    }
    exportAction->setEnabled(model.rowCount()>0);
}

void StreamsPage::edit()
{
    QStringList streams;

    const QModelIndexList selected = view->selectionModel()->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    StreamDialog dlg(this);
    QModelIndex index=proxy.mapToSource(selected.first());
    QString name=model.data(index, Qt::DisplayRole).toString();
    QString url=model.data(index, Qt::ToolTipRole).toString();

    dlg.setEdit(name, url);

    if (QDialog::Accepted==dlg.exec()) {
        QString newName=dlg.name();
        QString newUrl=dlg.url();

        QString existingNameForUrl=newUrl!=url ? model.name(newUrl) : QString();

        if (!existingNameForUrl.isEmpty()) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("Stream already exists!<br/><i>%1</i>", existingNameForUrl));
            #else
            QMessageBox::critical(this, tr("Error"), tr("Stream already exists!<br/><i>%1</i>").arg(existingNameForUrl));
            #endif
        } else if (newName!=name && model.entryExists(newName)) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("A stream named <i>%1</i> already exists!", newName));
            #else
            QMessageBox::critical(this, tr("Error"), tr("A stream named <i>%1</i> already exists!").arg(newName));
            #endif
        } else {
            model.edit(index, newName, newUrl);
        }
    }
}

void StreamsPage::controlEdit()
{
    editAction->setEnabled(1==view->selectionModel()->selectedIndexes().size());
}

void StreamsPage::searchItems()
{
    proxy.setFilterRegExp(search->text());
}
