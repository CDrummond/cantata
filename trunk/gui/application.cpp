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

#include "application.h"
#include "settings.h"
#include "support/utils.h"
#include "mpd/mpdstats.h"
#include "mpd/mpdstatus.h"
#include "support/thread.h"
#ifdef ENABLE_EXTERNAL_TAGS
#include "tags/taghelperiface.h"
#endif
#include "scrobbling/scrobbler.h"
#include "support/fancytabwidget.h"
#include "widgets/itemview.h"
#include "widgets/groupedview.h"
#include "widgets/actionitemdelegate.h"

void Application::initObjects()
{
    // Ensure these objects are created in the GUI thread...
    ThreadCleaner::self();
    MPDStatus::self();
    MPDStats::self();
    #ifdef ENABLE_EXTERNAL_TAGS
    TagHelperIface::self();
    #endif
    Scrobbler::self();

    Utils::initRand();
    Song::initTranslations();
    Utils::setTouchFriendly(Settings::self()->touchFriendly());

    // Init sizes (before any widgets constructed!)
    ItemView::setup();
    FancyTabWidget::setup();
    GroupedView::setup();
    ActionItemDelegate::setup();
}
