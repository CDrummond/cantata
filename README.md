Introduction
============

Cantata is a graphical client for MPD, supporting the following features:

 1. Support for Qt4, Qt5, KDE, MacOSX, and Windows.
 2. Multiple MPD collections.
 3. Highly customisable layout.
 4. Songs grouped by album in play queue.
 5. Context view to show artist, album, and song information of current track.
 6. Simple tag editor.
 7. File organizer - use tags to organize files and folders.
 8. Ability to calculate ReplyGain tags. (Linux only, and if relevant libraries
    installed)
 9. Dynamic playlists.
 10. Online services; Jamendo, Magnatune, SoundCloud, and Podcasts.
 11. Radio stream support - with the ability to search for streams via TuneIn,
    ShoutCast, or Dirble.
 12. USB-Mass-Storage and MTP device support. (Linux only, and if relevant
    libraries installed)
 13. Audio CD ripping and playback. (Linux only, and if relevant libraries
    installed)
 14. Playback of non-MPD songs - via simple in-built HTTP server if connected
    to MPD via a standard socket, otherwise filepath is sent to MPD.
 15. MPRISv2 DBUS interface.
 16. Support for KDE global shortcuts (KDE builds), GNOME media keys
    (Linux only), and standard media keys (via Qxt)
 17. Ubuntu/ambiance theme integration - including dragging of window via
    toolbar.
 18. Basic support for touch-style interface (views are made 'flickable')
 19. Scrobbling.
 20. Ratings support.

Cantata started off as a fork of QtMPC, mainly to provide better KDE
integration - by using KDE libraries/classes wherever possible. However, the
code (and user interface) is now *very* different to that of QtMPC, and both
KDE and Qt (Linux) builds have the same feature set. Also, as of 1.4.0, by
default Cantata is built as a Qt-only application (with no KDE dependencies)

For more detailed information, please refer to the main [README](https://raw.githubusercontent.com/CDrummond/cantata/master/README)

Screenshots
===========

Some (outdated, 1.x) screenshots can be found at the [kde-apps](http://kde-apps.org/content/show.php/Cantata?content=147733) page.

Downloads
=========

Curently I'm developing v2.0, and as such the older 1.x is no longer actively
maintained (the code base is *very* different). However, these older versions
may still be downloaded from [Cantata's github wiki](https://github.com/CDrummond/cantata/wiki/Previous-%28Google-Code%29-Downloads)
