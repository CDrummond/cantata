/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "settings.h"
#include "stdactions.h"
#include "customactions.h"
#include "support/utils.h"
#include "support/icon.h"
#include "support/messagebox.h"
#include "widgets/tableview.h"
#include "widgets/menubutton.h"
#include "widgets/icons.h"

class SearchTableView : public TableView
{
public:
    SearchTableView(QWidget *p)
        : TableView(QLatin1String("search"), p)
    {
        setUseSimpleDelegate();
        setIndentation(0);
    }

    ~SearchTableView() override { }
};

SearchPage::SearchPage(QWidget *p)
    : SinglePageWidget(p)
    , state(-1)
    , model(this)
    , proxy(this)
{
    statsLabel=new SqueezedTextLabel(this);
    locateAction=new Action(Icons::self()->searchIcon, tr("Locate In Library"), this);
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
    statsLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    init(ReplacePlayQueue|AppendToPlayQueue, QList<QWidget *>() << menu << statsLabel);

    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    view->addAction(CustomActions::self());
    #ifdef TAGLIB_FOUND
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(StdActions::self()->copyToDeviceAction);
    #endif
    view->addAction(StdActions::self()->organiseFilesAction);
    view->addAction(StdActions::self()->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(StdActions::self()->replaygainAction);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addSeparator();
    view->addAction(StdActions::self()->deleteSongsAction);
    #endif
    #endif // TAGLIB_FOUND
    view->addAction(locateAction);
    view->setInfoText(tr("Use the text field above to search for music in your library via MPD's search mechanism. "
                         "Clicking the label next to the field will produce a menu allowing you to control the search area."));
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
    for (const QModelIndex &idx: selected) {
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
    for (const QModelIndex &idx: selected) {
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

void SearchPage::deleteSongs()
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        if (MessageBox::Yes==MessageBox::warningYesNo(this, tr("Are you sure you wish to delete the selected songs?\n\nThis cannot be undone."),
                                                      tr("Delete Songs"), StdGuiItem::del(), StdGuiItem::cancel())) {
            emit deleteSongs(QString(), songs);
        }
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

    StdActions::self()->enableAddToPlayQueue(enable);
    CustomActions::self()->setEnabled(enable);
    locateAction->setEnabled(enable);

    StdActions::self()->addToStoredPlaylistAction->setEnabled(enable);
    #ifdef TAGLIB_FOUND
    StdActions::self()->organiseFilesAction->setEnabled(enable && MPDConnection::self()->getDetails().dirReadable);
    StdActions::self()->editTagsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    StdActions::self()->replaygainAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->deleteSongsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    StdActions::self()->copyToDeviceAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #endif // TAGLIB_FOUND
}

void SearchPage::setSearchCategory(const QString &cat)
{
    view->setSearchCategory(cat);
}

void SearchPage::setSearchCategories()
{
    int newState=(MPDConnection::self()->composerTagSupported() ? State_ComposerSupported : 0)|
                 (MPDConnection::self()->commentTagSupported() ? State_CommmentSupported : 0)|
                 (MPDConnection::self()->performerTagSupported() ? State_PerformerSupported : 0)|
                 (MPDConnection::self()->modifiedFindSupported() ? State_ModifiedSupported : 0)|
                 (MPDConnection::self()->originalDateTagSupported() ? State_OrigDateSupported : 0);

    if (state==newState) {
        // No changes to be made!
        return;
    }

    state=newState;
    QList<SearchWidget::Category> categories;

    categories << SearchWidget::Category(tr("Artist:"), QLatin1String("artist"), tr("Search for songs by artist."));

    if (state&State_ComposerSupported) {
        categories << SearchWidget::Category(tr("Composer:"), QLatin1String("composer"), tr("Search for songs by composer."));
    }
    if (state&State_PerformerSupported) {
        categories << SearchWidget::Category(tr("Performer:"), QLatin1String("performer"), tr("Search for songs by performer."));
    }
    categories << SearchWidget::Category(tr("Album:"), QLatin1String("album"), tr("Search for songs by album."))
               << SearchWidget::Category(tr("Title:"), QLatin1String("title"), tr("Search for songs by title."))
               << SearchWidget::Category(tr("Genre:"), QLatin1String("genre"), tr("Search for songs by genre."));
    if (state&State_CommmentSupported) {
        categories << SearchWidget::Category(tr("Comment:"), QLatin1String("comment"), tr("Search for songs containing comment."));
    }
    categories << SearchWidget::Category(tr("Date:"), QLatin1String("date"),
                                         tr("Find songs be searching the 'Date' tag.<br/><br/>Usually just entering the year should suffice."));
    if (state&State_OrigDateSupported) {
        categories << SearchWidget::Category(tr("Original Date:"), QLatin1String("originaldate"),
                                             tr("Find songs be searching the 'Original Date' tag.<br/><br/>Usually just entering the year should suffice."));
    }
    if (state&State_ModifiedSupported) {
        categories << SearchWidget::Category(tr("Modified:"), MPDConnection::constModifiedSince,
                                             tr("Enter date (YYYY/MM/DD - e.g. 2015/01/31) to search for files modified since that date.<br/><br>"
                                                  "Or enter a number of days to find files that were modified in the previous number of days."));
    }
    categories << SearchWidget::Category(tr("File:"), QLatin1String("file"), tr("Search for songs by filename or path."))
               << SearchWidget::Category(tr("Any:"), QLatin1String("any"), tr("Search for songs by any matching metadata."));
    view->setSearchCategories(categories);
}

void SearchPage::statsUpdated(int songs, quint32 time)
{
    statsLabel->setText(0==songs ? tr("No tracks found.") : tr("%n Tracks (%1)", "", songs).arg(Utils::formatDuration(time)));
}

void SearchPage::locateSongs()
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit locate(songs);
    }
}

#include "moc_searchpage.cpp"
