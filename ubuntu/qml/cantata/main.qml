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

                        emptyViewVisible: !backend.artistsFound
                        emptyViewText: i18n.tr("No artists found")

                        function add(index, replace) {
                            backend.add("artists", index, replace)
                            pageStack.push(currentlyPlayingPage)
                        }

                        function onDelegateClicked(index, text) {
                            var component = Qt.createComponent("SubListViewPage.qml")

                            var page = component.createObject(parent, {"model": model, "title": text, "modelName": "artists"})
                            page.init([index], 2)
                            pageStack.push(page)
                        }
                    }
                }

                Tab {
                    id: albumsTab
                    title: i18n.tr("Albums")

                    page: ListViewPage {
                        id: albumPage

                        model: albumsProxyModel

                        emptyViewVisible: !backend.albumsFound
                        emptyViewText: i18n.tr("No albums found")

                        function add(index, replace) {
                            backend.add("albums", index, replace)
                            pageStack.push(currentlyPlayingPage)
                        }

                        function onDelegateClicked(index, text) {
                            var component = Qt.createComponent("SubListViewPage.qml")

                            var page = component.createObject(parent, {"model": model, "title": text, "modelName": "albums"})
                            page.init([index], 1)
                            pageStack.push(page)
                        }
                    }
                }

                Tab {
                    id: playlistsTab
                    title: i18n.tr("Playlists")

                    page: ListViewPage { //TODO: Disable second button!!!
                        id: playlistsPage

                        model: playlistsProxyModel

                        emptyViewVisible: !backend.playlistsFound
                        emptyViewText: i18n.tr("No playlists found")

                        function add(index, replace) {
                            backend.loadPlaylist(index)
                            pageStack.push(currentlyPlayingPage)
                        }

                        function onDelegateClicked(index, text) {
                            var component = Qt.createComponent("SubListViewPage.qml")

                            var page = component.createObject(parent, {"model": model, "title": text, "modelName": "playlists"})
                            page.init([index], 1)
                            pageStack.push(page)
                        }
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
}
