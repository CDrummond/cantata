/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpd-interface/mpdconnection.h"
#include "support/localize.h"
#include "settings.h"
#include "stdactions.h"
#include "customactions.h"
#include "support/utils.h"
#include "support/icon.h"
#include "plurals.h"
#include "widgets/tableview.h"
#include "widgets/menubutton.h"

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
    : SinglePageWidget(p)
    , state(-1)
    , model(this)
    , proxy(this)
{
    statsLabel=new SqueezedTextLabel(this);
    locateAction=new Action(Icon("edit-find"), i18n("Locate In Library"), this);
    view->allowTableView(new SearchTableView(view));

    connect(&model, SIGNAL(searching()), view, SLOT(showSpinner()));
    connect(&model, SIGNAL(searched()), view, SLOT(hideSpinner()));
    connect(&model, SIGNAL(statsUpdated(int, quint32)), this, SLOT(statsUpdated(int, quint32)));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), this, SLOT(setSearchCategories()));
    connect(locateAction, SIGNAL(triggered()), SLOT(locateSongs()));
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    view->setPermanentSearch();
    setSearchCategories();
    view->setSearchCategory(Settings::self()->searchCategory());
    statsUpdated(0, 0);    
    view->setMode(ItemView::Mode_List);
    Configuration config(metaObject()->className());
    view->load(config);
    MenuButton *menu=new MenuButton(this);
    menu->addActions(createViewActions(QList<ItemView::Mode>() << ItemView::Mode_List << ItemView::Mode_Table));
    init(ReplacePlayQueue|AppendToPlayQueue, QList<QWidget *>() << menu << statsLabel);

    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    view->addAction(CustomActions::self());
    #ifdef TAGLIB_FOUND
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(StdActions::self()->copyToDeviceAction);
    #endif
    #endif // TAGLIB_FOUND
    view->addAction(locateAction);
}

SearchPage::~SearchPage()
{
    Configuration config(metaObject()->className());
    view->save(config);
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

void SearchPage::showEvent(QShowEvent *e)
{
    SinglePageWidget::showEvent(e);
    view->focusSearch();
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

void SearchPage::doSearch()
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

    StdActions::self()->appendToPlayQueueAction->setEnabled(enable);
    StdActions::self()->addWithPriorityAction->setEnabled(enable);
    StdActions::self()->replacePlayQueueAction->setEnabled(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enable);
    locateAction->setEnabled(enable);
}

void SearchPage::setSearchCategories()
{
    int newState=(MPDConnection::self()->composerTagSupported() ? State_ComposerSupported : 0)|
                 (MPDConnection::self()->commentTagSupported() ? State_CommmentSupported : 0)|
                 (MPDConnection::self()->performerTagSupported() ? State_PerformerSupported : 0)|
                 (MPDConnection::self()->modifiedFindSupported() ? State_ModifiedSupported : 0);

    if (state==newState) {
        // No changes to be made!
        return;
    }

    state=newState;
    QList<SearchWidget::Category> categories;

    categories << SearchWidget::Category(i18n("Artist:"), QLatin1String("artist"));

    if (state&State_ComposerSupported) {
        categories << SearchWidget::Category(i18n("Composer:"), QLatin1String("composer"));
    }
    if (state&State_PerformerSupported) {
        categories << SearchWidget::Category(i18n("Performer:"), QLatin1String("performer"));
    }
    categories << SearchWidget::Category(i18n("Album:"), QLatin1String("album"))
               << SearchWidget::Category(i18n("Title:"), QLatin1String("title"))
               << SearchWidget::Category(i18n("Genre:"), QLatin1String("genre"));
    if (state&State_CommmentSupported) {
        categories << SearchWidget::Category(i18n("Comment:"), QLatin1String("comment"));
    }
    categories << SearchWidget::Category(i18n("Date:"), QLatin1String("date"),
                                         i18n("Find songs be searching the 'Date' tag.<br/><br/>Usually just entering the year should suffice."));
    if (state&State_ModifiedSupported) {
        categories << SearchWidget::Category(i18n("Modified:"), MPDConnection::constModifiedSince,
                                             i18n("Enter date (YYYY/MM/DD - e.g. 2015/01/31) to search for files modified since that date.<br/><br>"
                                                  "Or enter a number of days to find files that were modified in the previous number of days."));
    }
    categories << SearchWidget::Category(i18n("File:"), QLatin1String("file"))
               << SearchWidget::Category(i18n("Any:"), QLatin1String("any"));
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
