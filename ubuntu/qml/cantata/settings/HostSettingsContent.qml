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

import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem
import U1db 1.0 as U1db

Column {
    id: connectionDetailsColumn
    spacing: units.gu(1)

    property color textFieldColor: "#c2c2b8" //#f3f3e7 * 0.8 (#f3f3e7: label color)

    U1db.Database {
        id: connectionDatabase
        path: appDir + "/u1db"
    }
    U1db.Document {
        id: connectionDocument
        database: connectionDatabase
        docId: 'connections'
        create: true
        defaults: {
            "host": "",
            "port": "",
            "password": "",
            "musicfolder": ""
        }
    }

    Component.onCompleted: {
        fillWithU1dbData()
        tryToConnect()
    }

    function fillWithU1dbData() {
        var contents = connectionDocument.contents
        if (typeof contents != "undefined") {
            hostTextField.text = contents["host"]
            portTextField.text = contents["port"]
            passwordTextField.text = contents["password"]
            musicfolderTextField.text = contents["musicfolder"]
        }
    }

    function saveDataToU1db() {
        var contents = {};
        contents["host"] = hostTextField.text
        contents["port"] = portTextField.text
        contents["password"] = passwordTextField.text
        contents["musicfolder"] = musicfolderTextField.text
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

        width: parent.width
        anchors {
            horizontalCenter: parent.horizontalCenter
        }

        onClicked: {
            tryToConnect()
            connectionDetailsColumn.saveDataToU1db()
        }
    }

    function tryToConnect() {
        backend.connectTo(hostTextField.text, (portTextField.text === "")?6600:portTextField.text, passwordTextField.text, musicfolderTextField.text)
    }
}
