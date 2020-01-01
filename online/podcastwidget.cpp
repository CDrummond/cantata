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

#include "podcastwidget.h"
#include "podcastsearchdialog.h"
#include "podcastsettingsdialog.h"
#include "widgets/itemview.h"
#include "widgets/toolbutton.h"
#include "widgets/icons.h"
#include "widgets/menubutton.h"
#include "support/action.h"
#include "support/messagebox.h"
#include "support/configuration.h"
#include "support/monoicon.h"
#include "support/utils.h"
#include <QTimer>
#include <QFileDialog>

PodcastWidget::PodcastWidget(PodcastService *s, QWidget *p)
    : SinglePageWidget(p)
    , srv(s)
    , proxy(this)
{
    QIcon newIcon = MonoIcon::icon(FontAwesome::asterisk, Utils::monoIconColor());
    subscribeAction = new Action(Icons::self()->addNewItemIcon, tr("Add Subscription"), this);
    unSubscribeAction = new Action(Icons::self()->removeIcon, tr("Remove Subscription"), this);
    downloadAction = new Action(Icons::self()->downloadIcon, tr("Download Episodes"), this);
    deleteAction = new Action(MonoIcon::icon(FontAwesome::trash, MonoIcon::constRed, MonoIcon::constRed), tr("Delete Downloaded Episodes"), this);
    cancelDownloadAction = new Action(Icons::self()->cancelIcon, tr("Cancel Download"), this);
    markAsNewAction = new Action(newIcon, tr("Mark Episodes As New"), this);
    markAsListenedAction = new Action(tr("Mark Episodes As Listened"), this);
    //unplayedOnlyAction = new Action(newIcon, tr("Show Unplayed Only"), this);
    //unplayedOnlyAction->setCheckable(true);
    exportAction = new Action(tr("Export Current Subscriptions"), this);

    proxy.setSourceModel(srv);
    view->setModel(&proxy);

    view->alwaysShowHeader();
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));

    connect(subscribeAction, SIGNAL(triggered()), this, SLOT(subscribe()));
    connect(unSubscribeAction, SIGNAL(triggered()), this, SLOT(unSubscribe()));
    connect(downloadAction, SIGNAL(triggered()), this, SLOT(download()));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteDownload()));
    connect(cancelDownloadAction, SIGNAL(triggered()), this, SLOT(cancelDownload()));
    connect(markAsNewAction, SIGNAL(triggered()), this, SLOT(markAsNew()));
    connect(markAsListenedAction, SIGNAL(triggered()), this, SLOT(markAsListened()));
    //connect(unplayedOnlyAction, SIGNAL(toggled(bool)), this, SLOT(showUnplayedOnly(bool)));
    connect(exportAction, SIGNAL(triggered()), SLOT(exportSubscriptions()));

    view->setMode(ItemView::Mode_DetailedTree);
    Configuration config(metaObject()->className());
    view->load(config);
    MenuButton *menu=new MenuButton(this);
    ToolButton *addSub=new ToolButton(this);
    //ToolButton *unplayedOnlyBtn=new ToolButton(this);
    addSub->setDefaultAction(subscribeAction);
    //unplayedOnlyBtn->setDefaultAction(unplayedOnlyAction);
    menu->addAction(createViewMenu(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                           << ItemView::Mode_DetailedTree << ItemView::Mode_List));

    Action *configureAction=new Action(Icons::self()->configureIcon, tr("Configure"), this);
    connect(configureAction, SIGNAL(triggered()), SLOT(configure()));
    menu->addAction(configureAction);
    menu->addAction(exportAction);
    init(ReplacePlayQueue|AppendToPlayQueue|Refresh, QList<QWidget *>() << menu/* << unplayedOnlyBtn*/, QList<QWidget *>() << addSub);

    view->addAction(subscribeAction);
    view->addAction(unSubscribeAction);
    view->addSeparator();
    view->addAction(downloadAction);
    view->addAction(deleteAction);
    view->addAction(cancelDownloadAction);
    view->addSeparator();
    view->addAction(markAsNewAction);
    view->addAction(markAsListenedAction);
    view->setInfoText(tr("Use the + icon (below) to add podcast subscriptions."));
}

PodcastWidget::~PodcastWidget()
{
    Configuration config(metaObject()->className());
    view->save(config);
}

QStringList PodcastWidget::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }
    return srv->filenames(proxy.mapToSource(selected), allowPlaylists);
}

QList<Song> PodcastWidget::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return srv->songs(proxy.mapToSource(selected), allowPlaylists);
}

void PodcastWidget::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void PodcastWidget::subscribe()
{
    if (0==PodcastSearchDialog::instanceCount()) {
        PodcastSearchDialog *dlg=new PodcastSearchDialog(srv, this);
        dlg->show();
    }
}

void PodcastWidget::unSubscribe()
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return;
    }

    PodcastService::Item *item=static_cast<PodcastService::Item *>(proxy.mapToSource(selected.first()).internalPointer());
    if (!item->isPodcast()) {
        return;
    }

    if (MessageBox::No==MessageBox::warningYesNo(this, tr("Unsubscribe from '%1'?").arg(item->name))) {
        return;
    }

    srv->unSubscribe(static_cast<PodcastService::Podcast *>(item));
}

enum GetEp {
    GetEp_Downloaded     = 0x01,
    GetEp_NotDownloaded  = 0x02,
    GetEp_Listened       = 0x04,
    GetEp_NotListened    = 0x08
};

