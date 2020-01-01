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

#include "gnomemediakeys.h"
#include "settingsdaemoninterface.h"
#include "mediakeysinterface.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QCoreApplication>

static const char * constOrigService = "org.gnome.SettingsDaemon";
static const char * constNewService = "org.gnome.SettingsDaemon.MediaKeys";
static const char * constDaemonPath = "/org/gnome/SettingsDaemon";
static const char * constMediaKeysPath = "/org/gnome/SettingsDaemon/MediaKeys";

GnomeMediaKeys::GnomeMediaKeys(QObject *p)
    : MultiMediaKeysInterface(p)
    , daemon(nullptr)
    , mk(nullptr)
    , watcher(nullptr)
{
}

bool GnomeMediaKeys::activate()
{
    if (mk) {
        return true;
    }
    if (daemonIsRunning()) {
        grabKeys();
        return true;
    }
    return false;
}

void GnomeMediaKeys::deactivate()
{
    if (mk) {
        releaseKeys();
        disconnectDaemon();
        if (watcher) {
            watcher->deleteLater();
            watcher=nullptr;
        }
    }
}

bool GnomeMediaKeys::daemonIsRunning()
{
    // Check if the service is available
    if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(constNewService) &&
        !QDBusConnection::sessionBus().interface()->isServiceRegistered(constOrigService)) {
        //...not already started, so attempt to start!
        QDBusConnection::sessionBus().interface()->startService(constOrigService); // ??
        if (!daemon) {
            daemon = new OrgGnomeSettingsDaemonInterface(constOrigService, constDaemonPath, QDBusConnection::sessionBus(), this);
            connect(daemon, SIGNAL(PluginActivated(QString)), this, SLOT(pluginActivated(QString)));
            daemon->Start();
            return false;
        }
    }
    return true;
}

void GnomeMediaKeys::releaseKeys()
{
    if (mk) {
        mk->ReleaseMediaPlayerKeys(QCoreApplication::applicationName());
        disconnect(mk, SIGNAL(MediaPlayerKeyPressed(QString,QString)), this, SLOT(keyPressed(QString,QString)));
        mk->deleteLater();
        mk=nullptr;
    }
}

void GnomeMediaKeys::grabKeys()
{
    QStringList services { constNewService, constOrigService };
    for (const auto &service: services) {
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(service)) {
            if (!mk) {
                mk = new OrgGnomeSettingsDaemonMediaKeysInterface(service, constMediaKeysPath, QDBusConnection::sessionBus(), this);
            }

            QDBusPendingReply<> reply = mk->GrabMediaPlayerKeys(QCoreApplication::applicationName(), QDateTime::currentDateTime().toTime_t());
            QDBusPendingCallWatcher *callWatcher = new QDBusPendingCallWatcher(reply, this);
            connect(callWatcher, SIGNAL(finished(QDBusPendingCallWatcher *)), this, SLOT(registerFinished(QDBusPendingCallWatcher*)));

            if (!watcher) {
                watcher = new QDBusServiceWatcher(this);
                watcher->setConnection(QDBusConnection::sessionBus());
                watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
                connect(watcher, SIGNAL(serviceOwnerChanged(QString, QString, QString)), this, SLOT(serviceOwnerChanged(QString, QString, QString)));
            }
            serviceName = service;
            break;
        }
    }
}

void GnomeMediaKeys::disconnectDaemon()
{
    if (daemon) {
        disconnect(daemon, SIGNAL(PluginActivated(QString)), this, SLOT(pluginActivated(QString)));
        daemon->deleteLater();
        daemon=nullptr;
    }
}

void GnomeMediaKeys::serviceOwnerChanged(const QString &name, const QString &, const QString &)
{
    if (name==serviceName) {
        releaseKeys();
        disconnectDaemon();
        if (daemonIsRunning()) {
            grabKeys();
        }
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
        emit playPause();
    } else if (QLatin1String("Stop")==key) {
        emit stop();
    } else if (QLatin1String("Next")==key) {
        emit next();
    } else if (QLatin1String("Previous")==key) {
        emit previous();
    }
}

void GnomeMediaKeys::pluginActivated(const QString &name)
{
    if (QLatin1String("media-keys")==name) {
        grabKeys();
    }
}

#include "moc_gnomemediakeys.cpp"
