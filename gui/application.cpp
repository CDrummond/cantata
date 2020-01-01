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

#include <QListView>
#include "application.h"
#include "settings.h"
#include "support/proxystyle.h"
#include "models/mpdlibrarymodel.h"
#include "models/playlistsmodel.h"
#include "models/streamsmodel.h"
#include "support/utils.h"
#include "mpd-interface/mpdstats.h"
#include "mpd-interface/mpdstatus.h"
#include "support/thread.h"
#include "tags/taghelperiface.h"
#include "scrobbling/scrobbler.h"
#include "support/fancytabwidget.h"
#include "support/combobox.h"
#include "widgets/itemview.h"
#include "widgets/groupedview.h"
#include "widgets/actionitemdelegate.h"
#include "widgets/toolbutton.h"
#include "widgets/sizegrip.h"
#include "http/httpserver.h"
#include "network/networkproxyfactory.h"
#include "config.h"

void Application::init()
{
    #if defined Q_OS_WIN
    ProxyStyle *proxy=new ProxyStyle(ProxyStyle::VF_Side);
    #elif defined Q_OS_MAC
    ProxyStyle *proxy=new ProxyStyle(ProxyStyle::VF_Side|ProxyStyle::VF_Top);
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

    #ifdef Q_OS_WIN
    // Qt does not seem to consistently apply the application font under Windows.
    // To work-around this, set the application font to that used for a listview.
    // Issues #1097 and #1109
    QListView view;
    view.ensurePolished();
    QApplication::setFont(view.font());
    #endif

    // Ensure these objects are created in the GUI thread...
    ThreadCleaner::self();
    MPDStatus::self();
    MPDStats::self();
    #ifdef ENABLE_TAGLIB
    TagHelperIface::self();
    #endif
    NetworkProxyFactory::self();
    Scrobbler::self();
    MpdLibraryModel::self();
    PlaylistsModel::self();
    StreamsModel::self();

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

void Application::fixSize(QWidget *widget)
{
    static int fixedHeight = -1;
    if (-1 == fixedHeight) {
        ComboBox c(widget);
        ToolButton b(widget);
        SizeGrip g(widget);
        c.ensurePolished();
        b.ensurePolished();
        g.ensurePolished();
        fixedHeight=qMax(24, qMax(c.sizeHint().height(), qMax(b.sizeHint().height(), g.sizeHint().height())));
        if (fixedHeight%2) {
            fixedHeight--;
        }
    }

    QToolButton *tb=qobject_cast<QToolButton *>(widget);
    if (tb) {
        tb->setFixedSize(fixedHeight, fixedHeight);
    } else {
        #ifdef Q_OS_MAC
        // TODO: Why is this +(2*focus) required for macOS? If its not used, library page's statusbar is larger
        // than the rest - due to genre combo?
        widget->setFixedHeight(fixedHeight+(2*widget->style()->pixelMetric(QStyle::PM_FocusFrameHMargin)));
        #else
        if (0==qstrcmp("QWidget", widget->metaObject()->className())) {
            widget->setFixedHeight(fixedHeight+Utils::scaleForDpi(4));
        } else {
            widget->setFixedHeight(fixedHeight);
        }
        #endif
    }
}
