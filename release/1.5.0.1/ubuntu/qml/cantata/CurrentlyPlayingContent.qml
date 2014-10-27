/*************************************************************************
** Cantata
**
** Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
 * Copyright (c) 2014 Craig Drummond <craig.p.drummond@gmail.com>
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
import Ubuntu.Components.ListItems 1.0 as ListItem
import Ubuntu.Components.Popups 1.0
import Qt.labs.settings 1.0
import 'qrc:/qml/cantata/components'

Item {
    id: currentlyPlayingContent

    property int buttonSize: isPhone ? units.gu(6) : units.gu(7)

    Column {
        id: currentSongInfoColumn
        anchors {
            top: parent.top
            topMargin: units.gu(1)
        }
        width: parent.width
        spacing: units.gu(0.5)
        visible: !(backend.playQueueEmpty || backend.isStopped) && backend.isConnected

        Label {
            id: titleLabel
            width: parent.width
            text: backend.currentSongMainText
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            fontSize: "large"
        }

        Label {
            id: artistLabel
            width: parent.width
            text: backend.currentSongSubText
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
        }
    }

    ControlButtonsRow {
        id: buttonsRow
        visible: currentSongInfoColumn.visible

        anchors {
            top: currentSongInfoColumn.bottom
            horizontalCenter: parent.horizontalCenter
        }
    }

    Label {
        id: playQueueLabel
        text: i18n.tr("Play Queue:")
        anchors {
            top: buttonsRow.visible?buttonsRow.bottom:parent.top
            left: parent.left
            topMargin: units.gu(1)
            leftMargin: units.gu(1)
        }
    }

    Label {
        id: playQueueStatusLabel
        text: backend.playQueueStatus
        anchors {
            top: playQueueLabel.top
            right: parent.right
            rightMargin: units.gu(1)
        }
        visible: !backend.playQueueEmpty && backend.isConnected;
    }

    ListView {
        id: playqueueListView
        clip: true
        model: playQueueProxyModel

        anchors {
            top: playQueueLabel.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        Connections {
            target: backend
            onCurrentSongPlayqueuePositionChanged: {
                playqueueListView.scrollToCurrent()
            }
        }

        Component.onCompleted: { //TODO-r: Scroll to current position, but wrong numbers during swipe gesture
            scrollToCurrent()
        }

        function scrollToCurrent() {
            if (settingsBackend.scrollPlayQueue) {
                console.log("pos: " + backend.getCurrentSongPlayqueuePosition())
                playqueueListView.positionViewAtIndex(backend.getCurrentSongPlayqueuePosition(), ListView.Contain)
            }
        }

        delegate: PlayQueueListItemDelegate {
            id: delegate
            text: model.mainText
            subText: model.subText
            timeText: model.time
            iconSource: model.image
            confirmRemoval: true
            removable: true
            currentTrack: index === backend.getCurrentSongPlayqueuePosition()
            onItemRemoved: {
                backend.removeFromPlayQueue(index)
            }

            onClicked: backend.startPlayingSongAtPos(index)

            Connections {
                target: backend
                onCurrentSongPlayqueuePositionChanged: {
                    delegate.currentTrack = (index === backend.getCurrentSongPlayqueuePosition())
                }
            }
        }
    }

    Label {
        anchors.centerIn: playqueueListView
        text: i18n.tr("No songs queued for playing")
        fontSize: "large"
        visible: backend.playQueueEmpty || !backend.isConnected
    }
}
