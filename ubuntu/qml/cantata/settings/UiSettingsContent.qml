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
import Qt.labs.settings 1.0

Flickable {
    clip: true

    contentHeight: column.height

    Column {
        id: column
        spacing: units.gu(1)
        height: childrenRect.height
        anchors.horizontalCenter: parent.horizontalCenter

        Item {
            id: topSpacer
            height: units.gu(2)
            width: parent.width
        }

        Grid {
            anchors.horizontalCenter: parent.horizontalCenter
            columns: 2
            rowSpacing: units.gu(2)
            columnSpacing: parent.width/10
            height: childrenRect.height

            Label {
                id: scrollPlayQueueLabel
                text: i18n.tr("Scroll play queue to active track:")
                fontSize: "medium"
                verticalAlignment: Text.AlignVCenter
                height: scrollPlayQueueCheckBox.height
            }

            CheckBox {
                id: scrollPlayQueueCheckBox
                checked: true
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: fetchCoversCheckBox

                Component.onCompleted: {
                    scrollPlayQueueCheckBox.checked = settingsBackend.scrollPlayQueue
                }

                onCheckedChanged: {
                    settingsBackend.uiSettings.scrollPlayQueue = scrollPlayQueueCheckBox.checked
                }
            }

            Label {
                id: fetchCoversLabel
                text: i18n.tr("Fetch missing covers from last.fm:")
                fontSize: "medium"
                verticalAlignment: Text.AlignVCenter
                height: fetchCoversCheckBox.height
            }

            CheckBox {
                id: fetchCoversCheckBox
                checked: true
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.backtab: scrollPlayQueueCheckBox

                Component.onCompleted: {
                    fetchCoversCheckBox.checked = settingsBackend.fetchCovers
                }

                onCheckedChanged: {
                    settingsBackend.uiSettings.fetchCovers = fetchCoversCheckBox.checked
                }
            }
        }

        Item {
            id: bottomSpacer
            height: units.gu(2)
            width: parent.width
        }
    }
}
