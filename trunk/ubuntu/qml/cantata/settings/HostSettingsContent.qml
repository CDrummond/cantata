/*************************************************************************
** Cantata
**
** Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
** Copyright (c) 2014 Craig Drummond <craig.p.drummond@gmail.com>
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
//import U1db 1.0 as U1db
import Qt.labs.settings 1.0

Flickable {
    clip: true

    contentHeight: connectionDetailsColumn.height

    property color textFieldColor: "#c2c2b8" //#f3f3e7 * 0.8 (#f3f3e7: label color)

    Column {
        id: connectionDetailsColumn
        spacing: units.gu(1)

        width: Math.round(parent.width / 1.3)
        height: parent.parent.height - root.header.height
        y: units.gu(2)
        anchors.horizontalCenter: parent.horizontalCenter

        Component.onCompleted: {
            tryToConnect()
        }

        Settings {
            id: settings
            category: "connection"
            property alias host: hostTextField.text
            property alias port: portTextField.text
            property alias password: passwordTextField.text
            property alias musicfolder: musicfolderTextField.text
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
            color: textFieldColor
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
            color: textFieldColor
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
            color: textFieldColor
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }

            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.tab: musicfolderTextField
            KeyNavigation.backtab: portTextField

            echoMode: TextInput.Password

            onAccepted: { //Invoked when the enter key is pressed
                connectButton.clicked()
            }
        }

        Label {
            id: musicfolderLabel
            text: i18n.tr("Music Folder:")
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }

            fontSize: "medium"
        }

        TextField {
            id: musicfolderTextField
            color: textFieldColor
            anchors {
                left: connectionDetailsColumn.left;
                right: connectionDetailsColumn.right;
            }

            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.backtab: passwordTextField
            placeholderText: "http://HOST/music" // TODO: Update this when hostTextField changes?
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
            color: UbuntuColors.orange

            width: parent.width
            anchors {
                horizontalCenter: parent.horizontalCenter
            }

            onClicked: {
                console.log("Org: " + Qt.application.organization)
                connectionDetailsColumn.tryToConnect()
            }
        }

        function tryToConnect() {
            backend.connectTo(settings.host, (settings.port === "")?6600:settings.port, settings.password, settings.musicfolder)
        }
    }
}
