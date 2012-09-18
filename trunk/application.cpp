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

#include "application.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KCmdLineArgs>
#include <KDE/KStartupInfo>
#include <KDE/KMessageBox>
#include <KDE/Solid/PowerManagement>
#else
#include <QtGui/QIcon>
#include <QtCore/QUrl>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#endif
#include "icon.h"
#include "utils.h"
#include "config.h"
#include "mainwindow.h"
#include "mpdconnection.h"

#ifdef ENABLE_KDE_SUPPORT
#ifdef Q_WS_X11
Application::Application(Display *display, Qt::HANDLE visual, Qt::HANDLE colormap)
    : KUniqueApplication(display, visual, colormap)
    , w(0)
{
}
#endif

Application::Application()
    : KUniqueApplication()
    , w(0)
{
    #if KDE_IS_VERSION(4, 7, 0)
    connect(Solid::PowerManagement::notifier(), SIGNAL(resumingFromSuspend()), MPDConnection::self(), SLOT(reconnect()));
    #endif
}

Application::~Application() {
    if (w) {
        delete w;
        w=0;
    }
}

int Application::newInstance() {
    if (w) {
        if (!w->isVisible()) {
            w->showNormal();
        }
    } else {
        if (0==Utils::getGroupId() && KMessageBox::Cancel==KMessageBox::warningContinueCancel(0,
                i18n("You are not currently a member of the \"users\" group. "
                        "Cantata will function better (saving of album covers, lyrics, etc. with the correct permissions) if you "
                        "(or your administrator) add yourself to this group.\n\n"
                        "Note, that if you do add yourself you will need to logout and back in for this to take effect.\n\n"
                        "Select \"Continue\" to start Cantata as is."),
                QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "groupWarning")) {
            QApplication::exit(0);
        }
        Icon::setupIconTheme();
        w=new MainWindow();
    }

    #ifdef TAGLIB_FOUND
    KCmdLineArgs *args(KCmdLineArgs::parsedArgs());
    QList<QUrl> urls;
    for (int i = 0; i < args->count(); ++i) {
        urls.append(QUrl(args->url(i)));
    }
    if (!urls.isEmpty()) {
        w->load(urls);
    }
    #endif
    KStartupInfo::appStarted(startupId());
    return 0;
}
#else // ENABLE_KDE_SUPPORT
Application::Application(int &argc, char **argv)
    : QtSingleApplication(argc, argv)
{
    #if defined TAGLIB_FOUND
    connect(this, SIGNAL(messageReceived(const QString &)), SLOT(message(const QString &)));
    #endif

    #ifdef Q_OS_WIN
    connect(this, SIGNAL(reconnect()), MPDConnection::self(), SLOT(reconnect()));
    #endif
}

Application::~Application()
{
}

#ifdef Q_OS_WIN
bool Application::winEventFilter(MSG *msg, long *result)
{
    if (msg && WM_POWERBROADCAST==msg->message && PBT_APMRESUMEAUTOMATIC==msg->wParam) {
        emit reconnect();
    }
    return QCoreApplication::winEventFilter(msg, result);
}
#endif

bool Application::start()
{
    if (isRunning()) {
        #ifdef TAGLIB_FOUND
        QStringList args(arguments());
        if (args.count()>1) {
            args.takeAt(0);
            sendMessage(args.join("\n"));
        }
        #endif
        return false;
    }

    Icon::setupIconTheme();
    return true;
}

#if defined TAGLIB_FOUND
void Application::loadFiles()
{
    QStringList args(arguments());
    if (args.count()>1) {
        args.takeAt(0);
        load(args);
    }
}

void Application::message(const QString &msg)
{
    load(msg.split("\n"));
}

void Application::load(const QStringList &files)
{
    if (files.isEmpty()) {
        return;
    }

    QList<QUrl> urls;
    foreach (const QString &f, files) {
        urls.append(QUrl(f));
    }
    if (!urls.isEmpty()) {
        qobject_cast<MainWindow *>(activationWindow())->load(urls);
    }
}
#endif // TAGLIB_FOUND

#endif
