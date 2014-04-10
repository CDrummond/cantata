/*
 * Cantata
 *
 * Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
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

#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuick/QQuickPaintedItem>
#include <QStandardPaths>
#include <QDir>

#include "mpdconnection.h"
#include "song.h"
#include "mpdparseutils.h"
#include "settings.h"
#include "thread.h"
#include "ubuntu/backend/mpdbackend.h"
#include "utils.h"
#include "albumsmodel.h"
#include "playlistsmodel.h"
#include "currentcover.h"
#include "config.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(CANTATA_REV_URL);
    QThread::currentThread()->setObjectName("GUI");

    Utils::initRand();
    Song::initTranslations();

    // Ensure these objects are created in the GUI thread...
    ThreadCleaner::self();
    MPDStatus::self();
    MPDStats::self();
    MPDConnection::self();

    MPDBackend backend;

    QGuiApplication app(argc, argv);
    qmlRegisterType<MPDBackend>("MPDBackend", 1, 0, "MPDBackend");
    QQuickView view;
    view.setMinimumSize(QSize(360, 540));
    view.rootContext()->setContextProperty("albumsModel", AlbumsModel::self());
    view.rootContext()->setContextProperty("backend", &backend);
    view.rootContext()->setContextProperty("albumsProxyModel", &backend.albumsProxyModel);
    view.rootContext()->setContextProperty("playQueueModel", &backend.playQueueModel);
    view.rootContext()->setContextProperty("playQueueProxyModel", &backend.playQueueProxyModel);
    view.rootContext()->setContextProperty("currentCover", CurrentCover::self());
    view.rootContext()->setContextProperty("appDir", Utils::dataDir(QString(), true));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    //view.setSource(QUrl("qrc:qml/cantata/main.qml"));
    view.setSource(QUrl::fromLocalFile("ubuntu/qml/cantata/main.qml"));
    view.show();

    AlbumsModel::self()->setEnabled(true);

    return app.exec();
}
