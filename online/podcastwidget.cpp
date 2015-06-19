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

#include "podcastwidget.h"
#include "podcastsearchdialog.h"
#include "podcastsettingsdialog.h"
#include "widgets/itemview.h"
#include "widgets/toolbutton.h"
#include "support/action.h"
#include "support/localize.h"
#include "support/messagebox.h"
#include <QTimer>

PodcastWidget::PodcastWidget(PodcastService *s, QWidget *p)
    : SinglePageWidget(p)
    , srv(s)
{
    view->setModel(s);
    subscribeAction = new Action(Icon("document-new"), i18n("Add Subscription"), this);

    ToolButton *addSub=new ToolButton(this);
    addSub->setDefaultAction(subscribeAction);
    init(All, QList<ToolButton *>(), QList<ToolButton *>() <<addSub);
    view->alwaysShowHeader();
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(subscribeAction, SIGNAL(triggered()), this, SLOT(subscribe()));
}

PodcastWidget::~PodcastWidget()
{
}

QStringList PodcastWidget::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }
    return srv->filenames(selected, allowPlaylists);
}

QList<Song> PodcastWidget::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return srv->songs(selected, allowPlaylists);
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

void PodcastWidget::doSearch()
{

}

void PodcastWidget::refresh()
{
    QModelIndexList sel=view->selectedIndexes(false);
    QModelIndexList selected;
    foreach (const QModelIndex &i, sel) {
        if (!i.parent().isValid()) {
            selected+=i;
        }
    }

    switch (selected.isEmpty() || selected.count()!=srv->podcastCount()
                ? MessageBox::questionYesNoCancel(this, i18n("Refresh all subscriptions?"), i18n("Refresh"))
                : MessageBox::questionYesNoCancel(this, i18n("Refresh all subscriptions, or only those selected?"), i18n("Refresh"), i18n("All"), i18n("Selected"))) {
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
