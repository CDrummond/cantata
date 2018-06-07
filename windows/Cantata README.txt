Introduction
============

Cantata is a GUI client for MPD. Its developed on Linux, using the Qt
libraries.

This release of Cantata for Windows is built without ffmpeg or mpg123 so
ReplayGain calculation is not enabled.


Dynamic Playlists
=================

Cantata uses a perl helper script to facilitate dynamic playlists. This script
is packaged with the Cantata source tarball, but can also be downloaded from
the github repository:

Script:
https://raw.githubusercontent.com/CDrummond/cantata/master/playlists/cantata-dynamic

Example config file:
https://raw.githubusercontent.com/CDrummond/cantata/master/playlists/cantata-dynamic.conf

Example Systemd service file:
https://raw.githubusercontent.com/CDrummond/cantata/master/playlists/cantata-dynamic.service

This script may be run in local or server mode. In local mode, Cantata itself
starts and stops the script, and controls the loading of playlists. This mode is
currently only available for Linux builds. In server mode, it is expected that
the script is started and stopped with MPD on the host Linux machine. If this
script is running on the MPD host, then Cantata will automatically detect this
and all dynamic playlists will be stored on the server. In this way, windows
stopped with MPD on the host Linux machine. Cantata can then be configured to
builds may use the dyamic playlist functionality.

More information on how to install this script, etc, is available in the
README file of the Cantata source tarball - or:

https://raw.githubusercontent.com/CDrummond/cantata/master/README
