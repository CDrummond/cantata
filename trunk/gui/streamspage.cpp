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
    renameAction = p->actionCollection()->addAction("renamestream");
    renameAction->setText(i18n("Rename Stream"));
#else
    addAction = new QAction(tr("Add Stream"), this);
    removeAction = new QAction(tr("Remove Stream"), this);
    renameAction = new QAction(tr("Rename Stream"), this);
#endif
    addAction->setIcon(QIcon::fromTheme("list-add"));
    removeAction->setIcon(QIcon::fromTheme("list-remove"));
    addStream->setDefaultAction(addAction);
    removeStream->setDefaultAction(removeAction);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), removeStream, SLOT(setEnabled(bool)));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(removeAction, SIGNAL(triggered(bool)), this, SLOT(remove()));
    connect(renameAction, SIGNAL(triggered(bool)), this, SLOT(rename()));
    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));

    addStream->setAutoRaise(true);
    removeStream->setAutoRaise(true);
    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    removeStream->setEnabled(false);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

    view->setHeaderHidden(true);
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(removeAction);
    view->addAction(renameAction);

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


static void showError(QWidget *parent, const QString &error)
{
#ifdef ENABLE_KDE_SUPPORT
    KMessageBox::error(parent, error);
#else
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Error);
    msgBox.setWindowTitle(tr("Error"));
    msgBox.setText(error);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
#endif
}

void StreamsPage::add()
{
    StreamDialog dlg(this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();

        if (name.isEmpty()) {
#ifdef ENABLE_KDE_SUPPORT
            showError(this, i18n("No name supplied!"));
#else
            showError(this, tr("No name supplied!"));
#endif
            return;
        }

        if (url.isEmpty()) {
#ifdef ENABLE_KDE_SUPPORT
            showError(this, i18n("No stream supplied!"));
#else
            showError(this, tr("No stream supplied!"));
#endif
            return;
        }

        QString existing=StreamsModel::self()->name(url);
        if (!existing.isEmpty()) {
#ifdef ENABLE_KDE_SUPPORT
            showError(this, i18n("Stream already exists!</br><i>%1</i>", existing));
#else
            showError(this, tr("Stream already exists!</br><i>%1</i>").arg(existing));
#endif
            return;
        }

        if (!StreamsModel::self()->add(name, url)) {
#ifdef ENABLE_KDE_SUPPORT
            showError(this, i18n("A stream named <i>%1</i> already exists!", name));
#else
            showError(this, tr("A stream named <i>%1</i> already exists!").arg(name));
#endif
        }
    }
}

void StreamsPage::remove()
{
}

void StreamsPage::rename()
{
}
