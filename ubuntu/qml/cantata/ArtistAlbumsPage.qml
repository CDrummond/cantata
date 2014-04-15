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
import 'qrc:/qml/cantata/'
import 'components'

Page {
    id: artistAlbumsPage

    anchors.fill: parent
    visible: false

    property int artistRow: -1

    function init(index) {
        artistRow=index
        artistAlbumsListView.model.rootIndex = -1
        artistAlbumsListView.model.rootIndex = artistAlbumsListView.model.modelIndex(index)
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
        id: artistAlbumsListView
        anchors {
            fill: parent
            bottomMargin: isPhone?0:(-units.gu(2))
        }
        clip: true

        model: VisualDataModel {
            model: artistsProxyModel
            delegate: ListItemDelegate {
                id: delegate
                text: model.mainText
                subText: model.subText
                iconSource: !(model.image.indexOf("qrc:") === 0)?"file:" + model.image:model.image

                firstButtonImageSource: "../../icons/toolbar/media-playback-start-light.svg"
                secondButtonImageSource: "../../icons/toolbar/add.svg"

                onFirstImageButtonClicked: add(true)
                onSecondImageButtonClicked: add(false)

                function add(replace) {
                    backend.addArtistAlbum(artistRow, index, replace)
                    pageStack.push(currentlyPlayingPage)
                }

                onClicked: {
                    artistSongsPage.title = titleText
                    artistSongsPage.init(artistRow, index)
                    pageStack.push(artistSongsPage)
                }
            }
        }
    }
}
