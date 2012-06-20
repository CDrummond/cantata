QT += core gui xml network svg

TARGET = cantata
TEMPLATE = app
#QTPLUGIN += qsvg

# androidmain.cpp
SOURCES += \
    ../application.cpp \
    ../main.cpp \
    ../gui/mainwindow.cpp \
    ../gui/preferencesdialog.cpp \
    ../gui/settings.cpp \
    ../gui/covers.cpp \
    ../gui/interfacesettings.cpp \
    ../gui/externalsettings.cpp \
    ../gui/playbacksettings.cpp \
    ../gui/serverplaybacksettings.cpp \
    ../gui/serversettings.cpp \
    ../gui/streamspage.cpp \
    ../gui/streamdialog.cpp \
    ../gui/streamcategorydialog.cpp \
    ../gui/librarypage.cpp \
    ../gui/albumspage.cpp \
    ../gui/folderpage.cpp \
    ../gui/serverinfopage.cpp \
    ../gui/playlistspage.cpp \
    ../models/musiclibraryitemroot.cpp \
    ../models/musiclibraryitemartist.cpp \
    ../models/musiclibraryitemalbum.cpp \
    ../models/musiclibrarymodel.cpp \
    ../models/musiclibraryproxymodel.cpp \
    ../models/playlistsmodel.cpp \
    ../models/playlistsproxymodel.cpp \
    ../models/playqueuemodel.cpp \
    ../models/playqueueproxymodel.cpp \
    ../models/dirviewmodel.cpp \
    ../models/dirviewproxymodel.cpp \
    ../models/dirviewitem.cpp \
    ../models/dirviewitemdir.cpp \
    ../models/streamsmodel.cpp \
    ../models/streamsproxymodel.cpp \
    ../models/albumsmodel.cpp \
    ../models/albumsproxymodel.cpp \
    ../models/streamfetcher.cpp \
    ../models/proxymodel.cpp \
    ../mpd/mpdconnection.cpp \
    ../mpd/mpdparseutils.cpp \
    ../mpd/mpdstatus.cpp \
    ../mpd/song.cpp \
    ../widgets/treeview.cpp \
    ../widgets/fancytabwidget.cpp \
    ../widgets/listview.cpp \
    ../widgets/itemview.cpp \
    ../widgets/timeslider.cpp \
    ../widgets/actionlabel.cpp \
    ../widgets/playqueueview.cpp \
    ../widgets/groupedview.cpp \
    ../widgets/messagewidget.cpp \
    ../widgets/actionitemdelegate.cpp \
    ../widgets/textbrowser.cpp \
    ../widgets/coverwidget.cpp \
    ../lyrics/lyricspage.cpp \
    ../lyrics/lyricsettings.cpp \
    ../lyrics/ultimatelyricsprovider.cpp \
    ../lyrics/ultimatelyricsreader.cpp \
    ../lyrics/songinfoprovider.cpp \
    ../lyrics/lyricsdialog.cpp \
    ../network/networkaccessmanager.cpp \
    ../network/network.cpp \
    ../libmaia/maiaObject.cpp \
    ../libmaia/maiaFault.cpp \
    ../libmaia/maiaXmlRpcClient.cpp \
    ../devices/utils.cpp \
    ../widgets/dirrequester.cpp \
    ../widgets/lineedit.cpp \
    ../network/networkproxyfactory.cpp \
    ../widgets/kmessagewidget.cpp \
    ../widgets/dialog.cpp \
    ../widgets/messagebox.cpp \
    ../widgets/icon.cpp \
    ../widgets/togglebutton.cpp

HEADERS+= \
    ../gui/mainwindow.h \
    ../gui/settings.h \
    ../gui/covers.h \
    ../gui/folderpage.h \
    ../gui/librarypage.h \
    ../gui/albumspage.h \
    ../gui/playlistspage.h \
    ../gui/streamspage.h \
    ../gui/serverinfopage.h \
    ../gui/streamdialog.h \
    ../gui/streamcategorydialog.h \
    ../gui/serverplaybacksettings.h \
    ../gui/serversettings.h \
    ../gui/preferencesdialog.h \
    ../gui/interfacesettings.h \
    ../gui/externalsettings.h \
    ../models/musiclibrarymodel.h \
    ../models/musiclibraryproxymodel.h \
    ../models/playlistsmodel.h \
    ../models/playlistsproxymodel.h \
    ../models/playqueuemodel.h \
    ../models/playqueueproxymodel.h \
    ../models/dirviewmodel.h \
    ../models/dirviewproxymodel.h \
    ../models/albumsmodel.h \
    ../models/streamfetcher.h \
    ../models/streamsmodel.h \
    ../mpd/mpdconnection.h \
    ../mpd/mpdstatus.h \
    ../widgets/fancytabwidget.h \
    ../widgets/treeview.h \
    ../widgets/listview.h \
    ../widgets/itemview.h \
    ../widgets/timeslider.h \
    ../widgets/actionlabel.h \
    ../widgets/playqueueview.h \
    ../widgets/groupedview.h \
    ../widgets/messagewidget.h \
    ../widgets/coverwidget.h \
    ../lyrics/lyricspage.h \
    ../lyrics/lyricsettings.h \
    ../lyrics/ultimatelyricsprovider.h \
    ../lyrics/songinfoprovider.h \
    ../lyrics/lyricsdialog.h \
    ../network/networkaccessmanager.h \
    ../network/network.h \
    ../libmaia/maiaObject.h \
    ../libmaia/maiaFault.h \
    ../libmaia/maiaXmlRpcClient.h \
    ../application.h \
    ../widgets/dirrequester.h \
    ../widgets/lineedit.h \
    ../widgets/kmessagewidget.h \
    ../widgets/urllabel.h \
    ../widgets/dialog.h

