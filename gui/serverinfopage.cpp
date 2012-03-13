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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KGlobal>
#include <KDE/KActionCollection>
#else
#include <QtGui/QAction>
#endif


ServerInfoPage::ServerInfoPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    updateAction = p->actionCollection()->addAction("updatempdinfo");
    updateAction->setText(i18n("Update MPD Information"));
    #else
    updateAction = new QAction(tr("Update MPD Information"), this);
    #endif
    updateAction->setIcon(QIcon::fromTheme("view-refresh"));
    updateInfo->setDefaultAction(updateAction);
    connect(updateAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(getStats()));
    connect(MPDConnection::self(), SIGNAL(statsUpdated(const MPDStats &)), SLOT(statsUpdated(const MPDStats &)));
    connect(MPDConnection::self(), SIGNAL(version(long)), SLOT(mpdVersion(long)));
    connect(MPDConnection::self(), SIGNAL(version(long)), SIGNAL(getUrlHandlers()));
    connect(MPDConnection::self(), SIGNAL(urlHandlers(const QStringList &)), SLOT(urlHandlers(const QStringList &)));
    connect(this, SIGNAL(getUrlHandlers()), MPDConnection::self(), SLOT(getUrlHandlers()));

    MainWindow::initButton(updateInfo);
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

void ServerInfoPage::statsUpdated(const MPDStats &stats)
{
    dbUpdate=stats.dbUpdate;
    uptime->setText(MPDParseUtils::formatDuration(stats.uptime));
    timePlaying->setText(MPDParseUtils::formatDuration(stats.playtime));
    totalDuration->setText(MPDParseUtils::formatDuration(stats.dbPlaytime));
    artists->setText(QString::number(stats.artists));
    album->setText(QString::number(stats.albums));
    songs->setText(QString::number(stats.songs));
//     #ifdef ENABLE_KDE_SUPPORT
//     lastUpdate->setText(KGlobal::locale()->formatDateTime(stats.dbUpdate));
//     #else
    lastUpdate->setText(stats.dbUpdate.toString(Qt::SystemLocaleShortDate));
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
