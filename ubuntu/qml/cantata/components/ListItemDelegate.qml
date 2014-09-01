/*
 * Copyright 2012 Canonical Ltd.
 * Copyright 2014 Niklas Wenzel <nikwen.developer@gmail.com>
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

import QtQuick 2.2
import Ubuntu.Components 1.1
import Ubuntu.Components.ListItems 1.0

Empty {
    id: listItemDelegate
    __height: Math.max(middleVisuals.height, units.gu(6))

    property alias text: label.text

    property alias subText: subLabel.text

    property alias iconSource: iconImage.source

    property bool progression: false
    property bool forceProgressionSpacing: false

    property alias firstButtonIconName: firstIcon.name
    property alias secondButtonIconName: secondIcon.name
    property alias firstButtonIconSource: firstIcon.source
    property alias secondButtonIconSource: secondIcon.source

    signal firstImageButtonClicked()
    signal secondImageButtonClicked()

    property bool firstButtonShown: firstIcon.source !== "" || firstIcon.name !== ""
    property bool secondButtonShown: secondIcon.source !== "" || secondIcon.name !== ""
    property bool iconShown: iconImage.source !== ""

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
            anchors.fill: parent
        }
    }

    Item  {
        id: middleVisuals
        anchors {
            left: iconShown?iconShape.right:parent.left
            right: secondButtonShown?secondIcon.left:(firstButtonShown?firstIcon.left:((listItemDelegate.progression || listItemDelegate.forceProgressionSpacing)?progressionImage.right:parent.right))
            leftMargin: units.gu(iconShown?1:2)
            rightMargin: units.gu(firstButtonShown?1:2)
            verticalCenter: parent.verticalCenter
        }
        height: childrenRect.height + label.anchors.topMargin + subLabel.anchors.bottomMargin

        LabelVisual {
            id: label
            selected: listItemDelegate.selected
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
        }
        LabelVisual {
            id: subLabel
            selected: listItemDelegate.selected
            secondary: true
            anchors {
                left: parent.left
                right: parent.right
                top: label.bottom
            }
            fontSize: "small"
        }
    }

    Icon { //TODO: Revert changes regarding icons!
        id: secondIcon
        width: units.gu(3)
        height: units.gu(3)
        visible: secondButtonShown

        anchors {
            right: firstIcon.left
            rightMargin: units.gu(1)
            verticalCenter: parent.verticalCenter
        }

        MouseArea {
            id: secondTarget
            onClicked: secondImageButtonClicked()
            anchors.fill: parent
            preventStealing: true
        }
    }

    Icon {
        id: firstIcon
        width: units.gu(3)
        height: units.gu(3)
        visible: firstButtonShown

        anchors {
            right: (listItemDelegate.progression || listItemDelegate.forceProgressionSpacing)?progressionImage.left:parent.right
            rightMargin: (listItemDelegate.progression || listItemDelegate.forceProgressionSpacing)?units.gu(1.5):units.gu(2)
            verticalCenter: parent.verticalCenter
        }

        MouseArea {
            id: firstTarget
            onClicked: firstImageButtonClicked()

            anchors.fill: parent
            preventStealing: true
        }
    }

    Image {
        id: progressionImage
        anchors {
            verticalCenter: parent.verticalCenter
            right: parent.right
            rightMargin: units.gu(2)
        }

        opacity: enabled ? 1.0 : 0.5
        source: "../../../icons/toolbar/chevron.png"

        visible: listItemDelegate.progression
    }
}
