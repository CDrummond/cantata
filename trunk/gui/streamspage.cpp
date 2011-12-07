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
#include "streamsmodel.h"
#include "streamdialog.h"
#include "mpdconnection.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KMessageBox>
#else
#include <QtGui/QAction>
#include <QtGui/QMessageBox>
#endif

StreamsPage::StreamsPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    addAction = p->actionCollection()->addAction("addstream");
    addAction->setText(i18n("Add Stream"));
    removeAction = p->actionCollection()->addAction("removestream");
    removeAction->setText(i18n("Remove Stream"));
    editAction = p->actionCollection()->addAction("editstream");
    editAction->setText(i18n("Edit Stream"));
    #else
    addAction = new QAction(tr("Add Stream"), this);
    removeAction = new QAction(tr("Remove Stream"), this);
    editAction = new QAction(tr("Rename Stream"), this);
    #endif
    addAction->setIcon(QIcon::fromTheme("list-add"));
    removeAction->setIcon(QIcon::fromTheme("list-remove"));
    editAction->setIcon(QIcon::fromTheme("document-edit"));
    addStream->setDefaultAction(addAction);
    removeStream->setDefaultAction(removeAction);
    editStream->setDefaultAction(editAction);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), removeStream, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), editStream, SLOT(setEnabled(bool)));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(removeAction, SIGNAL(triggered(bool)), this, SLOT(remove()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlEdit()));
    addStream->setAutoRaise(true);
    removeStream->setAutoRaise(true);
    editStream->setAutoRaise(true);
    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    removeStream->setEnabled(false);
    editStream->setEnabled(false);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

    view->setHeaderHidden(true);
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(removeAction);
    view->addAction(editAction);

    proxy.setSourceModel(StreamsModel::self());
    view->setModel(&proxy);
}

StreamsPage::~StreamsPage()
{
}

void StreamsPage::refresh()
{
    StreamsModel::self()->reload();
}

void StreamsPage::save()
{
    StreamsModel::self()->save();
}

void StreamsPage::addSelectionToPlaylist()
{
    QStringList streams;

    const QModelIndexList selected = view->selectionModel()->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    foreach (const QModelIndex &idx, selected) {
        QString stream=StreamsModel::self()->data(proxy.mapToSource(idx), Qt::ToolTipRole).toString();

        if (!streams.contains(stream)) {
            streams << stream;
        }
    }

    if (!streams.isEmpty()) {
        emit add(streams);
        view->selectionModel()->clearSelection();
    }
}

void StreamsPage::add()
{
    StreamDialog dlg(this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();

        QString existing=StreamsModel::self()->name(url);
        if (!existing.isEmpty()) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("Stream already exists!<br/><i>%1</i>", existing));
            #else
            QMessageBox::critical(this, tr("Error"), tr("Stream already exists!<br/><i>%1</i>").arg(existing));
            #endif
            return;
        }

        if (!StreamsModel::self()->add(name, url)) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("A stream named <i>%1</i> already exists!", name));
            #else
            QMessageBox::critical(this, tr("Error"), tr("A stream named <i>%1</i> already exists!").arg(name));
            #endif
        }
    }
}

void StreamsPage::remove()
{
    QStringList streams;

    const QModelIndexList selected = view->selectionModel()->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndex firstIndex=proxy.mapToSource(selected.first());
    QString firstName=StreamsModel::self()->data(firstIndex, Qt::DisplayRole).toString();
    #ifdef ENABLE_KDE_SUPPORT
    if (KMessageBox::No==KMessageBox::warningYesNo(this, selected.size()>1 ? i18n("Are you sure you wish to remove the %1 selected streams?").arg(selected.size())
                                                                           : i18n("Are you sure you wish to remove <i>%1</i>?").arg(firstName),
                                                   selected.size()>1 ? i18n("Delete Streams?") : i18n("Delete Stream?"))) {
        return;
    }
    #else
    if (QMessageBox::No==QMessageBox::warning(this, selected.size()>1 ? tr("Delete Streams?") : tr("Delete Stream?"),
                                              selected.size()>1 ? tr("Are you sure you wish to remove the %1 selected streams?").arg(selected.size())
                                                                : tr("Are you sure you wish to remove <i>%1</i>?").arg(firstName),
                                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
        return;
    }
    #endif

    foreach (const QModelIndex &idx, selected) {
        StreamsModel::self()->remove(proxy.mapToSource(idx));
    }
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
    QString name=StreamsModel::self()->data(index, Qt::DisplayRole).toString();
    QString url=StreamsModel::self()->data(index, Qt::ToolTipRole).toString();

    dlg.setEdit(name, url);

    if (QDialog::Accepted==dlg.exec()) {
        QString newName=dlg.name();
        QString newUrl=dlg.url();

        QString existingNameForUrl=newUrl!=url ? StreamsModel::self()->name(newUrl) : QString();

        if (!existingNameForUrl.isEmpty()) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("Stream already exists!<br/><i>%1</i>", existingNameForUrl));
            #else
            QMessageBox::critical(this, tr("Error"), tr("Stream already exists!<br/><i>%1</i>").arg(existingNameForUrl));
            #endif
        } else if (newName!=name && StreamsModel::self()->entryExists(newName)) {
            #ifdef ENABLE_KDE_SUPPORT
            KMessageBox::error(this, i18n("A stream named <i>%1</i> already exists!", newName));
            #else
            QMessageBox::critical(this, tr("Error"), tr("A stream named <i>%1</i> already exists!").arg(newName));
            #endif
        } else {
            StreamsModel::self()->edit(index, newName, newUrl);
        }
    }
}

void StreamsPage::controlEdit()
{
    editAction->setEnabled(1==view->selectionModel()->selectedIndexes().size());
}
