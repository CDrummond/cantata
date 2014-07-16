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

Flickable {
    contentHeight: column.height
    clip: true

    property bool push: true
    property int selectedIndex: 0

    Column {
        id: column
        width: parent.width
        height: childrenRect.height

        ListItem.Standard {
            id: connectionSettingsLabel
            text: i18n.tr("Connection")
            progression: true
            selected: !root.isPhone && selectedIndex === 0
            onClicked: {
                selectedIndex = 0
                if (push) pageStack.push(hostSettingsPage)
            }
        }

        ListItem.Standard {
            id: uiSettingsLabel
            text: i18n.tr("UI")
            progression: true
            selected: !root.isPhone && selectedIndex === 1
            onClicked:  {
                selectedIndex = 1
                if (push) pageStack.push(uiSettingsPage)
            }
        }

        ListItem.Standard {
            id: playbackSettingsLabel
            enabled: false
            text: i18n.tr("Playback")
            progression: true
            selected: !root.isPhone && selectedIndex === 2
            onClicked:  {
                selectedIndex = 2
                if (push) pageStack.push(playbackSettingsPage)
            }
        }
    }
}
