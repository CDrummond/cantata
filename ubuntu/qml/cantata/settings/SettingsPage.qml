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

import QtQuick 2.2
import Ubuntu.Components 1.1
import Ubuntu.Components.ListItems 1.0 as ListItem
import Ubuntu.Layouts 1.0

Page {
    id: settingsPage
    objectName: "settingsPage"

    title: qsTr(i18n.tr("Settings %1")).arg((layouts.currentLayout === "tablet" && (categories.selectedIndex === 0 || categories.selectedIndex === -1)) ? backend.isConnected?i18n.tr("(Connected)"):i18n.tr("(Not Connected)") : "")

    property bool tabletSettings: layouts.width > units.gu(80)

    Layouts {
        id: layouts
        anchors.fill: parent

        layouts: [
            ConditionalLayout {
                name: "tablet"
                when: tabletSettings

                Item {
                    anchors.fill: parent

                    ItemLayout {
                        id: tabletCategories
                        item: "categories"

                        width: Math.min(parent.width / 3, units.gu(50))

                        anchors {
                            top: parent.top
                            left: parent.left
                            bottom: parent.bottom
                        }

                        Connections {
                            target: categories

                            onSelectedIndexChanged: stack.updateSource()
                        }
                    }

                    Loader {
                        id: stack

                        Component.onCompleted: {
                            updateSource()
                        }

                        function updateSource() {
                            switch (categories.selectedIndex) {
                            case -1:
                            case 0:
                                setSource("HostSettingsContent.qml")
                                break
                            case 1:
                                setSource("UiSettingsContent.qml")
                                break
//                            case 2:
//                                setSource("PlaybackSettingsContent.qml")
//                                break
                            }
                        }

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

        onCurrentLayoutChanged: {
            if (pageStack.currentPage.objectName === settingsPage.objectName) {
                if (categories.selectedIndex >= 0 && currentLayout === "") {
                    switch (categories.selectedIndex) {
                    case 0:
                        pageStack.push(hostSettingsPage)
                        break
                    case 1:
                        pageStack.push(uiSettingsPage)
                        break
//                    case 2:
//                        pageStack.setSource(playbackSettingsPage)
//                        break
                    }
                }
            } else if (tabletSettings) { //Not in foreground and tabletSettings
                while (pageStack.currentPage.objectName !== settingsPage.objectName) {
                    pageStack.pop()
                }
                if (pageStack.currentPage.objectName === hostSettingsPage.objectName) {
                    categories.selectedIndex = 0
                } else if (pageStack.currentPage.objectName === uiSettingsPage.objectName) {
                    categories.selectedIndex = 1
//                } else if (pageStack.currentPage.objectName === playbackSettingsPage.objectName) {
//                    categories.selectedIndex = 2
                }
            }
        }

        Connections {
            target: pageStack

            onCurrentPageChanged: {
                if (!tabletSettings && pageStack.currentPage.objectName === settingsPage.objectName) {
                    categories.selectedIndex = -1
                }
            }
        }

        SettingsCategories {
            id: categories
            Layouts.item: "categories"
            push: layouts.currentLayout === ""
            anchors.fill: parent
        }

    }

}

