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

#include "serverinfopage.h"
#include "mpdconnection.h"
#include "mpdstats.h"
#include "mainwindow.h"
#include "mpdparseutils.h"
#include "localize.h"
#include "icon.h"
#include "action.h"
#include "actioncollection.h"

ServerInfoPage::ServerInfoPage(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    updateAction = ActionCollection::get()->createAction("updatempdinfo", i18n("Update MPD Information"), "view-refresh");
    updateInfo->setDefaultAction(updateAction);
    connect(updateAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(getStats()));
    connect(MPDStats::self(), SIGNAL(updated()), SLOT(statsUpdated()));
    connect(MPDConnection::self(), SIGNAL(version(long)), SLOT(mpdVersion(long)));
    connect(MPDConnection::self(), SIGNAL(urlHandlers(const QStringList &)), SLOT(urlHandlers(const QStringList &)));

    Icon::init(updateInfo);
    clear();
}

ServerInfoPage::~ServerInfoPage()
{
}

void ServerInfoPage::clear()
{
    version->setText(QString());
    uptime->setText(QString());
    timePlaying->setText(QString());
    totalDuration->setText(QString());
    artists->setText(QString());
    album->setText(QString());
    songs->setText(QString());
    lastUpdate->setText(QString());
    urlhandlers->setText(QString());
}

void ServerInfoPage::statsUpdated()
{
    uptime->setText(MPDParseUtils::formatDuration(MPDStats::self()->uptime()));
    timePlaying->setText(MPDParseUtils::formatDuration(MPDStats::self()->playtime()));
    totalDuration->setText(MPDParseUtils::formatDuration(MPDStats::self()->dbPlaytime()));
    artists->setText(QString::number(MPDStats::self()->artists()));
    album->setText(QString::number(MPDStats::self()->albums()));
    songs->setText(QString::number(MPDStats::self()->songs()));
//     #ifdef ENABLE_KDE_SUPPORT
//     lastUpdate->setText(KGlobal::locale()->formatDateTime(MPDStats::self()->dbUpdate));
//     #else
    lastUpdate->setText(MPDStats::self()->dbUpdate().toString(Qt::SystemLocaleShortDate));
//     #endif
}

void ServerInfoPage::mpdVersion(long v)
{
    version->setText(QString("%1.%2.%3").arg((v>>16)&0xFF).arg((v>>8)&0xFF).arg(v&0xFF));
}

void ServerInfoPage::urlHandlers(const QStringList &handlers)
{
    urlhandlers->setText(handlers.join(", "));
}
