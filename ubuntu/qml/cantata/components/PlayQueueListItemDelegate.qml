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

Empty {
    id: playQueueListItemDelegate
    __height: Math.max(middleVisuals.height, units.gu(6))

    property alias text: label.text

    property alias subText: subLabel.text
    property alias timeText: timeLabel.text
    property alias iconSource: iconImage.source

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
