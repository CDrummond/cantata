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
    anchors.fill: parent
    title: i18n.tr("About")
    visible: false

    tools: ToolbarItems {
        pageStack: root.pageStack
        opened: true
        locked: root.width > units.gu(60) && opened //"&& opened": prevents the bar from being hidden and locked at the same time
    }

    Layouts {
        id: aboutTabLayout
        anchors.fill: parent

        layouts: [
//            ConditionalLayout {
//                id: conditionalLayout
//                name: "tablet"
//                when: root.width > units.gu(80)
//                Row {
//                    anchors {
//                        left: parent!=null?parent.left:conditionalLayout.left
//                        leftMargin: root.width*0.1
//                        top: parent!=null?parent.top:conditionalLayout.top
//                        topMargin: root.height*0.2

//                    }
//                    spacing: units.gu(5)
//                    ItemLayout {
//                        item: "icon"
//                        id: iconTabletItem
//                        property real maxWidth: units.gu(80)
//                        width: Math.min(parent.width, maxWidth)/2
//                        height: Math.min(parent.width, maxWidth)/2

//                    }
//                    Column {
//                        spacing: 1
//                        ItemLayout {
//                            item: "info"
//                            width: aboutTabLayout.width*0.25
//                            height: iconTabletItem.height*0.75
//                        }
//                        ItemLayout {
//                            item: "link"
//                            width: aboutTabLayout.width*0.25
//                            height: units.gu(3)
//                        }
//                        ItemLayout {
//                            item: "version"
//                            width: aboutTabLayout.width*0.25
//                            height: units.gu(2)
//                        }
//                        ItemLayout {
//                            item: "year"
//                            width: aboutTabLayout.width*0.25
//                            height: units.gu(2)
//                        }
//                    }
//                }
//            },
            ConditionalLayout {
                id: conditionalLayout
                name: "tablet"
                when: root.width > units.gu(80)
                Row {
                    anchors {
                        left: parent!=null?parent.left:conditionalLayout.left
                        leftMargin: root.width*0.1
                        top: parent!=null?parent.top:conditionalLayout.top
                        topMargin: root.height*0.2

                    }
                    spacing: units.gu(5)

                    UbuntuShape {
                        id: iconTabletItem
                        property real maxWidth: units.gu(80)
                        width: Math.min(parent.width, maxWidth)/2
                        height: Math.min(parent.width, maxWidth)/2
                        image: Image {
                            source: "../../icons/cantata.svg"
                            smooth: true
                            fillMode: Image.PreserveAspectFit

                        }
                    }

                    Column {
                        spacing: 1
                        Grid {
                            anchors.horizontalCenter: parent.horizontalCenter
                            columns: 2
                            rowSpacing: units.gu(2)
                            columnSpacing: root.width/10
                            width: aboutTabLayout.width*0.25
                            height: iconTabletItem.height*0.75
                            Label {
                                text: i18n.tr("Authors: ")

                            }
                            Label {
                                text: "<b>Niklas Wenzel<br/>Craig Drummond</b><br/>Sander Knopper<br/>Roeland Douma<br/>Daniel Selinger<br/>Armin Walland"
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
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: aboutTabLayout.width*0.25
                            height: units.gu(3)
                            Label {
                                font.bold: true
                                text: "<a href=\"https://cantata.googlecode.com\">https://cantata.googlecode.com</a>"
                                onLinkActivated: Qt.openUrlExternally(link)
                            }
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: aboutTabLayout.width*0.25
                            height: units.gu(2)
                            Label {
                                text: i18n.tr("Version: ")
                            }
                            Label {
                                font.bold: true;
                                text: "0.1.8"
                            }
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: aboutTabLayout.width*0.25
                            height: units.gu(2)
                            Label {
                                font.bold: true;
                                text: "2014"
                            }
                        }
                    }
                }
            },
            ConditionalLayout {
                id: quickFixConditionalLayoutPhone
                name: "phone"
                when: root.width <= units.gu(80)

                Flickable {
                    id: flickable2
                    anchors.fill: parent
                    clip: true

                    contentHeight: aboutColumn2.height + 2 * aboutColumn2.marginTop  + root.header.height //doubled marginTop to get the same margin at the bottom

                    Column {
                        id: aboutColumn2
                        spacing: units.gu(3)
                        width: parent.width
                        property real marginTop: units.gu(6)
                        y: marginTop + root.header.height

                        UbuntuShape {
                            property real maxWidth: units.gu(45)
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: Math.min(parent.width, maxWidth)/2
                            height: Math.min(parent.width, maxWidth)/2
                            image: Image {
                                source: "../../icons/cantata.svg"
                                smooth: true
                                fillMode: Image.PreserveAspectFit

                            }
                        }

                        Grid {
                            anchors.horizontalCenter: parent.horizontalCenter
                            columns: 2
                            rowSpacing: units.gu(2)
                            columnSpacing: root.width/10
                            Label {
                                text: i18n.tr("Authors: ")

                            }
                            Label {
                                text: "<b>Niklas Wenzel<br/>Craig Drummond</b><br/>Sander Knopper<br/>Roeland Douma<br/>Daniel Selinger<br/>Armin Walland"
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
                            id: homepage2
                            anchors.horizontalCenter: parent.horizontalCenter
                            Label {
                                font.bold: true
                                text: "<a href=\"https://cantata.googlecode.com\">https://cantata.googlecode.com</a>"
                                onLinkActivated: Qt.openUrlExternally(link)
                            }
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            Label {
                                text: i18n.tr("Version: ")
                            }
                            Label {
                                font.bold: true;
                                text: "0.1.8"
                            }
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            Label {
                                font.bold: true;
                                text: "2014"
                            }
                        }
                    }
                }
            }


        ]

        Flickable {
            id: flickable
            anchors.fill: parent
            clip: true

            contentHeight: aboutColumn.height + 2 * aboutColumn.marginTop  + root.header.height //doubled marginTop to get the same margin at the bottom

            Column {
                id: aboutColumn
                spacing: units.gu(3)
                width: parent.width
                property real marginTop: units.gu(6)
                y: marginTop + root.header.height

                UbuntuShape {
                    Layouts.item: "icon"
                    property real maxWidth: units.gu(45)
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, maxWidth)/2
                    height: Math.min(parent.width, maxWidth)/2
                    image: Image {
                        source: "../../icons/cantata.svg"
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
                        text: i18n.tr("Authors: ")

                    }
                    Label {
                        text: "<b>Niklas Wenzel<br/>Craig Drummond</b><br/>Sander Knopper<br/>Roeland Douma<br/>Daniel Selinger<br/>Armin Walland"
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
                        text: "0.1.8"
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
