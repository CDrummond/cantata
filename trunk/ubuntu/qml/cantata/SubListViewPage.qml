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

import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem
import 'qrc:/qml/cantata/'
import 'qrc:/qml/cantata/components'

Page {
    id: subListViewPage

    anchors.fill: parent
    visible: false

    property bool editable: false
    property variant rows
    property string modelName
    property alias model: visualDataModel.model

    function hierarchy(index) {
        var newRows = []
        for (var i = 0; i < rows.length; i++) {
            newRows[i] = rows[i]
        }
        newRows[rows.length] = index
        return newRows
    }

    function add(index, replace, mainText) {
        backend.add(modelName, index, replace)
        if (replace) {
            pageStack.push(currentlyPlayingPage)
        } else if (mainText !== undefined && mainText !== "") {
            notification.show(qsTr(i18n.tr("Added \"%1\"")).arg(mainText))
        }
    }

    function remove(index) {
        backend.remove(modelName, hierarchy(index))
    }
    
    function onDelegateClicked(index, text) {
        var component = Qt.createComponent("SubListViewPage.qml")
        if (component.status == Component.Ready) {
            var page = component.createObject(parent, {"model": model, "title": text, "modelName": modelName})
            page.init(hierarchy(index))
            pageStack.push(page)
        }
    }

    function init(rows) {
        this.rows = rows
        subListView.model.rootIndex = -1
        for (var i = 0; i < this.rows.length; i++) {
            subListView.model.rootIndex = subListView.model.modelIndex(this.rows[i])
        }
    }

    actions: [
        Action {
            id: currentlyPlayingAction
            text: i18n.tr("Currently playing")
            onTriggered: pageStack.push(currentlyPlayingPage)
        },
        Action {
            id: settingsAction
            text: i18n.tr("Settings")
            onTriggered: pageStack.push(settingsPage)
        },
        Action {
            id: aboutAction
            text: i18n.tr("About")
            onTriggered: pageStack.push(aboutPage)
        }
    ]

    tools: ToolbarItems {
        ToolbarButton {
            action: Action {
                iconName: "media-playback-start"
                text: i18n.tr("Playing")
                onTriggered: pageStack.push(currentlyPlayingPage)
            }
        }

        ToolbarButton {
            action: Action {
                iconName: "settings"
                text: i18n.tr("Settings")
                onTriggered: pageStack.push(settingsPage)
            }
        }

        ToolbarButton {
            action: Action {
                iconName: "help"
                text: i18n.tr("About")
                onTriggered: pageStack.push(aboutPage)
            }
        }
    }

    ListView {
        id: subListView
        anchors {
            fill: parent
            bottomMargin: isPhone?0:(-units.gu(2))
        }
        clip: true

        property bool hasProgression: false

        model: VisualDataModel {
            id: visualDataModel

            onRootIndexChanged: subListView.hasProgression = false

            delegate: ListItemDelegate {
                id: delegate
                text: model.mainText
                subText: model.subText
                iconSource: model.image
                confirmRemoval: true
                removable: subListViewPage.editable
            
                firstButtonImageSource: "../../icons/toolbar/media-playback-start-light.svg"
                secondButtonImageSource: "../../icons/toolbar/add.svg"
                progression: model.hasChildren
                forceProgressionSpacing: subListView.hasProgression

                onFirstImageButtonClicked: subListViewPage.add(index, true, model.mainText)
                onSecondImageButtonClicked: subListViewPage.add(index, false, model.mainText)

                onClicked: model.hasChildren ? subListViewPage.onDelegateClicked(index, model.titleText) : "";
                onItemRemoved: subListViewPage.remove(index)

                onProgressionChanged: {
                    if (progression) {
                        subListView.hasProgression = true
                    }
                }
            }
        }
    }
}
