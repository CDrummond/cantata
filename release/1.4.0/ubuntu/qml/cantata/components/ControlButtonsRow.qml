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
import 'qrc:/qml/cantata'

Row {
    id: buttonsRow
    height: Math.max(previousSongButton.height, playPauseButton.height, nextSongButton.height)
    width: 3 * currentlyPlayingPage.buttonSize + 2 * spacing

    Image {
        id: previousSongButton
        width: currentlyPlayingPage.buttonSize
        height: currentlyPlayingPage.buttonSize
        smooth: true
        source: "../../../icons/toolbar/media-skip-backward.svg"

        MouseArea {
            onClicked: {
                backend.previousSong()
            }
            anchors.fill: parent
            preventStealing: true
        }
    }

    Image {
        id: playPauseButton
        width: currentlyPlayingPage.buttonSize
        height: currentlyPlayingPage.buttonSize
        smooth: true
        source: backend.isPlaying?"../../../icons/toolbar/media-playback-pause.svg":"../../../icons/toolbar/media-playback-start.svg"

        MouseArea {
            onClicked: {
                backend.playPause()
            }
            anchors.fill: parent
            preventStealing: true
        }
    }

    Image {
        id: nextSongButton
        width: currentlyPlayingPage.buttonSize
        height: currentlyPlayingPage.buttonSize
        smooth: true
        source: "../../../icons/toolbar/media-skip-forward.svg"

        MouseArea {
            onClicked: {
                backend.nextSong()
            }
            anchors.fill: parent
            preventStealing: true
        }
    }
}
