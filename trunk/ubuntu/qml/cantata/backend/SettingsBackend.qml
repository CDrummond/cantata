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
import Ubuntu.Components 1.1
import U1db 1.0 as U1db
import 'qrc:/qml/cantata/'

Item {

    U1db.Database {
        id: db
        path: appDir + "/u1db"
    }

    U1db.Document {
        id: uiDocument
        database: db
        docId: 'ui'
        create: true
        defaults: {
            "playQueueScroll": true,
            "coverFetch": true,
            "artistYear": true,
            "hiddenViews": ["folders"],
            "albumSort": "album-artist"
        }

        onContentsChanged: {
            if (contents != undefined) {
                console.log("Update cover fetch")
                backend.setCoverFetch(contents["coverFetch"]) //TODO: Untick fetch checkbox => Delete saved images => Reopen app => Tick fetch checkbox => First few album covers are not loaded
                backend.resetAllModels()
            }
        }
    }

    U1db.Document {
        id: connectionDocument
        database: db
        docId: 'connections'
        create: true
        defaults: {
            "host": "",
            "port": "",
            "password": "",
            "musicfolder": ""
        }
    }

    function getUiContents() {
        return uiDocument.contents
    }

    function setUiContents(contents) {
        uiDocument.contents = contents
    }

    function getConnectionContents() {
        return connectionDocument.contents
    }

    function setConnectionContents(contents) {
        connectionDocument.contents = contents
    }

}
