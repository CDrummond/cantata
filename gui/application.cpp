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

#include "application.h"
#include "settings.h"
#include "support/proxystyle.h"
#include "models/mpdlibrarymodel.h"
#include "support/utils.h"
#include "mpd-interface/mpdstats.h"
#include "mpd-interface/mpdstatus.h"
#include "support/thread.h"
#include "tags/taghelperiface.h"
#include "scrobbling/scrobbler.h"
#include "support/fancytabwidget.h"
#include "widgets/itemview.h"
#include "widgets/groupedview.h"
#include "widgets/actionitemdelegate.h"
#include "http/httpserver.h"
#include "config.h"

void Application::init()
{
    #if defined Q_OS_WIN
    ProxyStyle *proxy=new ProxyStyle(ProxyStyle::VF_Side);
    #elif defined Q_OS_MAC
    ProxyStyle *proxy=ProxyStyle(ProxyStyle::VF_Side|ProxyStyle::VF_Top);
    #else
    ProxyStyle *proxy=new ProxyStyle(0);
    #endif
    QString theme = Settings::self()->style();
    if (!theme.isEmpty()) {
        QStyle *s=QApplication::setStyle(theme);
        if (s) {
            proxy->setBaseStyle(s);
        }
    }
    qApp->setStyle(proxy);

    // Ensure these objects are created in the GUI thread...
    ThreadCleaner::self();
    MPDStatus::self();
    MPDStats::self();
    #ifdef ENABLE_TAGLIB
    TagHelperIface::self();
    #endif
    Scrobbler::self();
    MpdLibraryModel::self();

    // Ensure this is started before any MPD connection
    HttpServer::self();

    Utils::initRand();
    Song::initTranslations();

    // Init sizes (before any widgets constructed!)
    ItemView::setup();
    FancyTabWidget::setup();
    GroupedView::setup();
    ActionItemDelegate::setup();
}