static bool useEpisode(const PodcastService::Episode *episode, int get)
{
    return ! ((get&GetEp_Downloaded && episode->localFile.isEmpty()) ||
              (get&GetEp_NotDownloaded && !episode->localFile.isEmpty()) ||
              (get&GetEp_Listened && !episode->played) ||
              (get&GetEp_NotListened && episode->played) );
}

static QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > getEpisodes(PodcastService::Proxy &proxy, const QModelIndexList &selected, int get)
{
    QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > urls;
    for (const QModelIndex &idx: selected) {
        PodcastService::Item *item=static_cast<PodcastService::Item *>(proxy.mapToSource(idx).internalPointer());
        if (item->isPodcast()) {
            for (PodcastService::Episode *episode: static_cast<PodcastService::Podcast *>(item)->episodes) {
                if (useEpisode(episode, get)) {
                    urls[static_cast<PodcastService::Podcast *>(item)].insert(episode);
                }
            }
        } else {
            PodcastService::Episode *episode=static_cast<PodcastService::Episode *>(item);
            if (useEpisode(episode, get)) {
                urls[episode->parent].insert(episode);
            }
        }
    }
    return urls;
}

void PodcastWidget::download()
{
    QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > urls=getEpisodes(proxy, view->selectedIndexes(true), GetEp_NotDownloaded/*|(unplayedOnlyAction->isChecked() ? GetEp_NotListened : 0)*/);

    if (!urls.isEmpty()) {
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator it(urls.constBegin());
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator end(urls.constEnd());
        for (; it!=end; ++it) {
            srv->downloadPodcasts(it.key(), it.value().toList());
        }
    }
}

void PodcastWidget::cancelDownload()
{
    if (srv->isDownloading()) {
        srv->cancelAll();
    }
}

void PodcastWidget::deleteDownload()
{
    QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > urls=getEpisodes(proxy, view->selectedIndexes(false), GetEp_Downloaded);

    if (!urls.isEmpty()) {
        if (MessageBox::No==MessageBox::questionYesNo(this, tr("Do you wish to the delete downloaded files of the selected podcast episodes?"))) {
            return;
        }
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator it(urls.constBegin());
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator end(urls.constEnd());
        for (; it!=end; ++it) {
            srv->deleteDownloadedPodcasts(it.key(), it.value().toList());
        }
    }
}

void PodcastWidget::markAsNew()
{
    QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > urls=getEpisodes(proxy, view->selectedIndexes(false), GetEp_Listened);

    if (!urls.isEmpty()) {
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator it(urls.constBegin());
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator end(urls.constEnd());
        for (; it!=end; ++it) {
            srv->setPodcastsAsListened(it.key(), it.value().toList(), false);
        }
    }
}

void PodcastWidget::markAsListened()
{
    QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > urls=getEpisodes(proxy, view->selectedIndexes(false), GetEp_NotListened);

    if (!urls.isEmpty()) {
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator it(urls.constBegin());
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator end(urls.constEnd());
        for (; it!=end; ++it) {
            srv->setPodcastsAsListened(it.key(), it.value().toList(), true);
        }
    }
}

/*
void PodcastWidget::showUnplayedOnly(bool on)
{
    view->goToTop();
    proxy.showUnplayedOnly(on);
}
*/

void PodcastWidget::doSearch()
{
    QString text=view->searchText().trimmed();
    proxy.update(text);
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll();
    }
}

void PodcastWidget::refresh()
{
    QModelIndexList sel=view->selectedIndexes(false);
    QModelIndexList selected;
    for (const QModelIndex &i: sel) {
        if (!i.parent().isValid()) {
            selected+=proxy.mapToSource(i);
        }
    }

    if (selected.isEmpty() || selected.count()==srv->podcastCount()) {
        srv->refreshAll();
        return;
    }
    switch (MessageBox::questionYesNoCancel(this, tr("Refresh all subscriptions, or only those selected?"), tr("Refresh"), GuiItem(tr("Refresh All")), GuiItem(tr("Refresh Selected")))) {
    case MessageBox::Yes:
        srv->refreshAll();
        break;
    case MessageBox::No:
        srv->refresh(selected);
        break;
    default:
        break;
    }
}

void PodcastWidget::configure()
{
    srv->configure(this);
}

void PodcastWidget::exportSubscriptions()
{
    if (0==srv->podcastCount()) {
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this, tr("Export Podcast Subscriptions"), QDir::homePath(), QString(".opml"));
    if (filename.isEmpty()) {
        return;
    }

    if (!srv->exportSubscriptions(filename)) {
        MessageBox::error(this, tr("Export failed!"));
    }
}

void PodcastWidget::controlActions()
{
    SinglePageWidget::controlActions();

    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    downloadAction->setEnabled(!selected.isEmpty());
    unSubscribeAction->setEnabled(1==selected.count() && static_cast<PodcastService::Item *>(proxy.mapToSource(selected.first()).internalPointer())->isPodcast());
    downloadAction->setEnabled(!selected.isEmpty());
    deleteAction->setEnabled(!selected.isEmpty());
    cancelDownloadAction->setEnabled(!selected.isEmpty());
    markAsNewAction->setEnabled(!selected.isEmpty());
    markAsListenedAction->setEnabled(!selected.isEmpty());
}

#include "moc_podcastwidget.cpp"
