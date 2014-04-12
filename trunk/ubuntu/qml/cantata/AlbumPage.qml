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
import 'qrc:/qml/cantata/'

Page {
    id: albumPage

    anchors.fill: parent
    title: i18n.tr("Albums")

    actions: [
        Action {
            id: currentlyPlayingAction
            text: i18n.tr("Currently playing")
            onTriggered: pageStack.push(currentlyPlayingPage)
        },
        Action {
            id: settingsAction
            text: i18n.tr("Settings")
            onTriggered: pageStack.push(hostSettingsPage)
        },
        Action {
            id: aboutAction
            text: i18n.tr("About")
            onTriggered: pageStack.push(aboutPage)
        }
    ]

    tools: ToolbarItems {
        opened: true
        locked: root.width > units.gu(60) && opened //"&& opened": prevents the bar from being hidden and locked at the same time

        ToolbarButton {
            iconSource: Qt.resolvedUrl("../../icons/toolbar/media-playback-start.svg")
            action: Action {
                text: i18n.tr("Playing")
                onTriggered: pageStack.push(currentlyPlayingPage)
            }
        }

        ToolbarButton {
            iconSource: Qt.resolvedUrl("../../icons/toolbar/settings.svg")
            action: Action {
                text: i18n.tr("Settings")
                onTriggered: pageStack.push(hostSettingsPage)
            }
        }

        ToolbarButton {
            iconSource: Qt.resolvedUrl("../../icons/toolbar/help.svg")
            action: Action {
                text: i18n.tr("About")
                onTriggered: pageStack.push(aboutPage)
            }
        }
    }

    ListView {
        id: albumListView
        anchors {
            fill: parent
            bottomMargin: isPhone?0:(-units.gu(2))
        }
        model: albumsProxyModel
        clip: true

        delegate: PlayQueueListItemDelegate {
            text: model.mainText
            subText: model.subText
//            icon: model.image
//           progression: true //Removed due to the app showdown, will be implemented later

//            Image {
//                id: addImage
//                width: units.gu(3)
//                height: units.gu(3)
//                smooth: true
//                source: "../../icons/toolbar/add.svg"
//                opacity: 0.9

//                anchors {
//                    right: parent.right
//                    rightMargin: units.gu(4)
//                    verticalCenter: parent.verticalCenter
//                }

//                MouseArea {
//                    onClicked: {
//                        backend.addAlbum(index, false)
//                        pageStack.push(currentlyPlayingPage)
//                    }
//                    anchors.fill: parent
//                    preventStealing: true
//                }
//            }
//            Image {
//                width: units.gu(3)
//                height: units.gu(3)
//                smooth: true
//                source: "../../icons/toolbar/media-playback-start-light.svg"
//                opacity: 0.9

//                anchors {
//                    right: parent.right
//                    rightMargin: units.gu(0)
//                    verticalCenter: parent.verticalCenter
//                }

//                MouseArea {
//                    onClicked: {
//                        backend.addAlbum(index, true)
//                        pageStack.push(currentlyPlayingPage)
//                    }
//                    anchors.fill: parent
//                    preventStealing: true
//                }
//            }
        }
    }

    Label {
        anchors.centerIn: parent
        text: i18n.tr("No albums found")
        fontSize: "large"
        visible: !backend.albumsFound
    }
}
