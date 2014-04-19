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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KCmdLineArgs>
#include <KDE/KStartupInfo>
#include "initialsettingswizard.h"
#include "thread.h"
#include "song.h"
#else
#include <QIcon>
#ifdef Q_OS_WIN
#include <QDir>
#include <windows.h>
#endif
#ifdef Q_OS_MAC
#include <QDir>
#endif
#endif
#include "icon.h"
#include "icons.h"
#include "utils.h"
#include "config.h"
#include "mainwindow.h"
#include "mpdconnection.h"
#include "mpdstats.h"
#include "mpdstatus.h"
#include "thread.h"
#ifdef ENABLE_EXTERNAL_TAGS
#include "taghelperiface.h"
#endif
#include "gtkstyle.h"

void Application::initObjects()
{
    // Ensure these objects are created in the GUI thread...
    ThreadCleaner::self();
    MPDStatus::self();
    MPDStats::self();
    #ifdef ENABLE_EXTERNAL_TAGS
    TagHelperIface::self();
    #endif

    Utils::initRand();
    Song::initTranslations();
    Icon::setTouchFriendly(Settings::self()->touchFriendly());
}

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
}

Application::~Application() {
    if (w) {
        disconnect(w, SIGNAL(destroyed(QObject *)), this, SLOT(mwDestroyed(QObject *)));
        delete w;
        w=0;
    }
}

int Application::newInstance() {
    static bool in=false;
    if (in) {
        return 0;
    } else {
        in=true;
    }

    if (w) {
        w->restoreWindow();
    } else {
        initObjects();
        if (Settings::self()->firstRun()) {
            InitialSettingsWizard wz;
            if (QDialog::Rejected==wz.exec()) {
                QApplication::exit(0);
                return 0;
            }
        }
        w=new MainWindow();
        connect(w, SIGNAL(destroyed(QObject *)), this, SLOT(mwDestroyed(QObject *)));
        if (!Settings::self()->startHidden()) {
            w->show();
        }
    }

    KCmdLineArgs *args(KCmdLineArgs::parsedArgs());
    QStringList urls;
    for (int i = 0; i < args->count(); ++i) {
        urls.append(args->arg(i));
    }
    if (!urls.isEmpty()) {
        w->load(urls);
    }
    KStartupInfo::appStarted(startupId());
    in=false;
    return 0;
}

void Application::mwDestroyed(QObject *obj)
{
    if (obj==w) {
        w=0;
    }
}

#elif defined Q_OS_WIN || defined Q_OS_MAC
Application::Application(int &argc, char **argv)
    : QtSingleApplication(argc, argv)
{
    connect(this, SIGNAL(messageReceived(const QString &)), SLOT(message(const QString &)));
    #if defined Q_OS_WIN2
    connect(this, SIGNAL(reconnect()), MPDConnection::self(), SLOT(reconnect()));
    #endif
}

static void setupIconTheme()
{
    #ifdef Q_OS_MAC
    //QIcon::setThemeSearchPaths(QStringList(QCoreApplication::applicationDirPath() + "/../Resources/icons"));
    QIcon::setThemeName(QLatin1String("oxygen"));
    #else
    QString cfgTheme=Settings::self()->iconTheme();
    if (!cfgTheme.isEmpty()) {
        QIcon::setThemeName(cfgTheme);
    }

    QString theme=Icon::themeName().toLower();
    if (QLatin1String("oxygen")!=theme && !QIcon::hasThemeIcon("document-save-as")) {
        QStringList paths=QIcon::themeSearchPaths();

        foreach (const QString &p, paths) {
            if (QDir(p+QLatin1String("/oxygen")).exists()) {
                QIcon::setThemeName(QLatin1String("oxygen"));
                return;
            }
        }
    }
    #endif
}

#if defined Q_OS_WIN2
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
        QStringList args(arguments());
        if (args.count()>1) {
            args.takeAt(0);
            sendMessage(args.join("\n"));
        } else {
            sendMessage(QString());
        }
        return false;
    }

    setupIconTheme();
    return true;
}

void Application::message(const QString &msg)
{
    if (!msg.isEmpty()) {
        load(msg.split("\n"));
    }
    MainWindow *mw=qobject_cast<MainWindow *>(activationWindow());
    if (mw) {
        mw->restoreWindow();
    }
}

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

#else // Q_OS_WIN || Q_OS_MAC
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
}

bool Application::start()
{
    if (QDBusConnection::sessionBus().registerService(CANTATA_REV_URL)) {
        setupIconTheme();
        return true;
    }
    loadFiles();
    // ...and activate window!
    QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall("com.googlecode.cantata", "/org/mpris/MediaPlayer2", "", "Raise"));
    return false;
}

void Application::setupIconTheme()
{
    QString cfgTheme=Settings::self()->iconTheme();
    if (cfgTheme.isEmpty() && GtkStyle::isActive()) {
        cfgTheme=GtkStyle::iconTheme();
    }
    if (!cfgTheme.isEmpty()) {
        QIcon::setThemeName(cfgTheme);
    }

    // BUG:130 Some non-DE environments (IceWM, etc) seem to set theme to HiColor, and this has missing
    // icons. So check for a themed icon, if the current theme does not have this - then see if oxygen
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
    QStringList args(arguments());
    if (args.count()>1) {
        args.takeAt(0);
        QDBusMessage m = QDBusMessage::createMethodCall("com.googlecode.cantata", "/cantata", "", "load");
        QList<QVariant> a;
        a.append(args);
        m.setArguments(a);
        QDBusConnection::sessionBus().send(m);
    }
}

#endif
