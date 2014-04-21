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

Column {
    id: column
    spacing: units.gu(1)

    Component.onCompleted: {
        // TODO: Is this called each time page is made visible??? - Nope, it's called when the component is created
        backend.getPlaybackSettings();
    }

    Label {
        id: crossfadeLabel
        text: i18n.tr("Crossfade between tracks:")
        anchors {
            left: column.left;
            right: column.right;
        }
        fontSize: "medium"
    }

    Label {
        id: todo
        text: "TODO: SpinBox - "+backend.crossfade
        anchors {
            left: column.left;
            right: column.right;
        }
        fontSize: "medium"
    }
/*
    SpinBox {
        id: crossfade
        anchors {
            left: column.left;
            right: column.right;
        }
        fontSize: "medium"
        //KeyNavigation.priority: KeyNavigation.BeforeItem
        //KeyNavigation.tab: replaygain
    }
*/

    Label {
        id: replaygainLabel
        text: i18n.tr("ReplayGain:")
        anchors {
            left: column.left;
            right: column.right;
        }
        fontSize: "medium"
    }

    Label {
        id: todo2
        text: "TODO: ComboBox - "+backend.replayGain
    }
    /*
    ComboBox {
        currentIndex: 2
        model: ListModel {
            id: cbItems
            ListElement { text: i18n.tr("None") }
            ListElement { text: i18n.tr("Track") }
            ListElement { text: i18n.tr("Album") }
            ListElement { text: i18n.tr("Auto") }
        }
    }
    */

    Label {
        id: outputsLabel
        text: i18n.tr("Outputs:")
        anchors {
            left: column.left;
            right: column.right;
        }
        fontSize: "medium"
    }

    Label {
        id: todoV
        text: i18n.tr("TODO: Want checkable list of outputs???:")
    }

    ListView {
        contentHeight: ouptutsColumn.height

        Column {
            id: ouptutsColumn
            width: parent.width

            ListItem.Standard {
                id: o1
                text: "Ouptut 1"
            }

            ListItem.Standard {
                id: o2
                text: "Ouptut 2"
            }
        }
    }
}
