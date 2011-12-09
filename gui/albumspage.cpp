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

#include "albumspage.h"
#include "mpdconnection.h"
#include "covers.h"
#include "musiclibraryitemsong.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#else
#include <QAction>
#endif

AlbumsPage::AlbumsPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->updateDbAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

#ifdef ENABLE_KDE_SUPPORT
    search->setPlaceholderText(i18n("Search albums..."));
#else
    search->setPlaceholderText(tr("Search albums..."));
#endif
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    proxy.setSourceModel(&model);
    view->setModel(&proxy);

    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(search, SIGNAL(returnPressed()), this, SLOT(searchItems()));
    connect(search, SIGNAL(textChanged(const QString)), this, SLOT(searchItems()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            &model, SLOT(setCover(const QString &, const QString &, const QImage &)));
    clear();
}

AlbumsPage::~AlbumsPage()
{
}

void AlbumsPage::clear()
{
    int size=AlbumsModel::coverPixels();
    view->setIconSize(QSize(size, size));
    view->setGridSize(QSize(size+14, size+44));
    model.clear();
    view->update();
}

void AlbumsPage::addSelectionToPlaylist()
{
    QStringList files;

    const QModelIndexList selected = view->selectionModel()->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    foreach (const QModelIndex &idx, selected) {
        QStringList albumFiles=model.data(proxy.mapToSource(idx), Qt::UserRole).toStringList();

        foreach (const QString & file, albumFiles) {
            if (!files.contains(file)) {
                files.append(file);
            }
        }
    }

    if (!files.isEmpty()) {
        emit add(files);
        view->selectionModel()->clearSelection();
    }
}

void AlbumsPage::itemActivated(const QModelIndex &)
{
    if (1==view->selectionModel()->selectedIndexes().size()) {//doubleclick should only have one selected item
        addSelectionToPlaylist();
    }
}

void AlbumsPage::searchItems()
{
    proxy.setFilterGenre(0==genreCombo->currentIndex() ? QString() : genreCombo->currentText());

    if (search->text().isEmpty()) {
        proxy.setFilterRegExp(QString());
        return;
    }

    proxy.setFilterRegExp(search->text());
}

void AlbumsPage::updateGenres(const QStringList &genres)
{
    QStringList entries;
#ifdef ENABLE_KDE_SUPPORT
    entries << i18n("All Genres");
#else
    entries << tr("All Genres");
#endif
    entries+=genres;

    bool diff=genreCombo->count() != entries.count();
    if (!diff) {
        // Check items...
        for (int i=1; i<genreCombo->count() && !diff; ++i) {
            if (genreCombo->itemText(i) != entries.at(i)) {
                diff=true;
            }
        }
    }

    if (!diff) {
        return;
    }

    QString currentFilter = genreCombo->currentIndex() ? genreCombo->currentText() : QString();

    genreCombo->clear();
    if (genres.count()<2) {
        genreCombo->setCurrentIndex(0);
    } else {
        genreCombo->addItems(entries);
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
