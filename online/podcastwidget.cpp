/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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

PodcastWidget::PodcastWidget(PodcastService *s, QWidget *p)
    : SinglePageWidget(p)
    , srv(s)
    , proxy(this)
{
    subscribeAction = new Action(Icons::self()->addNewItemIcon, tr("Add Subscription"), this);
    unSubscribeAction = new Action(Icons::self()->removeIcon, tr("Remove Subscription"), this);
    downloadAction = new Action(Icons::self()->downloadIcon, tr("Download Episodes"), this);
    deleteAction = new Action(MonoIcon::icon(FontAwesome::trash, MonoIcon::constRed, MonoIcon::constRed), tr("Delete Downloaded Episodes"), this);
    cancelDownloadAction = new Action(Icons::self()->cancelIcon, tr("Cancel Download"), this);
    markAsNewAction = new Action(MonoIcon::icon(FontAwesome::asterisk, Utils::monoIconColor()), tr("Mark Episodes As New"), this);
    markAsListenedAction = new Action(tr("Mark Episodes As Listened"), this);
    unplayedOnlyAction = new Action(Icons::self()->rssListIcon, tr("Show Unplayed Only"), this);
    unplayedOnlyAction->setCheckable(true);

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
    connect(unplayedOnlyAction, SIGNAL(toggled(bool)), this, SLOT(showUnplayedOnly(bool)));

    view->setMode(ItemView::Mode_DetailedTree);
    Configuration config(metaObject()->className());
    view->load(config);
    MenuButton *menu=new MenuButton(this);
    ToolButton *addSub=new ToolButton(this);
    ToolButton *unplayedOnlyBtn=new ToolButton(this);
    addSub->setDefaultAction(subscribeAction);
    unplayedOnlyBtn->setDefaultAction(unplayedOnlyAction);
    menu->addActions(createViewActions(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                               << ItemView::Mode_DetailedTree << ItemView::Mode_List));
    init(ReplacePlayQueue|AppendToPlayQueue|Refresh, QList<QWidget *>() << menu << unplayedOnlyBtn, QList<QWidget *>() << addSub);

    view->addAction(subscribeAction);
    view->addAction(unSubscribeAction);
    view->addSeparator();
    view->addAction(downloadAction);
    view->addAction(deleteAction);
    view->addAction(cancelDownloadAction);
    view->addSeparator();
    view->addAction(markAsNewAction);
    view->addAction(markAsListenedAction);
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
    GetEp_Downloaded,
    GetEp_NotDownloaded,
    GetEp_Listened,
    GetEp_NotListened
};

static QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > getEpisodes(PodcastService::Proxy &proxy, const QModelIndexList &selected, GetEp get)
{
    QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > urls;
    foreach (const QModelIndex &idx, selected) {
        PodcastService::Item *item=static_cast<PodcastService::Item *>(proxy.mapToSource(idx).internalPointer());
        if (item->isPodcast()) {
            foreach (PodcastService::Episode *episode, static_cast<PodcastService::Podcast *>(item)->episodes) {
                if ((GetEp_Downloaded==get && !episode->localFile.isEmpty()) ||
                    (GetEp_NotDownloaded==get && episode->localFile.isEmpty()) ||
                    (GetEp_Listened==get && episode->played) ||
                    (GetEp_NotListened==get && !episode->played)) {
                    urls[static_cast<PodcastService::Podcast *>(item)].insert(episode);
                }
            }
        } else {
            PodcastService::Episode *episode=static_cast<PodcastService::Episode *>(item);
            if ((GetEp_Downloaded==get && !episode->localFile.isEmpty()) ||
                (GetEp_NotDownloaded==get && episode->localFile.isEmpty()) ||
                (GetEp_Listened==get && episode->played) ||
                (GetEp_NotListened==get && !episode->played)) {
                urls[episode->parent].insert(episode);
            }
        }
    }
    return urls;
}

void PodcastWidget::download()
{
    QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> > urls=getEpisodes(proxy, view->selectedIndexes(true), GetEp_NotDownloaded);

    if (!urls.isEmpty()) {
        if (MessageBox::No==MessageBox::questionYesNo(this, tr("Do you wish to download the selected podcast episodes?"))) {
            return;
        }
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator it(urls.constBegin());
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator end(urls.constEnd());
        for (; it!=end; ++it) {
            srv->downloadPodcasts(it.key(), it.value().toList());
        }
    }
}

void PodcastWidget::cancelDownload()
{
    if (srv->isDownloading() &&
        MessageBox::Yes==MessageBox::questionYesNo(this, tr("Cancel podcast episode downloads (both current and any that are queued)?"))) {
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
        if (MessageBox::No==MessageBox::questionYesNo(this, tr("Do you wish to mark the selected podcast episodes as new?"))) {
            return;
        }
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
        if (MessageBox::No==MessageBox::questionYesNo(this, tr("Do you wish to mark the selected podcast episodes as listened?"))) {
            return;
        }
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator it(urls.constBegin());
        QMap<PodcastService::Podcast *, QSet<PodcastService::Episode *> >::ConstIterator end(urls.constEnd());
        for (; it!=end; ++it) {
            srv->setPodcastsAsListened(it.key(), it.value().toList(), true);
        }
    }
}

void PodcastWidget::showUnplayedOnly(bool on)
{
    proxy.showUnplayedOnly(on);
}

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
    foreach (const QModelIndex &i, sel) {
        if (!i.parent().isValid()) {
            selected+=proxy.mapToSource(i);
        }
    }

    if (selected.isEmpty() || selected.count()==srv->podcastCount()) {
        if (MessageBox::Yes==MessageBox::questionYesNo(this, tr("Refresh all subscriptions?"), tr("Refresh"), GuiItem(tr("Refresh All")), StdGuiItem::cancel())) {
            srv->refreshAll();
        }
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
    PodcastSettingsDialog dlg(this);
    if (QDialog::Accepted==dlg.exec()) {
        int changes=dlg.changes();
        if (changes&PodcastSettingsDialog::RssUpdate) {
            srv->startRssUpdateTimer();
        }
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
