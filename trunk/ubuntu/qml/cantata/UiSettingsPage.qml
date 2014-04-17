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
//import QtQuick.Controls 1.1
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem
import U1db 1.0 as U1db

Page {
    id: uiSettingsPage

    visible: false
    width: parent.width
    title: i18n.tr("UI Settings")

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

    // TODO: Save settings??? How/when??
    Flickable {
        anchors.fill: parent
        contentHeight: column.height
        Column {
            id: column
            spacing: units.gu(1)
            //height: parent.height - parent.header.height
            y: units.gu(2)
            anchors.horizontalCenter: parent.horizontalCenter

            Component.onCompleted: {
                fillWithU1dbData()
            }

            function fillWithU1dbData() {
                // TODO: albumSort and hiddenViews
                var contents = dbDocument.contents
                if (typeof contents != "undefined") {
                    artistYear.checked = contents["artistYear"]
                    coverFetch.checked = contents["coverFetch"]
                    playQueueScroll.checked = contents["playQueueScroll"]
                } else {
                    artistYear.checked = true
                    coverFetch.checked = true
                    playQueueScroll.checked = true
                }
          }

           function saveDataToU1db() {
               var contents = {};
               contents["artistYear"] = artistYear.isChecked
               contents["coverFetch"] = coverFetch.isChecked
               contents["playQueueScroll"] = playQueueScroll.isChecked
               // TODO: albumSort and hiddenViews
               dbDocument.contents = contents
           }

           Grid {
               anchors.horizontalCenter: parent.horizontalCenter
               columns: 2
               rowSpacing: units.gu(2)
               columnSpacing: parent.width/10
               //width: aboutTabLayout.width*0.25
               //height: iconTabletItem.height*0.75

               Label {
                   id: playQueueScrollLabel
                   text: i18n.tr("Scroll play queue to active track:")
                   fontSize: "medium"
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
               }
       
               CheckBox {
                   id: playlistsView
                   KeyNavigation.priority: KeyNavigation.BeforeItem
                   KeyNavigation.tab: albumSort
                   KeyNavigation.backtab: foldersView
               }
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
        }
    }
}

