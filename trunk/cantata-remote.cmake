#!/bin/bash

#
# cantata-remote
#
#  Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
#
#  ----
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; see the file COPYING.  If not, write to
#  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA 02110-1301, USA.
#
#  ---------------------------------------------------------------------
#
#  This shell scrpt is intended to be invoked from the Cantata badge on
#  the unity task bar.
#

if [ $# -ne 1 ] ; then
    exit
fi

service=@PROJECT_REV_URL@

# If we have qdbus use that...
qt=`which qdbus`
if [ "$qt" != "" ] ; then
    $qt $service > /dev/null
    if [ $? -ne 0 ] ; then
        # Cantata not started? Try to start...
        cantata &
        sleep 1s
    fi
    $qt $service com.googlecode.cantata /org/mpris/MediaPlayer2 $1 > /dev/null
fi

# No qdbus so try dbus-send...
dbus=`which dbus-send`
if [ "$dbus" != "" ] ; then
    status=`$dbus --print-reply --reply-timeout=250 --type=method_call --session --dest=$service /org/mpris/MediaPlayer2 org.freedesktop.DBus.Peer.Ping 2>&1 | grep "org.freedesktop.DBus.Error.ServiceUnknown" | wc -l`
    if [ "$status" != "0" ] ; then
        # Cantata not started? Try to start...
        cantata &
        sleep 1s
    fi
    dbus-send --type=method_call --session --dest=$service /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.$1
fi

