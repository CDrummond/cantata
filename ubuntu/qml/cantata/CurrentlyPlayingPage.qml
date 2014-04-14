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
import Ubuntu.Components.Popups 0.1
import 'components'

Page {
    id: currentlyPlayingPage

    visible: false
    width: parent.width

    title: i18n.tr("Currently Playing")

    property int buttonSize: isPhone?units.gu(6):units.gu(7)

    property Popover actionsPopover;

    Component {
         id: dialog
         Dialog {
             id: dialogue
             title: i18n.tr("Clear Play Queue")

             Button {
                 text: i18n.tr("Yes")
                 color: UbuntuColors.orange
                 onClicked: (backend.clearPlayQueue(), PopupUtils.close(dialogue))
             }
             Button {
                 text: i18n.tr("No")
                 color: UbuntuColors.coolGrey
                 onClicked: PopupUtils.close(dialogue)
             }
         }
    }
    
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
        locked: (root.width > units.gu(60) && opened) || (actionsPopover != null && actionsPopover.visible) //"&& opened": prevents the bar from being hidden and locked at the same time
        pageStack: pageStack
        ToolbarButton {
            iconSource: Qt.resolvedUrl("../../icons/toolbar/clear.svg")
            action: Action {
                text: i18n.tr("Clear")
                onTriggered: PopupUtils.open(dialog)
            }
        }
        ToolbarButton {
            id: actionActionToolbarButton
            iconSource: Qt.resolvedUrl("../../icons/toolbar/navigation-menu.svg")
            action: Action {
                text: i18n.tr("Actions")
                onTriggered: actionsPopover = PopupUtils.open(actionsPopoverComponent, actionActionToolbarButton)
            }
            visible: isPhone
        }
    }

    Component {
        id: actionsPopoverComponent

        Popover {
            id: actionsPopover

            Column {
                id: containerLayout
                anchors {
                    left: parent.left
                    top: parent.top
                    right: parent.right
                }

                ListItem.Header { text: i18n.tr("Volume") }
                ListItem.Standard {
                    anchors {
                        left: parent.left
                        right: parent.right
                        leftMargin: units.gu(1)
                        rightMargin: units.gu(1)
                    }
                    Row {
                        width: parent.width
                        spacing: units.gu(1)

                        Image {
                            id: speakerImage
                            height: units.gu(3)
                            width: units.gu(3)
                            smooth: true
                            anchors.verticalCenter: parent.verticalCenter
                            source: "../../icons/toolbar/speaker.svg"
                        }

                        Slider {
                            id: volumeSlider2
                            width: parent.width - speakerImage.width - parent.spacing
                            live: false
                            minimumValue: 0
                            maximumValue: 100
                            value: backend.mpdVolume

                            onValueChanged: {
                                backend.setMpdVolume(value)
                            }

                            Connections {
                                target: backend
                                onMpdVolumeChanged: volumeSlider2.value = backend.mpdVolume
                            }

                            function formatValue(v) {
                                return Math.round(v) + "%"
                            }
                        }
                    }
                }
            }
        }
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
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                text: backend.currentSongMainText
                fontSize: "large"
            }

            Label {
                id: artistLabel
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                text: backend.currentSongSubText
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
            text: backend.currentSongMainText
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            fontSize: "large"
        }

        Label {
            id: artistLabel2
            width: parent.width
            text: backend.currentSongSubText
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
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
//        height: parent.height - playQueueLabel.height - (isPhone?(backend.playQueueEmpty?0:(buttonsRow2.height + currentSongInfoColumn2.height)):controlsRow.height) //Spacing fehlt in Phone???

        anchors {
            top: playQueueLabel.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        delegate: ListItemDelegate {
            id: delegate
            text: model.mainText
            subText: model.subText
            confirmRemoval: true
            removable: true
            onItemRemoved: {
                backend.removeFromPlayQueue(index)
            }

            onClicked: backend.startPlayingSongAtPos(index)

            firstButtonImageSource: "../../icons/toolbar/media-playback-start-light.svg"
            firstButtonShown: index === backend.getCurrentSongPlayqueuePosition()

            Connections {
                target: backend

                onCurrentSongPlayqueuePositionChanged: {
                    delegate.firstButtonShown = (index === backend.getCurrentSongPlayqueuePosition())
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
