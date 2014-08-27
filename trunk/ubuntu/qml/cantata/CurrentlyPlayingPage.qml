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
import 'qrc:/qml/cantata/components'

Page {
    id: currentlyPlayingPage

    title: i18n.tr("Currently Playing")

    property Dialog actionsDialog

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

    head.actions: [ //TODO: Better solution when using just the content
        Action {
            iconName: "edit-clear"
            text: i18n.tr("Clear")
            onTriggered: PopupUtils.open(dialog)
        },

        Action {
            iconName: "stock_music"
            text: i18n.tr("Actions")
            onTriggered: actionsDialog = PopupUtils.open(actionsDialogComponent)
        }
    ]

    Component {
        id: actionsDialogComponent

        Dialog {
            id: actionsDialog

            title: i18n.tr("Playback Options")

            contents: [
                Column {
                    id: containerLayout

                    ListItem.Header { text: i18n.tr("Playback") }
                    ListItem.Standard {
                        Label {
                            anchors {
                                left: parent.left
                                leftMargin: units.gu(1)
                                verticalCenter: parent.verticalCenter
                            }

                            color: volumeHeader.color

                            text: i18n.tr("Repeat")
                        }

                        CheckBox {
                            id: repeatCheckBox
                            anchors {
                                right: parent.right
                                rightMargin: units.gu(1)
                                verticalCenter: parent.verticalCenter
                            }

                            checked: backend.isRepeating

                            Connections {
                                target: backend
                                onIsRepeatingChanged: repeatCheckBox.checked = backend.isRepeating
                            }

                            onTriggered: {
                                if (checked !== backend.isRepeating) {
                                    backend.setIsRepeating(checked)
                                }
                            }
                        }
                    }
                    ListItem.Standard {
                        Label {
                            anchors {
                                left: parent.left
                                leftMargin: units.gu(1)
                                verticalCenter: parent.verticalCenter
                            }

                            color: volumeHeader.color

                            text: i18n.tr("Random")
                        }

                        CheckBox {
                            id: randomCheckBox
                            anchors {
                                right: parent.right
                                rightMargin: units.gu(1)
                                verticalCenter: parent.verticalCenter
                            }

                            checked: backend.isRandomOrder

                            Connections {
                                target: backend
                                onIsRandomOrderChanged: randomCheckBox.checked = backend.isRandomOrder
                            }

                            onTriggered: {
                                if (checked !== backend.isRandomOrder) {
                                    backend.setIsRandomOrder(checked)
                                }
                            }
                        }
                    }

                    ListItem.Header {
                        id: volumeHeader
                        text: i18n.tr("Volume")
                        visible: isPhone
                    }

                    ListItem.Standard {
                        anchors {
                            left: parent.left
                            right: parent.right
                            leftMargin: units.gu(1)
                            rightMargin: units.gu(1)
                        }
                        visible: isPhone
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

                    Item {
                        id: dialogSpacer

                        height: units.gu(3)
                        width: parent.width
                    }

                    Button {
                         text: i18n.tr("Ready")
                         color: UbuntuColors.orange

                         anchors {
                             left: parent.left
                             right: parent.right
                             leftMargin: units.gu(1)
                             rightMargin: units.gu(1)
                         }

                         onClicked: PopupUtils.close(actionsDialog)
                    }
                }


            ]
        }
    }

    CurrentlyPlayingContent {
        id: currentlyPlayingContent

        anchors.fill: parent
    }
}
