/*************************************************************************
** Cantata
**
** Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
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
    id: currentlyPlayingPage

    visible: false
    width: parent.width

    title: i18n.tr("Currently Playing")

    property int buttonSize: isPhone?units.gu(6):units.gu(7)

    actions: [
        Action {
            id: playPauseAction
            text: i18n.tr("Play/Pause")
            onTriggered: backend.playPause()
        },
        Action {
            id: goBackAction
            text: i18n.tr("Back")
            onTriggered: pageStack.pop()
        }
    ]

    tools: ToolbarItems {
        opened: true
        locked: root.width > units.gu(60) && opened //"&& opened": prevents the bar from being hidden and locked at the same time
        pageStack: pageStack
    }

    Row {
        id: controlsRow
        height: Math.max(buttonsRow.height, currentSongInfoColumn.height, volumeRectangle.height)
        width: buttonsRow.width + currentSongInfoColumn.width + spacing
        spacing: units.gu(2)
        visible: !isPhone

        anchors {
            top: parent.top
            left: parent.left
            topMargin: units.gu(1)
            leftMargin: units.gu(1)
        }

        ControlButtonsRow {
            id: buttonsRow
        }

        Column {
            id: currentSongInfoColumn
            anchors.verticalCenter: parent.verticalCenter
            property int preferedWidth: Math.max(titleLabel.width, artistLabel.width)
            property int restWidth: currentlyPlayingPage.width - buttonsRow.width - buttonsRow.anchors.leftMargin - volumeColumn.anchors.leftMargin - volumeColumn.anchors.rightMargin
            width: (restWidth - volumeColumn.preferedWidth < preferedWidth)?Math.min(preferedWidth, restWidth/2):preferedWidth

            Label {
                id: titleLabel
                text: backend.currentSongTitle
            }

            Label {
                id: artistLabel
                text: (backend.currentSongTitle != "")?backend.currentSongArtist + " - " + backend.currentSongAlbum + " (" + backend.currentSongYear + ")":"" //Previously: (!backend.playQueueEmpty && !backend.isStopped)
            }
        }
    }

    Rectangle {
        id: volumeRectangle
        color: root.backgroundColor
        anchors.verticalCenter: controlsRow.verticalCenter
        visible: !isPhone

        anchors {
            top: parent.top
            right: parent.right
            left: controlsRow.right
            topMargin: units.gu(1)
            leftMargin: Math.max(parent.width - controlsRow.width - buttonsRow.anchors.leftMargin - volumeColumn.preferedWidth, 0)
        }

        Column {
            id: volumeColumn
            property int preferedWidth: units.gu(30)

            anchors {
                top: parent.top
                right: parent.right
                left: parent.left
                leftMargin: controlsRow.spacing * 3 / 4
                rightMargin: units.gu(1)
            }

            Label {
                text: i18n.tr("Volume: ")
            }

            Slider {
                id: volumeSlider
                width: parent.width
                live: false
                minimumValue: 0
                maximumValue: 100
                value: backend.mpdVolume

                onValueChanged: {
                    backend.setMpdVolume(value)
                }

                Connections {
                    target: backend
                    onMpdVolumeChanged: volumeSlider.value = backend.mpdVolume
                }

                function formatValue(v) {
                    return Math.round(v) + "%"
                }
            }
        }
    }

    Column {
        id: currentSongInfoColumn2
        anchors {
            top: parent.top
            topMargin: units.gu(1)
        }
        width: parent.width
        spacing: units.gu(0.5)
        visible: isPhone && !(backend.playQueueEmpty || backend.isStopped) && backend.isConnected

        Label {
            id: titleLabel2
            width: parent.width
            text: backend.currentSongTitle
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            fontSize: "large"
        }

        Label {
            id: artistLabel2
            width: parent.width
            text: (backend.currentSongTitle != "")?backend.currentSongArtist + " - " + backend.currentSongAlbum + " (" + backend.currentSongYear + ")":""
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    ControlButtonsRow {
        id: buttonsRow2
        visible: currentSongInfoColumn2.visible

        anchors {
            top: currentSongInfoColumn2.bottom
            horizontalCenter: parent.horizontalCenter
        }
    }

    Label {
        id: playQueueLabel
        text: i18n.tr("Play Queue:")
        anchors {
            top: isPhone?(buttonsRow2.visible?buttonsRow2.bottom:parent.top):controlsRow.bottom
            left: parent.left
            topMargin: (isPhone)?units.gu(1):units.gu(2)
            leftMargin: units.gu(1)
        }
    }

    ListView {
        id: playqueueListView
        clip: true
        model: playQueueProxyModel
//        height: parent.height - playQueueLabel.height - (isPhone?(backend.playQueueEmpty?0:(buttonsRow2.height + currentSongInfoColumn2.height)):controlsRow.height) //Spacing fehlt in Phone???

        anchors {
            top: playQueueLabel.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            bottomMargin: isPhone?0:(-units.gu(2))
        }

        delegate: ListItem.Subtitled {
            text: model.mainText
            subText: model.subText
            confirmRemoval: true
            removable: true
            onItemRemoved: {
                backend.removeFromPlayQueue(index)
            }

            onClicked: backend.startPlayingSongAtPos(index)

            Image {
                id: playIndicatorImage
                source: "../../icons/toolbar/media-playback-start-light.svg"
                width: units.gu(3)
                height: units.gu(3)
                opacity: 0.9
                visible: index === backend.getCurrentSongPlayqueuePosition()

                anchors {
                    right: parent.right
                    rightMargin: units.gu(2)
                    verticalCenter: parent.verticalCenter
                }

                Connections {
                    target: backend

                    onCurrentSongPlayqueuePositionChanged: {
                        playIndicatorImage.visible = (index === backend.getCurrentSongPlayqueuePosition())
                    }
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
