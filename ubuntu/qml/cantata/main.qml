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
import 'qrc:/qml/cantata/'
import 'qrc:/qml/cantata/components'
import 'qrc:/qml/cantata/settings'
 
MainView {
    id: root
    objectName: "mainView"
    applicationName: "com.ubuntu.developer.nikwen.cantata-touch"
 
    width: units.gu(100)
    height: units.gu(75)

    backgroundColor: "#395996"

    property bool isPhone: width < units.gu(60)

    PageStack {
        id: pageStack
        anchors.fill: parent

        Component.onCompleted: {
            push(tabsPage)
            push(hostSettingsPage)
        }

        Page {
            id: tabsPage
            visible: false
            anchors.fill: parent

            Tabs {
                id: tabs
                anchors.fill: parent

                Tab {
                    id: artistTab
                    title: i18n.tr("Artists")

                    page: ListViewPage {
                        id: artistPage
                        model: artistsProxyModel
                        modelName: "artists"
                        emptyViewVisible: !backend.artistsFound
                        emptyViewText: i18n.tr("No artists found")
                    }
                }

                Tab {
                    id: albumsTab
                    title: i18n.tr("Albums")

                    page: ListViewPage {
                        id: albumPage
                        model: albumsProxyModel
                        modelName: "albums"
                        emptyViewVisible: !backend.albumsFound
                        emptyViewText: i18n.tr("No albums found")
                    }
                }

                Tab {
                    id: foldersTab
                    title: i18n.tr("Folders")

                    page: ListViewPage {
                        id: foldersPage
                        model: foldersProxyModel
                        modelName: "folders"
                        emptyViewVisible: !backend.foldersFound
                        emptyViewText: i18n.tr("No folders found")
                    }
                }

                Tab {
                    id: playlistsTab
                    title: i18n.tr("Playlists")

                    page: ListViewPage {
                        id: playlistsPage
                        model: playlistsProxyModel
                        modelName: "playlists"
                        editable: true
                        emptyViewVisible: !backend.playlistsFound
                        emptyViewText: i18n.tr("No playlists found")
                    }
                }
            }
        }

        CurrentlyPlayingPage {
            id: currentlyPlayingPage
        }

        SettingsPage {
            id: settingsPage
        }

        HostSettingsPage {
            id: hostSettingsPage
        }

        UiSettingsPage {
            id: uiSettingsPage
        }

        PlaybackSettingsPage {
            id: playbackSettingsPage
        }

        AboutPage {
            id: aboutPage
        }
    }

    Notification {
        id: notification
    }
}
