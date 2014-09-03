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
import Ubuntu.Components.ListItems 1.0 as ListItem
import Ubuntu.Layouts 1.0
import 'qrc:/qml/cantata/'
import 'qrc:/qml/cantata/components'

PageWithBottomEdge {
    id: listViewPage

    property string modelName
    property bool editable: false
    property alias model: listView.model
    property alias emptyViewVisible: emptyLabel.visible
    property alias emptyViewText: emptyLabel.text //TODO: Fix position in tablet view

    function add(index, replace, mainText) {
        backend.add(modelName, index, replace)
        if (replace && isPhone) {
            pageStack.push(Qt.resolvedUrl("CurrentlyPlayingPage.qml"))
        } else if (mainText !== undefined && mainText !== "") {
            notification.show(qsTr(replace ? i18n.tr("Playing \"%1\"") : i18n.tr("Added \"%1\"")).arg(mainText))
        }
    }

    function remove(index) {
        backend.remove(modelName, index)
    }

    function onDelegateClicked(index, text) {
        var component = Qt.createComponent("SubListViewPage.qml")
        if (component.status === Component.Ready) {
            var page = component.createObject(parent, {"model": model, "title": text, "modelName": modelName})
            page.init([index])
            page.editable=editable
            pageStack.push(page)
        }
    }

    head.actions: [
        Action {
            iconName: "settings"
            text: i18n.tr("Settings")
            onTriggered: pageStack.push(settingsPage)
        },

        Action {
            iconName: "info"
            text: i18n.tr("About")
            onTriggered: pageStack.push(aboutPage)
        }
    ]

    bottomEdgePageSource: Qt.resolvedUrl("CurrentlyPlayingPage.qml")
    bottomEdgeTitle: i18n.tr("Currently Playing")
    bottomEdgeEnabled: isPhone

    Connections {
        target: settingsBackend

        onFetchCoversChanged: {
            var saveModel = listView.model
            listView.model = undefined
            listView.model = saveModel
        }
    }

    Layouts { //TODO: Same for SubListViewPage
        id: layouts

        height: parent.height //NOT "anchors.fill: parent" as otherwise the bottom edge gesture will continue behind the header
        width: parent.width

        anchors {
            top: parent.top
            left: parent.left
        }

        layouts: [
            ConditionalLayout {
                id: tabletLayout
                name: "tablet"
                when: !isPhone //TODO: Fix width of isPhone

                Item {
                    anchors.fill: parent

                    property real spacing: units.gu(2) //TODO: Visual divider?

                    ItemLayout {
                        item: "listView"

                        width: (parent.width - spacing) / 2
                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            left: parent.left
                        }
                    }

                    ItemLayout {
                        item: "emptyView"
                        anchors.centerIn: parent
                    }

                    CurrentlyPlayingContent { //TODO: Is it possible to use the same page for all ListViewPages?
                        id: currentlyPlayingPage

                        width: (parent.width - spacing) / 2
                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            right: parent.right
                        }
                    }
                }


            }

        ]

        ListView {
            id: listView
            Layouts.item: "listView"

            height: parent.height
            width: parent.width

            anchors {
                top: parent.top
                left: parent.left
            }

            clip: true

            property bool hasProgression: false

            delegate: ListItemDelegate {
                id: delegate
                text: model.mainText
                subText: model.subText
                iconSource: model.image
                confirmRemoval: true
                removable: listViewPage.editable

                progression: model.hasChildren;
                forceProgressionSpacing: listView.hasProgression

                firstButtonIconName: "media-playback-start"
                secondButtonIconName: "add"

                onFirstImageButtonClicked: listViewPage.add(index, true, model.mainText)
                onSecondImageButtonClicked: listViewPage.add(index, false, model.mainText)

                onClicked: model.hasChildren ? listViewPage.onDelegateClicked(index, model.titleText) : "";
                onItemRemoved: listViewPage.remove(index)

                onProgressionChanged: {
                    if (progression) {
                        listView.hasProgression = true
                    }
                }
            }
        }

        Label {
            id: emptyLabel
            Layouts.item: "emptyView"
            anchors.centerIn: listView
            fontSize: "large"
        }
    }
}
