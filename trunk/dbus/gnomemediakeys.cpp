/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "gnomemediakeys.h"
#include "mainwindow.h"
#include "settingsdaemoninterface.h"
#include "mediakeysinterface.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QCoreApplication>

static const char * constService = "org.gnome.SettingsDaemon";
static const char * constDaemonPath = "/org/gnome/SettingsDaemon";
static const char * constMediaKeysPath = "/org/gnome/SettingsDaemon/MediaKeys";

GnomeMediaKeys::GnomeMediaKeys(MainWindow *parent)
    : QObject(parent)
    , mw(parent)
    , daemon(0)
    , mk(0)
{
}

static bool runningUnderKde()
{
    const char *env=qgetenv("KDE_FULL_SESSION");
    if (env && 0==strcmp(env, "true")) {
        return true;
    }
    return QLatin1String("KDE")==QString(qgetenv("XDG_CURRENT_DESKTOP"));
}

void GnomeMediaKeys::setEnabled(bool en)
{
    if (en && !mk) {
        // Check if the service is available
        if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(constService) && !runningUnderKde()) {
            //...not already started, so attempt to start!
            QDBusConnection::sessionBus().interface()->startService(constService);
            if (!daemon) {
                daemon = new OrgGnomeSettingsDaemonInterface(constService, constDaemonPath, QDBusConnection::sessionBus(), this);
                connect(daemon, SIGNAL(PluginActivated(QString)), this, SLOT(pluginActivated(QString)));
                daemon->Start();
                return;
            }
        }
        grabKeys();
    } else if (!en && mk) {
        mk->ReleaseMediaPlayerKeys(QCoreApplication::applicationName());
        disconnect(mk, SIGNAL(MediaPlayerKeyPressed(QString,QString)), this, SLOT(keyPressed(QString,QString)));
        mk->deleteLater();
        mk=0;
        disconnectDaemon();
    }
}

void GnomeMediaKeys::grabKeys()
{
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(constService)) {
        if (!mk) {
            mk = new OrgGnomeSettingsDaemonMediaKeysInterface(constService, constMediaKeysPath, QDBusConnection::sessionBus(), this);
        }

        QDBusPendingReply<> reply = mk->GrabMediaPlayerKeys(QCoreApplication::applicationName(), QDateTime::currentDateTime().toTime_t());
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
        connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher *)), this, SLOT(registerFinished(QDBusPendingCallWatcher*)));
    }
}

void GnomeMediaKeys::disconnectDaemon()
{
    if (daemon) {
        disconnect(daemon, SIGNAL(PluginActivated(QString)), this, SLOT(pluginActivated(QString)));
        daemon->deleteLater();
        daemon=0;
    }
}

void GnomeMediaKeys::registerFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusMessage reply = watcher->reply();
    watcher->deleteLater();

    if (QDBusMessage::ErrorMessage!=reply.type()) {
        connect(mk, SIGNAL(MediaPlayerKeyPressed(QString, QString)),  this, SLOT(keyPressed(QString,QString)));
        disconnectDaemon();
    }
}

void GnomeMediaKeys::keyPressed(const QString &app, const QString &key)
{
    if (QCoreApplication::applicationName()!=app) {
        return;
    }
    if (QLatin1String("Play")==key) {
        mw->playPauseTrack();
    } else if (QLatin1String("Stop")==key) {
        mw->stopPlayback();
    } else if (QLatin1String("Next")==key) {
        mw->nextTrack();
    } else if (QLatin1String("Previous")==key) {
        mw->prevTrack();
    }
}

void GnomeMediaKeys::pluginActivated(const QString &name)
{
    if (QLatin1String("media-keys")==name) {
        grabKeys();
    }
}
