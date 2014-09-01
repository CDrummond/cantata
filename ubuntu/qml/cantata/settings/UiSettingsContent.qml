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
        height: childrenRect.height
        width: parent.width

        ListItem.Standard {
            text: i18n.tr("Scroll play queue to active track")
            width: parent.width

            control: CheckBox {
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
        }

        ListItem.Standard {
            text: i18n.tr("Fetch missing covers from last.fm")
            width: parent.width

            control: CheckBox {
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
    }
}
