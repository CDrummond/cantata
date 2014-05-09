import QtQuick 2.0
import Ubuntu.Components 0.1

Rectangle {
    id: notification

    anchors {
        horizontalCenter: parent.horizontalCenter
        bottom: parent.bottom
        margins: (toolbar.opened && !toolbar.locked ? toolbar.height : 0) + units.gu(2) + ((!mainView.anchorToKeyboard && Qt.inputMethod.visible) ? Qt.inputMethod.keyboardRectangle.height : 0)
    }

    height: label.height + units.gu(3)
    width: label.width + units.gu(4.5)
    radius: height/2
    color: Qt.rgba(0,0,0,0.7)

    opacity: showing ? 1 : 0

    Behavior on opacity {
        UbuntuNumberAnimation {}
    }

    property bool showing: false
    property string text
    property MainView mainView
    property var queue: []

    Component.onCompleted: mainView = findMainView() //This cannot be done as a property binding because the method will later return the QQuickRootItem.

    function show(text) {
        queue.push(text)
        if (!showing) {
            update()
        }
    }

    function update() {
        notification.text = queue.pop()
        notification.showing = true
    }

    onShowingChanged: {
        if (showing) {
            timer.restart()
        } else {
            if (queue.length > 0) {
                timer.interval = 800
                timer.restart()
            }
        }
    }

    Label {
        id: label
        anchors.centerIn: parent
        text: notification.text
        fontSize: "medium"
    }

    Timer {
        id: timer
        interval: 2000
        onTriggered: {
            if (interval === 2000) {
                showing = false
            } else {
                interval = 2000
                update()
            }
        }
    }

    function findMainView() {
        var up = parent
        while (up.parent !== null) {
            up = up.parent
        }
        return up
    }
}