FORMS+= \
    ../gui/mainwindow.ui \
    ../gui/folderpage.ui \
    ../gui/librarypage.ui \
    ../gui/albumspage.ui \
    ../gui/playlistspage.ui \
    ../gui/streamspage.ui \
    ../gui/serverinfopage.ui \
    ../gui/interfacesettings.ui \
    ../gui/externalsettings.ui \
    ../gui/playbacksettings.ui \
    ../gui/serverplaybacksettings.ui \
    ../gui/serversettings.ui \
    ../lyrics/lyricspage.ui \
    ../lyrics/lyricsettings.ui \
    ../widgets/itemview.ui

INCLUDEPATH += \
    .. \
    ../devices \
    ../libmaia \
    ../gui \
    ../mpd \
    ../models \
    ../widgets \
    ../lyrics \
    ../network

RESOURCES += \
    ../cantata.qrc \
    cantata_android.qrc

DEFINES += \
    QT_NO_DEBUG_OUTPUT

OTHER_FILES += \
    android/src/org/kde/necessitas/origo/QtActivity.java \
    android/src/org/kde/necessitas/origo/QtApplication.java \
    android/src/org/kde/necessitas/ministro/IMinistroCallback.aidl \
    android/src/org/kde/necessitas/ministro/IMinistro.aidl \
    android/AndroidManifest.xml \
    android/version.xml \
    android/res/drawable-hdpi/icon.png \
    android/res/values-nb/strings.xml \
    android/res/values-ru/strings.xml \
    android/res/drawable/icon.png \
    android/res/drawable/logo.png \
    android/res/layout/splash.xml \
    android/res/values-fr/strings.xml \
    android/res/values-zh-rCN/strings.xml \
    android/res/values-de/strings.xml \
    android/res/values/strings.xml \
    android/res/values/libs.xml \
    android/res/values-ms/strings.xml \
    android/res/values-et/strings.xml \
    android/res/values-rs/strings.xml \
    android/res/values-fa/strings.xml \
    android/res/values-pl/strings.xml \
    android/res/values-pt-rBR/strings.xml \
    android/res/values-it/strings.xml \
    android/res/values-el/strings.xml \
    android/res/values-nl/strings.xml \
    android/res/values-ro/strings.xml \
    android/res/values-es/strings.xml \
    android/res/values-ja/strings.xml \
    android/res/values-id/strings.xml \
    android/res/drawable-mdpi/icon.png \
    android/res/drawable-ldpi/icon.png \
    android/res/values-zh-rTW/strings.xml \
    android/src/org/kde/necessitas/origo/QtActivity.java \
    android/src/org/kde/necessitas/origo/QtApplication.java \
    android/src/org/kde/necessitas/ministro/IMinistroCallback.aidl \
    android/src/org/kde/necessitas/ministro/IMinistro.aidl \
    android/AndroidManifest.xml \
    android/version.xml \
    android/res/drawable-hdpi/icon.png \
    android/res/values-nb/strings.xml \
    android/res/values-ru/strings.xml \
    android/res/drawable/icon.png \
    android/res/drawable/logo.png \
    android/res/layout/splash.xml \
    android/res/values-fr/strings.xml \
    android/res/values-zh-rCN/strings.xml \
    android/res/values-de/strings.xml \
    android/res/values/strings.xml \
    android/res/values/libs.xml \
    android/res/values-ms/strings.xml \
    android/res/values-et/strings.xml \
    android/res/values-rs/strings.xml \
    android/res/values-fa/strings.xml \
    android/res/values-pl/strings.xml \
    android/res/values-pt-rBR/strings.xml \
    android/res/values-it/strings.xml \
    android/res/values-el/strings.xml \
    android/res/values-nl/strings.xml \
    android/res/values-ro/strings.xml \
    android/res/values-es/strings.xml \
    android/res/values-ja/strings.xml \
    android/res/values-id/strings.xml \
    android/res/drawable-mdpi/icon.png \
    android/res/drawable-ldpi/icon.png \
    android/res/values-zh-rTW/strings.xml


