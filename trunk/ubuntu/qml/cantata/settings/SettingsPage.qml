/*************************************************************************
** Cantata
**
** Copyright (c) 2014 Craig Drummond <craig.p.drummond@gmail.com>
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
import Ubuntu.Layouts 0.1

Page {
    id: settingsPage

    visible: false
    anchors.fill: parent
    title: i18n.tr("Settings")

    property HostSettingsPage hostSettings: hostSettingsPage

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

        Component.onCompleted: {
            back.triggered.connect(test)
        }

        function test() {
            console.log("asdf")
        }
    }

    Layouts {
        id: layouts
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            topMargin: root.header.height
        }

        layouts: [
            ConditionalLayout {
                name: "phone"
                when: layouts.width <= units.gu(80)

                SettingsCategories {
                    id: phoneCategories
                    push: true
                    anchors.fill: parent
                }
            },
            ConditionalLayout {
                name: "tablet"
                when: layouts.width > units.gu(80)

                Item {
                    anchors.fill: parent

                    SettingsCategories {
                        id: tabletCategories
                        push: false

                        width: Math.min(parent.width / 3, units.gu(50))

                        onSelectedIndexChanged: {
                            switch (selectedIndex) {
                            case 0:
                                stack.setSource("HostSettingsContent.qml")
                                break
                            case 1:
                                stack.setSource("UiSettingsContent.qml")
                                break
                            case 2:
                                stack.setSource("PlaybackSettingsContent.qml")
                                break
                            }
                        }

                        anchors {
                            top: parent.top
                            left: parent.left
                            bottom: parent.bottom
                        }
                    }

                    Loader {
                        id: stack
                        source: "HostSettingsContent.qml"

                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            left: tabletCategories.right
                            right: parent.right
                            topMargin: units.gu(2)
                            rightMargin: units.gu(2)
                            bottomMargin: units.gu(2)
                            leftMargin: units.gu(2)
                        }
                    }
                }
            }
        ]

    }


}

