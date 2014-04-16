/*
 * Copyright 2012 Canonical Ltd.
 * Copyright (c) 2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1

/*!
    \qmltype MultiValue
    \inqmlmodule Ubuntu.Components.ListItems 0.1
    \ingroup ubuntu-listitems
    \brief List item displaying a second string under the main label.
    \b{This component is under heavy development.}

    Examples:
    \qml
        import Ubuntu.Components.ListItems 0.1 as ListItem
        Column {
            ListItem.Subtitled {
                text: "Idle"
                subText: "Secondary label"
            }
            ListItem.Subtitled {
                text: "Disabled"
                enabled: false
                subText: "Secondary label"
            }
            ListItem.Subtitled {
                text: "Selected"
                selected: true
                subText: "Secondary label"
            }
            ListItem.Subtitled {
                text: "Progression"
                subText: "Secondary label"
                progression: true
            }
            ListItem.Subtitled {
                text: "Icon"
                subText: "Secondary label"
                icon: Qt.resolvedUrl("icon.png")
            }
            ListItem.Subtitled {
                text: "Multiple lines"
                subText: "This is a single-line subText."
            }
            ListItem.Subtitled {
                text: "Multiple lines"
                subText: "It also works well with icons and progression."
                icon: Qt.resolvedUrl("icon.png")
                progression: true
            }
        }
    \endqml
*/
Empty {
    id: playQueueListItemDelegate
    __height: Math.max(middleVisuals.height, units.gu(6))

    /*!
      \preliminary
      The text that is shown in the list item as a label.
      \qmlproperty string text
     */
    property alias text: label.text

    /*!
      \preliminary
      The list of strings that will be shown under the label text
      \qmlproperty string subText
     */
    property alias subText: subLabel.text
    property alias timeText: timeLabel.text
    property alias iconSource: iconImage.source

    signal buttonClicked()

    property bool currentTrack: false
    property bool iconShown: iconImage.source != ""

    UbuntuShape {
        id: iconShape
        width: units.gu(5.5)
        height: units.gu(5.5)
        anchors {
            left: parent.left
            leftMargin: units.gu(2)
            verticalCenter: parent.verticalCenter
        }
        visible: iconShown

        image: Image {
            id: iconImage
            smooth: true
            opacity: 0.9
            visible: iconShown
        }
    }

    Image {
        id: image
        width: units.gu(3)
        height: units.gu(3)
        smooth: true
        opacity: 0.9
        visible: currentTrack
        source: "../../../icons/toolbar/media-playback-start-light.svg"

        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
        }

        MouseArea {
            id: firstTarget
            onClicked: buttonClicked()
            anchors.fill: parent
            preventStealing: true
        }
    }
   
    Item  {
        id: middleVisuals
        anchors {
            left: iconShown?iconShape.right:parent.left
            right: parent.right
            leftMargin: units.gu(iconShown?1:2)
            rightMargin: units.gu(2)
            verticalCenter: parent.verticalCenter
        }
        height: childrenRect.height + label.anchors.topMargin + subLabel.anchors.bottomMargin

        LabelVisual {
            id: label
            selected: playQueueListItemDelegate.selected
            font.bold: currentTrack
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
        }
        LabelVisual {
            id: subLabel
            selected: playQueueListItemDelegate.selected
            secondary: true
            anchors {
                left: parent.left
                right: timeLabel.left
                top: label.bottom
            }
        }
        
        LabelVisual {
            id: timeLabel
            selected: playQueueListItemDelegate.selected
            secondary: true
            anchors {
                top: subLabel.top
                right: parent.right
            }
        }
    }
}
