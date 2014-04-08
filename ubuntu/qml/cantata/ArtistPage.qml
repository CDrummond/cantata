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
    id: artistPage

    width: parent.width
    title: i18n.tr("Artists")

    tools: ToolbarItems {
        ToolbarButton {
            action: Action {
                text: i18n.tr("Currently playing")
                onTriggered: pageStack.push(currentlyPlayingPage)
            }
        }

        ToolbarButton {
            action: Action {
                text: "Settings"
                onTriggered: pageStack.push(hostSettingsPage)
            }
        }
    }

    ListModel {
        id: artistModel

        ListElement {
            name: "Guns 'n' Roses"
        }
        ListElement {
            name: "AC/DC"
        }
        ListElement {
            name: "Iron Maiden"
        }
    }

    ListView {
        id: artistListView
        anchors { left: parent.left; right: parent.right }
        height: parent.height
        model: artistModel
        clip: true

        delegate: ListItem.Standard {
            text: model.name
            progression: true
        }
    }
}
