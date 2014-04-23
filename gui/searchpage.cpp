/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "searchpage.h"
#include "mpdconnection.h"
#include "localize.h"
#include "settings.h"
#include "stdactions.h"
#include "utils.h"
#include "icon.h"
#include "plurals.h"
#include "tableview.h"

class SearchTableView : public TableView
{
public:
    SearchTableView(QWidget *p)
        : TableView(QLatin1String("search"), p)
    {
        setUseSimpleDelegate();
        setIndentation(0);
    }

    virtual ~SearchTableView() { }
};

SearchPage::SearchPage(QWidget *p)
    : QWidget(p)
    , state(-1)
    , model(this)
    , proxy(this)
{
    setupUi(this);
    addToPlayQueue->setDefaultAction(StdActions::self()->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
    locateAction=new Action(Icon("edit-find"), i18n("Locate In Library"), this);
    view->allowTableView(new SearchTableView(view));
    view->addAction(StdActions::self()->addToPlayQueueAction);
    view->addAction(StdActions::self()->addRandomToPlayQueueAction);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addWithPriorityAction);
    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    #ifdef TAGLIB_FOUND
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(StdActions::self()->copyToDeviceAction);
    #endif
    #endif // TAGLIB_FOUND
    view->addAction(locateAction);

    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(&model, SIGNAL(searching()), view, SLOT(showSpinner()));
    connect(&model, SIGNAL(searched()), view, SLOT(hideSpinner()));
    connect(&model, SIGNAL(statsUpdated(int, quint32)), this, SLOT(statsUpdated(int, quint32)));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), this, SLOT(setSearchCategories()));
    connect(locateAction, SIGNAL(triggered(bool)), SLOT(locateSongs()));
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    view->setMode(ItemView::Mode_List);
    view->setPermanentSearch();
    setSearchCategories();
    view->setSearchCategory(Settings::self()->searchCategory());
    statsUpdated(0, 0);
}

SearchPage::~SearchPage()
{
}

void SearchPage::showEvent(QShowEvent *e)
{
    view->focusSearch();
    QWidget::showEvent(e);
}

void SearchPage::saveConfig()
{
    Settings::self()->saveSearchCategory(view->searchCategory());
    TableView *tv=qobject_cast<TableView *>(view->view());
    if (tv) {
        tv->saveHeader();
    }
}

void SearchPage::refresh()
{
    model.refresh();
}

void SearchPage::clear()
{
    model.clear();
}

void SearchPage::setView(int mode)
{
    view->setMode((ItemView::Mode)mode);
    model.setMultiColumn(ItemView::Mode_Table==mode);
}

QStringList SearchPage::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return model.filenames(mapped, allowPlaylists);
}

QList<Song> SearchPage::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return model.songs(mapped, allowPlaylists);
}

void SearchPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty, bool randomAlbums)
{
    Q_UNUSED(randomAlbums)
    QStringList files=selectedFiles(true);

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, replace, priorty);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
void SearchPage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}
#endif

void SearchPage::searchItems()
{
    QString text=view->searchText().trimmed();

    if (text.isEmpty()) {
        model.clear();
    } else {
        model.search(view->searchCategory(), text);
    }
}

void SearchPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return; //doubleclick should only have one selected item
    }
    addSelectionToPlaylist();
}

void SearchPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enable=selected.count()>0;

    StdActions::self()->addToPlayQueueAction->setEnabled(enable);
    StdActions::self()->addWithPriorityAction->setEnabled(enable);
    StdActions::self()->replacePlayQueueAction->setEnabled(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enable);
    StdActions::self()->addRandomToPlayQueueAction->setEnabled(false);
    locateAction->setEnabled(enable);
}

void SearchPage::setSearchCategories()
{
    int newState=(MPDConnection::self()->composerTagSupported() ? State_ComposerSupported : 0)|
                 (MPDConnection::self()->commentTagSupported() ? State_CommmentSupported : 0);

    if (state==newState) {
        // No changes to be made!
        return;
    }

    state=newState;
    QList<QPair<QString, QString> > categories;

    categories << QPair<QString, QString>(i18n("Artist:"), QLatin1String("artist"));

    if (state&State_ComposerSupported) {
        categories << QPair<QString, QString>(i18n("Composer:"), QLatin1String("composer"));
    }

    categories << QPair<QString, QString>(i18n("Album:"), QLatin1String("album"))
               << QPair<QString, QString>(i18n("Title:"), QLatin1String("title"))
               << QPair<QString, QString>(i18n("Genre:"), QLatin1String("genre"));
    if (state&State_CommmentSupported) {
        categories << QPair<QString, QString>(i18n("Comment:"), QLatin1String("comment"));
    }
    categories << QPair<QString, QString>(i18n("Date:"), QLatin1String("date"))
               << QPair<QString, QString>(i18n("File:"), QLatin1String("file"))
               << QPair<QString, QString>(i18n("Any:"), QLatin1String("any"));
    view->setSearchCategories(categories);
}

void SearchPage::statsUpdated(int songs, quint32 time)
{
    statsLabel->setText(0==songs ? i18n("No tracks found.") : Plurals::tracksWithDuration(songs, Utils::formatDuration(time)));
}

void SearchPage::locateSongs()
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit locate(songs);
    }
}
