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
import Ubuntu.Components.ListItems 0.1 as ListItem
import U1db 1.0 as U1db

Page {
    id: hostSettingsPage

    visible: false
    width: parent.width
    title: qsTr(i18n.tr("Settings %1")).arg(backend.isConnected?i18n.tr("(Connected)"):i18n.tr("(Not Connected)"))

    actions: [
        Action {
            id: backAction
            text: i18n.tr("Back")
            onTriggered: pageStack.pop()
        }
    ]

    tools: ToolbarItems {
        opened: true
        locked: root.width > units.gu(60) && opened //"&& opened": prevents the bar from being hidden and locked at the same time
        pageStack: pageStack
    }

    U1db.Database {
        id: connectionDatabase
        path: appDir + "/connectionU1DbDatabase"
    }
    U1db.Document {
        id: connectionDocument
        database: connectionDatabase
        docId: 'connections'
        create: true
        defaults: {
            "host": "",
            "port": "",
            "password": ""
        }
    }

    Column {
        id: connectionDetailsColumn
        spacing: units.gu(1)
        width: Math.round(parent.width / 1.3)
        height: parent.height - parent.header.height
        y: parent.header.height + units.gu(2)
        anchors.horizontalCenter: parent.horizontalCenter

        Component.onCompleted: {
            fillWithU1dbData()
            tryToConnect()
        }

        function fillWithU1dbData() {
            var contents = connectionDocument.contents
            hostTextField.text = contents["host"]
            portTextField.text = contents["port"]
            passwordTextField.text = contents["password"]
        }

        function saveDataToU1db() {
            var contents = {};
            contents["host"] = hostTextField.text
            contents["port"] = portTextField.text
            contents["password"] = passwordTextField.text
            connectionDocument.contents = contents
        }

        Label {
            id: hostLabel
            text: i18n.tr("Host:")
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }

            fontSize: "medium"
        }

        TextField {
            id: hostTextField
            color: "#c2c2b8" //#f3f3e7 * 0.8 (#f3f3e7: label color)
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }

            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.tab: portTextField
        }

        Label {
            id: portLabel
            text: i18n.tr("Port:")
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }

            fontSize: "medium"
        }

        TextField {
            id: portTextField
            color: "#c2c2b8"
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }
            validator: IntValidator { bottom: 1; top: 65535 }

            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.tab: passwordTextField
            KeyNavigation.backtab: hostTextField

            placeholderText: "6600"
        }

        Label {
            id: passwordLabel
            text: i18n.tr("Password:")

            fontSize: "medium"
        }

        TextField {
            id: passwordTextField
            color: "#c2c2b8"
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }

            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.backtab: portTextField

            echoMode: TextInput.Password

            onAccepted: { //Invoked when the enter key is pressed
                connectButton.clicked()
            }
        }

        Item {
            height: units.gu(0.5)
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }
        }

        Button {
            id: connectButton
            text: i18n.tr("Connect")

            width: parent.width
            anchors {
                horizontalCenter: parent.horizontalCenter
            }

            onClicked: {
                tryToConnect()
                connectionDetailsColumn.saveDataToU1db()
            }
        }
    }

    function tryToConnect() {
        backend.connectTo(hostTextField.text, (portTextField.text === "")?6600:portTextField.text, passwordTextField.text)
    }
}
