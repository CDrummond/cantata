/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
    Example:

    MainView {
        objectName: "mainView"

        applicationName: "com.ubuntu.developer.boiko.bottomedge"

        width: units.gu(100)
        height: units.gu(75)

        Component {
            id: pageComponent

            PageWithBottomEdge {
                id: mainPage
                title: i18n.tr("Main Page")

                Rectangle {
                    anchors.fill: parent
                    color: "white"
                }

                bottomEdgePageComponent: Page {
                    title: "Contents"
                    anchors.fill: parent
                    //anchors.topMargin: contentsPage.flickable.contentY

                    ListView {
                        anchors.fill: parent
                        model: 50
                        delegate: ListItems.Standard {
                            text: "One Content Item: " + index
                        }
                    }
                }
                bottomEdgeTitle: i18n.tr("Bottom edge action")
            }
        }

        PageStack {
            id: stack
            Component.onCompleted: stack.push(pageComponent)
        }
    }

*/

import QtQuick 2.2
import Ubuntu.Components 1.1

Page {
    id: page

    property alias bottomEdgePageComponent: edgeLoader.sourceComponent
    property alias bottomEdgePageSource: edgeLoader.source
    property alias bottomEdgeTitle: tipLabel.text
    property alias bottomEdgeEnabled: bottomEdge.visible
    property int bottomEdgeExpandThreshold: page.height * 0.2
    property int bottomEdgeExposedArea: bottomEdge.state !== "expanded" ? (page.height - bottomEdge.y - bottomEdge.tipHeight) : _areaWhenExpanded
    property bool reloadBottomEdgePage: true

    readonly property alias bottomEdgePage: edgeLoader.item
    readonly property bool isReady: ((bottomEdge.y === 0) && bottomEdgePageLoaded && edgeLoader.item.active)
    readonly property bool isCollapsed: (bottomEdge.y === page.height)
    readonly property bool bottomEdgePageLoaded: (edgeLoader.status == Loader.Ready)

    property bool _showEdgePageWhenReady: false
    property int _areaWhenExpanded: 0

    signal bottomEdgeReleased()
    signal bottomEdgeDismissed()


    function showBottomEdgePage(source, properties)
    {
        edgeLoader.setSource(source, properties)
        _showEdgePageWhenReady = true
    }

    function setBottomEdgePage(source, properties)
    {
        edgeLoader.setSource(source, properties)
    }

    function _pushPage()
    {
        if (edgeLoader.status === Loader.Ready) {
            edgeLoader.item.active = true
            page.pageStack.push(edgeLoader.item)
            if (edgeLoader.item.flickable) {
                edgeLoader.item.flickable.contentY = -page.header.height
                edgeLoader.item.flickable.returnToBounds()
            }
            if (edgeLoader.item.ready)
                edgeLoader.item.ready()
        }
    }


    Component.onCompleted: {
        // avoid a binding on the expanded height value
        var expandedHeight = height;
        _areaWhenExpanded = expandedHeight;
    }

    onActiveChanged: {
        if (active) {
            bottomEdge.state = "collapsed"
        }
    }

    onBottomEdgePageLoadedChanged: {
        if (_showEdgePageWhenReady && bottomEdgePageLoaded) {
            bottomEdge.state = "expanded"
            _showEdgePageWhenReady = false
        }
    }

    Rectangle {
        id: bgVisual

        color: "black"
        anchors.fill: page
        opacity: 0.7 * ((page.height - bottomEdge.y) / page.height)
        z: 1
    }

    Timer {
        id: hideIndicator

        interval: 3000
        running: true
        repeat: false
        onTriggered: tip.hiden = true
    }

    Rectangle {
        id: bottomEdge
        objectName: "bottomEdge"

        readonly property int tipHeight: units.gu(3)
        readonly property int pageStartY: 0

        z: 1
        color: Theme.palette.normal.background
        parent: page
        anchors {
            left: parent.left
            right: parent.right
        }
        height: page.height
        y: height

        UbuntuShape {
            id: tip
            objectName: "bottomEdgeTip"

            property bool hiden: false

            readonly property double visiblePosition: (page.height - bottomEdge.y) < units.gu(1) ? -bottomEdge.tipHeight + (page.height - bottomEdge.y) : 0
            readonly property double invisiblePosition: (page.height - bottomEdge.y) < units.gu(1) ? -units.gu(1) : 0

            z: -1
            anchors.horizontalCenter: parent.horizontalCenter
            y: hiden ? invisiblePosition : visiblePosition

            width: tipLabel.paintedWidth + units.gu(6)
            height: bottomEdge.tipHeight + units.gu(1)
            color: Theme.palette.normal.overlay
            Label {
                id: tipLabel

                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                }
                height: bottomEdge.tipHeight
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                opacity: tip.hiden ? 0.0 : 1.0
                Behavior on opacity {
                    UbuntuNumberAnimation {
                        duration: UbuntuAnimation.SnapDuration
                    }
                }
            }
            Behavior on y {
                UbuntuNumberAnimation {
                    duration: UbuntuAnimation.SnapDuration
                }
            }
        }

        Rectangle {
            id: shadow

            anchors {
                left: parent.left
                right: parent.right
            }
            height: units.gu(1)
            y: -height
            z: -2
            opacity: 0.0
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.2) }
            }
        }

        MouseArea {
            id: mouseArea

            preventStealing: true
            drag {
                axis: Drag.YAxis
                target: bottomEdge
                minimumY: bottomEdge.pageStartY
                maximumY: page.height
                threshold: 100
            }

            anchors {
                left: parent.left
                right: parent.right
            }
            height: bottomEdge.tipHeight
            y: -height

            onReleased: {
                page.bottomEdgeReleased()
                if (bottomEdge.y < (page.height - bottomEdgeExpandThreshold - bottomEdge.tipHeight)) {
                    bottomEdge.state = "expanded"
                } else {
                    bottomEdge.state = "collapsed"
                    bottomEdge.y = bottomEdge.height
                }
            }

            onClicked: {
                tip.hiden = false
                hideIndicator.restart()
            }
        }

        state: "collapsed"
        states: [
            State {
                name: "collapsed"
                PropertyChanges {
                    target: bottomEdge
                    y: bottomEdge.height
                }
                PropertyChanges {
                    target: tip
                    opacity: 1.0
                }
                PropertyChanges {
                    target: hideIndicator
                    running: true
                }
            },
            State {
                name: "expanded"
                PropertyChanges {
                    target: bottomEdge
                    y: bottomEdge.pageStartY
                }
                PropertyChanges {
                    target: hideIndicator
                    running: false
                }
            },
            State {
                name: "floating"
                when: mouseArea.drag.active
                PropertyChanges {
                    target: shadow
                    opacity: 1.0
                }
                PropertyChanges {
                    target: hideIndicator
                    running: false
                }
                PropertyChanges {
                    target: tip
                    hiden: false
                }
            }
        ]

        transitions: [
            Transition {
                to: "expanded"
                SequentialAnimation {
                    UbuntuNumberAnimation {
                        target: bottomEdge
                        property: "y"
                        duration: UbuntuAnimation.SlowDuration
                    }
                    ScriptAction {
                        script: page._pushPage()
                    }
                }
            },
            Transition {
                from: "expanded"
                to: "collapsed"
                SequentialAnimation {
                    ScriptAction {
                        script: {
                            Qt.inputMethod.hide()
                            edgeLoader.item.parent = edgeLoader
                            edgeLoader.item.anchors.fill = edgeLoader
                            edgeLoader.item.active = false
                        }
                    }
                    UbuntuNumberAnimation {
                        target: bottomEdge
                        property: "y"
                        duration: UbuntuAnimation.SlowDuration
                    }
                    ScriptAction {
                        script: {
                            // destroy current bottom page
                            if (page.reloadBottomEdgePage) {
                                edgeLoader.active = false
                            }

                            // notify
                            page.bottomEdgeDismissed()

                            edgeLoader.active = true
                            tip.hiden = false
                            hideIndicator.restart()
                        }
                    }
                }
            },
            Transition {
                from: "floating"
                to: "collapsed"
                UbuntuNumberAnimation {
                    target: bottomEdge
                    property: "opacity"
                }
            }
        ]

        Item {
            anchors.fill: parent
            clip: true

            Loader {
                id: edgeLoader

                z: 1
                active: true
                asynchronous: true
                anchors.fill: parent

                //WORKAROUND: The SDK move the page contents down to allocate space for the header we need to avoid that during the page dragging
                Binding {
                    target: edgeLoader.status === Loader.Ready ? edgeLoader : null
                    property: "anchors.topMargin"
                    value:  edgeLoader.item && edgeLoader.item.flickable ? edgeLoader.item.flickable.contentY : 0
                    when: !page.isReady
                }

                onLoaded: {
                    if (page.isReady && edgeLoader.item.active !== true) {
                        page._pushPage()
                    }
                }
            }
        }
    }
}
