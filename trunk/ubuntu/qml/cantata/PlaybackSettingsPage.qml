/*************************************************************************
** Cantata
**
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

import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem

Page {
    id: playbackSettingsPage

    visible: false
    width: parent.width
    title: i18n.tr("Playback Settings")

    property color textFieldColor: "#c2c2b8" //#f3f3e7 * 0.8 (#f3f3e7: label color)

    actions: [
        Action {
            id: backAction
            text: i18n.tr("Back")
            onTriggered: pageStack.pop()
        }
    ]

    tools: ToolbarItems {
        opened: true
        locked: root.width > units.gu(60) && opened //"&& opened": prevents the bar from being hidden and locked at the same time
        pageStack: pageStack
    }

    Column {
        id: column
        spacing: units.gu(1)
        width: Math.round(parent.width / 1.3)
        height: parent.height - parent.header.height
        y: parent.header.height + units.gu(2)
        anchors.horizontalCenter: parent.horizontalCenter

        Component.onCompleted: {
        }

        Label {
            id: todoLabel
            text: i18n.tr("TODO!!!")
            anchors {
                left: column.left;
                right: column.right;
            }

            fontSize: "medium"
        }
    }
}

