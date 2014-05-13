/*************************************************************************
** Cantata
**
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

// TODO: Save settings??? How/when??
Flickable {
    clip: true

    contentHeight: column.height

    U1db.Database {
        id: db
        path: appDir + "/u1db"
    }
    U1db.Document {
        id: dbDocument
        database: db
        docId: 'ui'
        create: true
        defaults: {
            "artistYear": true,
            "albumSort": "album-artist",
            "coverFetch": true,
            "playQueueScroll": true,
            "hiddenViews": ["folders"]
        }
    }

    Column { //TODO: Find better solution for spacing
        id: column
        spacing: units.gu(1)
        height: childrenRect.height
        anchors.horizontalCenter: parent.horizontalCenter

        Component.onCompleted: {
            fillWithU1dbData()
        }

        function arrayContains(container, element) {
            for (var i = 0; i < container.length; i++) {
                if (container[i] === element) {
                    return true
                }
            }
            return false
        }

        function fillWithU1dbData() {
            // TODO: albumSort
            var contents = dbDocument.contents
            if (contents !== undefined) {
                artistYear.checked = contents["artistYear"]
                coverFetch.checked = contents["coverFetch"]
                playQueueScroll.checked = contents["playQueueScroll"]
                albumsView.checked = !arrayContains(contents["hiddenViews"], "albums")
                foldersView.checked = !arrayContains(contents["hiddenViews"], "folders")
                playlistsView.checked = !arrayContains(contents["hiddenViews"], "playlists")
            } else {
                artistYear.checked = true
                coverFetch.checked = true
                playQueueScroll.checked = true
                albumsView.checked = true
                foldersView.checked = true
                playlistsView.checked = true
            }
        }

        function saveDataToU1db() {
            var contents = {};
            contents["artistYear"] = artistYear.checked
            contents["coverFetch"] = coverFetch.checked
            contents["playQueueScroll"] = playQueueScroll.checked
            contents["hiddenViews"] = []
            if (!albumsView.checked) contents["hiddenViews"].push("albums")
            if (!foldersView.checked) contents["hiddenViews"].push("folders")
            if (!playlistsView.checked) contents["hiddenViews"].push("playlists")
            // TODO: albumSort
            dbDocument.contents = contents
        }

        Item {
            id: topSpacer
            height: units.gu(2)
            width: parent.width
        }

        Grid {
            anchors.horizontalCenter: parent.horizontalCenter
            columns: 2
            rowSpacing: units.gu(2)
            columnSpacing: parent.width/10
            height: childrenRect.height
            //width: aboutTabLayout.width*0.25
            //height: iconTabletItem.height*0.75

            Component.onCompleted: {
                for (var i = 0; i < children.length; i++) {
                    if (children[i].checked !== undefined) { //Is CheckBox
                        children[i].onCheckedChanged.connect(column.saveDataToU1db)
                    }
                }
            }

            Component.onDestruction: {
                for (var i = 0; i < children.length; i++) {
                    if (children[i].checked !== undefined) { //Is CheckBox
                        children[i].onCheckedChanged.disconnect(column.saveDataToU1db) //Needed when created dynamically
                    }
                }
            }

            Label {
                id: playQueueScrollLabel
                text: i18n.tr("Scroll play queue to active track:")
                fontSize: "medium"
                verticalAlignment: Text.AlignVCenter
                height: playQueueScroll.height
            }

            CheckBox {
                id: playQueueScroll
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: coverFetch
            }

            Label {
                id: coverFetchLabel
                text: i18n.tr("Fetch missing covers from last.fm:")
                fontSize: "medium"
                verticalAlignment: Text.AlignVCenter
                height: coverFetch.height
            }

            CheckBox {
                id: coverFetch
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: artistYear
                KeyNavigation.backtab: playQueueScroll
            }

            Label {
                id: artistYearLabel
                text: i18n.tr("Sort albums in artists view by year:")
                fontSize: "medium"
                verticalAlignment: Text.AlignVCenter
                height: artistYear.height
            }

            CheckBox {
                id: artistYear
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: albumsView
                KeyNavigation.backtab: coverFetch
            }

            Label {
                id: albumsViewLabel
                text: i18n.tr("Show albums view:")
                fontSize: "medium"
                verticalAlignment: Text.AlignVCenter
                height: albumsView.height
            }

            CheckBox {
                id: albumsView
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: foldersView
                KeyNavigation.backtab: coverFetch
            }

            Label {
                id: foldersViewLabel
                text: i18n.tr("Show folders view:")
                fontSize: "medium"
                verticalAlignment: Text.AlignVCenter
                height: foldersView.height
            }

            CheckBox {
                id: foldersView
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: albumSort
                KeyNavigation.backtab: playlistsView
            }

            Label {
                id: playlistsViewLabel
                text: i18n.tr("Show playlists view:")
                fontSize: "medium"
                verticalAlignment: Text.AlignVCenter
                height: playlistsView.height
            }

            CheckBox {
                id: playlistsView
                KeyNavigation.priority: KeyNavigation.BeforeItem
                KeyNavigation.tab: albumSort
                KeyNavigation.backtab: foldersView
            }
        }

        Item {
            id: spacer
            height: units.gu(1)
            width: parent.width
        }

        OptionSelector {
            id: albumSort
            text: i18n.tr("Sort albums in albums view by:")
            model: [ i18n.tr("Album, Artist, Year"),
                i18n.tr("Album, Year, Artist"),
                i18n.tr("Artist, Album, Year"),
                i18n.tr("Artist, Year, Album"),
                i18n.tr("Year, Album, Artist"),
                i18n.tr("Year, Artist, Album") ]
            selectedIndex: 2
            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.backtab: foldersView
        }

        Item {
            id: bottomSpacer
            height: units.gu(2)
            width: parent.width
        }
    }
}
