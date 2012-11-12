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
#include "settings.h"
#include <QtGui/QIcon>
#ifdef Q_OS_WIN
#include <QtCore/QDir>
#include <windows.h>
#endif
#endif
#include "icon.h"
#include "icons.h"
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
        disconnect(w, SIGNAL(destroyed(QObject *)), this, SLOT(mwDestroyed(QObject *)));
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
        Icons::init();
        w=new MainWindow();
        connect(w, SIGNAL(destroyed(QObject *)), this, SLOT(mwDestroyed(QObject *)));
    }

    #ifdef TAGLIB_FOUND
    KCmdLineArgs *args(KCmdLineArgs::parsedArgs());
    QStringList urls;
    for (int i = 0; i < args->count(); ++i) {
        urls.append(args->arg(i));
    }
    if (!urls.isEmpty()) {
        w->load(urls);
    }
    #endif
    KStartupInfo::appStarted(startupId());
    return 0;
}

void Application::mwDestroyed(QObject *obj)
{
    if (obj==w) {
        w=0;
    }
}

#elif defined Q_OS_WIN
Application::Application(int &argc, char **argv)
    : QtSingleApplication(argc, argv)
{
    #if defined TAGLIB_FOUND
    connect(this, SIGNAL(messageReceived(const QString &)), SLOT(message(const QString &)));
    #endif

    connect(this, SIGNAL(reconnect()), MPDConnection::self(), SLOT(reconnect()));
}

static void setupIconTheme()
{
    QString cfgTheme=Settings::self()->iconTheme();
    if (!cfgTheme.isEmpty()) {
        QIcon::setThemeName(cfgTheme);
    }

    QString theme=QIcon::themeName();
    if (QLatin1String("oxygen")!=theme && !QIcon::hasThemeIcon("document-save-as")) {
        QStringList paths=QIcon::themeSearchPaths();

        foreach (const QString &p, paths) {
            if (QDir(p+QLatin1String("/oxygen")).exists()) {
                QIcon::setThemeName(QLatin1String("oxygen"));
                return;
            }
        }
    }
}

bool Application::winEventFilter(MSG *msg, long *result)
{
    if (msg && WM_POWERBROADCAST==msg->message && PBT_APMRESUMEAUTOMATIC==msg->wParam) {
        emit reconnect();
    }
    return QCoreApplication::winEventFilter(msg, result);
}

bool Application::start()
{
    if (isRunning()) {
        #ifdef TAGLIB_FOUND
        QStringList args(arguments());
        if (args.count()>1) {
            args.takeAt(0);
            sendMessage(args.join("\n"));
        } else
        #endif
            sendMessage(QString());
        return false;
    }

    setupIconTheme();
    Icons::init();
    return true;
}

void Application::message(const QString &msg)
{
    #if defined TAGLIB_FOUND
    if (!msg.isEmpty()) {
        load(msg.split("\n"));
    }
    #endif
    MainWindow *mw=qobject_cast<MainWindow *>(activationWindow());
    if (mw) {
        mw->restoreWindow();
    }
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

void Application::load(const QStringList &files)
{
    if (files.isEmpty()) {
        return;
    }

    QStringList urls;
    foreach (const QString &f, files) {
        urls.append(f);
    }
    if (!urls.isEmpty()) {
        MainWindow *mw=qobject_cast<MainWindow *>(activationWindow());
        if (mw) {
            mw->load(urls);
        }
    }
}
#endif // TAGLIB_FOUND

#else // Q_OS_WIN
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtCore/QDir>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
}

bool Application::start()
{
    if (QDBusConnection::sessionBus().registerService("org.kde.cantata")) {
        setupIconTheme();
        Icons::init();
        return true;
    }
    loadFiles();
    return false;
}

void Application::setupIconTheme()
{
    QString cfgTheme=Settings::self()->iconTheme();
    if (!cfgTheme.isEmpty()) {
        QIcon::setThemeName(cfgTheme);
    }

    // BUG:130 Some non-DE environments (IceWM, etc) seem to set theme to HiColor, and this has missing
    // icons. So cehck for a themed icon,if the current theme does not have this - then see if oxygen
    // or gnome icon themes are installed, and set theme to one of those.
    if (!QIcon::hasThemeIcon("document-save-as")) {
        QStringList themes=QStringList() << QLatin1String("oxygen") << QLatin1String("gnome");
        foreach (const QString &theme, themes) {
            QString dir(QLatin1String("/usr/share/icons/")+theme);
            if (QDir(dir).exists()) {
                QIcon::setThemeName(theme);
                return;
            }
        }
    }
}

void Application::loadFiles()
{
    #ifdef TAGLIB_FOUND
    QStringList args(arguments());
    if (args.count()>1) {
        args.takeAt(0);
        QDBusMessage m = QDBusMessage::createMethodCall("org.kde.cantata", "/cantata", "", "load");
        QList<QVariant> a;
        a.append(args);
        m.setArguments(a);
        QDBusConnection::sessionBus().send(m);
    }
    #endif
}

#endif
