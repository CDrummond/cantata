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
import Ubuntu.Layouts 0.1

Page {
    id: aboutPage

    width: parent.width
    title: i18n.tr("About")
    visible: false

    tools: ToolbarItems {
        pageStack: root.pageStack
        opened: true
        locked: root.width > units.gu(60) && opened //"&& opened": prevents the bar from being hidden and locked at the same time
    }

    Layouts {
        id: aboutTabLayout
        width: parent.width
        height: parent.height
        layouts: [
            ConditionalLayout {
                name: "tablet"
                when: mainView.width > units.gu(80)
                Row {
                    anchors {
                        left: parent.left
                        leftMargin: mainView.width*0.1
                        top: parent.top
                        topMargin: mainView.height*0.2

                    }
                    spacing: units.gu(5)
                    ItemLayout {
                        item: "icon"
                        id: iconTabletItem
                        property real maxWidth: units.gu(80)
                        width: Math.min(parent.width, maxWidth)/2
                        height: Math.min(parent.width, maxWidth)/2

                    }
                    Column {
                        spacing: 1
                        ItemLayout {
                            item: "info"
                            width: aboutTabLayout.width*0.25
                            height: iconTabletItem.height*0.75
                        }
                        ItemLayout {
                            item: "link"
                            width: aboutTabLayout.width*0.25
                            height: units.gu(3)
                        }
                        ItemLayout {
                            item: "version"
                            width: aboutTabLayout.width*0.25
                            height: units.gu(2)
                        }
                        ItemLayout {
                            item: "year"
                            width: aboutTabLayout.width*0.25
                            height: units.gu(2)
                        }
                    }
                }
            }


        ]

        Flickable {
            id: flickable
            width: parent.width
            height: parent.height
            clip: true

            contentHeight: aboutColumn.height + 2 * aboutColumn.y //doubled y to get the same margin at the bottom

            Column {
                id: aboutColumn
                spacing: units.gu(3)
                width: parent.width
                y: units.gu(6);

                UbuntuShape {
                    Layouts.item: "icon"
                    property real maxWidth: units.gu(45)
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, maxWidth)/2
                    height: Math.min(parent.width, maxWidth)/2
                    image: Image {
                        source: "../../icons/desktop/cantata.png"
                        smooth: true
                        fillMode: Image.PreserveAspectFit

                    }
                }

                Grid {
                    anchors.horizontalCenter: parent.horizontalCenter
                    columns: 2
                    rowSpacing: units.gu(2)
                    columnSpacing: root.width/10
                    Layouts.item: "info"
                    Label {
                        text: i18n.tr("Author(s): ")

                    }
                    Label {
                        font.bold: true;
                        text: "Niklas Wenzel \nCraig Drummond \nSander Knopper \nRoeland Douma \nDaniel Selinger \nArmin Walland"
                    }
                    Label {
                        text: i18n.tr("Contact: ")
                    }
                    Label {
                        font.bold: true;
                        text: "nikwen.developer@gmail.com"
                    }

                }

                Row {
                    id: homepage
                    anchors.horizontalCenter: parent.horizontalCenter
                    Layouts.item: "link"
                    Label {
                        font.bold: true
                        text: "<a href=\"https://cantata.googlecode.com\">https://cantata.googlecode.com</a>"
                        onLinkActivated: Qt.openUrlExternally(link)
                    }
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    Layouts.item: "version"
                    Label {
                        text: i18n.tr("Version: ")
                    }
                    Label {
                        font.bold: true;
                        text: "0.1.6"
                    }
                }
                Row {
                    Layouts.item: "year"
                    anchors.horizontalCenter: parent.horizontalCenter
                    Label {
                        font.bold: true;
                        text: "2014"
                    }
                }
            }
        }



    }
}
