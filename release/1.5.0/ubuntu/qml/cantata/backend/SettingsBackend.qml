/*************************************************************************
** Cantata
**
** Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
** Copyright (c) 2014 Craig Drummond <craig.p.drummond@gmail.com>
**
** $QT_BEGIN_LICENSE:GPL$
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING.  If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
**
** $QT_END_LICENSE$
**
*************************************************************************/

import QtQuick 2.2
import Ubuntu.Components 1.1
import Qt.labs.settings 1.0
import 'qrc:/qml/cantata/'

Item {

    readonly property alias uiSettings: uiSettings

    readonly property alias scrollPlayQueue: uiSettings.scrollPlayQueue
    readonly property alias fetchCovers: uiSettings.fetchCovers

    Component.onCompleted: {
        backend.setCoverFetch(fetchCovers)
    }

    onFetchCoversChanged: {
        backend.setCoverFetch(fetchCovers)
    }

    Settings {
        id: uiSettings

        category: "ui"
        property bool scrollPlayQueue: true
        property bool fetchCovers: true
    }

}
