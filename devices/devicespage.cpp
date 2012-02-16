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

#include "devicespage.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "mainwindow.h"
#include "devicesmodel.h"
#include "settings.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KGlobalSettings>
#include <KDE/KMessageBox>
#else
#include <QtGui/QAction>
#endif

DevicesPage::DevicesPage(MainWindow *p)
    : QWidget(p)
    , mw(p)
{
    setupUi(this);

    configureAction = p->actionCollection()->addAction("configuredevice");
    configureAction->setText(i18n("Configure Device"));
    configureAction->setIcon(QIcon::fromTheme("configure"));
    refreshAction = p->actionCollection()->addAction("refreshdevice");
    refreshAction->setText(i18n("Refresh Device"));
    refreshAction->setIcon(QIcon::fromTheme("view-refresh"));
    copyAction = p->actionCollection()->addAction("copytolibrary");
    copyAction->setText(i18n("Copy To Library"));
    copyAction->setIcon(QIcon::fromTheme("document-import"));
    copyToLibraryButton->setDefaultAction(copyAction);
    MainWindow::initButton(copyToLibraryButton);
    copyToLibraryButton->setEnabled(false);
    view->addAction(copyAction);
//     view->addAction(p->burnAction);
    view->addAction(p->organiseFilesAction);
    #ifdef TAGLIB_FOUND
    view->addAction(p->editTagsAction);
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(p->replaygainAction);
    #endif
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    view->addAction(sep);
    view->addAction(p->deleteSongsAction);
    connect(DevicesModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), this, SLOT(updateGenres(const QSet<QString> &)));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copyToLibrary()));
    connect(configureAction, SIGNAL(triggered()), this, SLOT(configureDevice()));
    connect(refreshAction, SIGNAL(triggered()), this, SLOT(refreshDevice()));
    MainWindow::initButton(menuButton);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu=new QMenu(this);
    menu->addAction(configureAction);
    menu->addAction(refreshAction);
    menu->addSeparator();
//     menu->addAction(copyAction);
    menu->addAction(p->organiseFilesAction);
    #ifdef TAGLIB_FOUND
    menu->addAction(p->editTagsAction);
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    menu->addAction(p->replaygainAction);
    #endif
    menu->addAction(sep);
    menu->addAction(p->deleteSongsAction);
    menuButton->setMenu(menu);
    menuButton->setIcon(QIcon::fromTheme("system-run"));
    proxy.setSourceModel(DevicesModel::self());
    #ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Devices"));
    #else
    view->setTopText(tr("Devices"));
    #endif
    view->setModel(&proxy);
    view->init(configureAction, refreshAction, 0);
    view->setRootIsDecorated(false);
    updateGenres(QSet<QString>());
}

DevicesPage::~DevicesPage()
{
}

void DevicesPage::clear()
{
    DevicesModel::self()->clear();
    view->setLevel(0);
}

QString DevicesPage::activeUmsDeviceUdi() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QString();
    }

    foreach (const QModelIndex &idx, selected) {
        QModelIndex index = proxy.mapToSource(idx);
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(index.internalPointer());
        if (item && MusicLibraryItem::Type_Root==item->type()) {
            return QString();
        } else {
            MusicLibraryItem *p=item;
            while(p->parent()) {
                p=p->parent();
            }
            if (MusicLibraryItem::Type_Root==p->type()) {
                Device *dev=static_cast<Device *>(p);
                if (Device::Ums!=dev->type()) {
                    return QString();
                }
                return dev->dev().udi();
            }
        }
    }

    return QString();
}

QList<Song> DevicesPage::selectedSongs() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }

    // Ensure all songs are from UMS devices...
    QString udi;
    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        QModelIndex index = proxy.mapToSource(idx);
        mapped.append(index);
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(index.internalPointer());
        if (item && MusicLibraryItem::Type_Root==item->type()) {
            return QList<Song>();
        } else {
            MusicLibraryItem *p=item;
            while(p->parent()) {
                p=p->parent();
            }
            if (MusicLibraryItem::Type_Root==p->type()) {
                Device *dev=static_cast<Device *>(p);

                if (Device::Ums!=dev->type()) {
                    return QList<Song>();
                }
            }
        }
    }

    return DevicesModel::self()->songs(mapped);
}

void DevicesPage::itemDoubleClicked(const QModelIndex &)
{
//     const QModelIndexList selected = view->selectedIndexes();
//     if (1!=selected.size()) {
//         return; //doubleclick should only have one selected item
//     }
//     MusicDevicesItem *item = static_cast<MusicDevicesItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
//     if (MusicDevicesItem::Type_Song==item->type()) {
//         addSelectionToPlaylist();
//     }
}

void DevicesPage::searchItems()
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

void DevicesPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    bool enable=!selected.isEmpty();
    bool onlyUms=true;
    bool singleUdi=true;
    QString udi;

    foreach (const QModelIndex &idx, selected) {
        QModelIndex index = proxy.mapToSource(idx);
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(index.internalPointer());
        if (item && MusicLibraryItem::Type_Root==item->type()) {
            enable=false;
            break;
        } else {
            MusicLibraryItem *p=item;
            while(p->parent()) {
                p=p->parent();
            }
            if (MusicLibraryItem::Type_Root==p->type()) {
                Device *dev=static_cast<Device *>(p);
                if (Device::Ums!=dev->type()) {
                    onlyUms=false;
                }
                if (udi.isEmpty()) {
                    udi=dev->dev().udi();
                } else if (udi!=dev->dev().udi()) {
                    singleUdi=false;
                }
            }
        }
    }

    configureAction->setEnabled(!enable && 1==selected.count());
    refreshAction->setEnabled(!enable && 1==selected.count());
    copyAction->setEnabled(enable);
    mw->deleteSongsAction->setEnabled(enable);
    #ifdef TAGLIB_FOUND
    mw->editTagsAction->setEnabled(enable && onlyUms);
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    mw->replaygainAction->setEnabled(enable && onlyUms);
    #endif
    //mw->burnAction->setEnabled(enable && onlyUms);
    mw->organiseFilesAction->setEnabled(enable && onlyUms && singleUdi);
}

void DevicesPage::copyToLibrary()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(mapped.first().internalPointer());
    while (item->parent()) {
        item=item->parent();
    }
    QString udi;
    if (MusicLibraryItem::Type_Root==item->type()) {
        udi=static_cast<Device *>(item)->dev().udi();
    }

    if (udi.isEmpty()) {
        return;
    }

    QList<Song> songs=DevicesModel::self()->songs(mapped);

    if (!songs.isEmpty()) {
        emit addToDevice(udi, QString(), songs);
        view->clearSelection();
    }
}

void DevicesPage::configureDevice()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->type()) {
        static_cast<Device *>(item)->configure(this);
    }
}

void DevicesPage::refreshDevice()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->type()) {
        static_cast<Device *>(item)->rescan();
    }
}

void DevicesPage::deleteSongs()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(mapped.first().internalPointer());
    while (item->parent()) {
        item=item->parent();
    }
    QString udi;
    if (MusicLibraryItem::Type_Root==item->type()) {
        udi=static_cast<Device *>(item)->dev().udi();
    }

    if (udi.isEmpty()) {
        return;
    }

    QList<Song> songs=DevicesModel::self()->songs(mapped);

    if (!songs.isEmpty()) {
        if (KMessageBox::Yes==KMessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected songs?\nThis cannot be undone."))) {
            emit deleteSongs(udi, songs);
        }
        view->clearSelection();
    }
}

void DevicesPage::updateGenres(const QSet<QString> &g)
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
