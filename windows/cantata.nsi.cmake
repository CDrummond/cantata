!define APPNAME "@WINDOWS_APP_NAME@"
!define COMPANYNAME "@WINDOWS_COMPANY_NAME@"
!define DESCRIPTION "MPD Client"
!define VERSIONMAJOR @CPACK_PACKAGE_VERSION_MAJOR@
!define VERSIONMINOR @CPACK_PACKAGE_VERSION_MINOR@
!define VERSIONBUILD @CPACK_PACKAGE_VERSION_PATCH@@CPACK_PACKAGE_VERSION_SPIN@
#!define HELPURL "http://..." # "Support Information" link
#!define UPDATEURL "http://..." # "Product Updates" link
!define ABOUTURL "https://github.com/CDrummond/cantata" # "Publisher" link
 
RequestExecutionLevel admin

SetCompressor /SOLID lzma
!include "MUI2.nsh"
 
InstallDir "$PROGRAMFILES\@WINDOWS_APP_NAME@"
# This will be in the installer/uninstaller's title bar
Name "@WINDOWS_APP_NAME@"
Icon "cantata.ico"
outFile "@WINDOWS_APP_NAME@-@CANTATA_VERSION_WITH_SPIN@-Setup.exe"

!define MUI_ABORTWARNING
!define MUI_ICON "cantata.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English" ;first language is the default language
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "SpanishInternational"
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "TradChinese"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "Italian"
!insertmacro MUI_LANGUAGE "Dutch"
!insertmacro MUI_LANGUAGE "Danish"
!insertmacro MUI_LANGUAGE "Swedish"
!insertmacro MUI_LANGUAGE "Norwegian"
!insertmacro MUI_LANGUAGE "NorwegianNynorsk"
!insertmacro MUI_LANGUAGE "Finnish"
!insertmacro MUI_LANGUAGE "Greek"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "Portuguese"
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "Polish"
!insertmacro MUI_LANGUAGE "Ukrainian"
!insertmacro MUI_LANGUAGE "Czech"
!insertmacro MUI_LANGUAGE "Slovak"
!insertmacro MUI_LANGUAGE "Croatian"
!insertmacro MUI_LANGUAGE "Bulgarian"
!insertmacro MUI_LANGUAGE "Hungarian"
!insertmacro MUI_LANGUAGE "Thai"
!insertmacro MUI_LANGUAGE "Romanian"
!insertmacro MUI_LANGUAGE "Latvian"
!insertmacro MUI_LANGUAGE "Macedonian"
!insertmacro MUI_LANGUAGE "Estonian"
!insertmacro MUI_LANGUAGE "Turkish"
!insertmacro MUI_LANGUAGE "Lithuanian"
!insertmacro MUI_LANGUAGE "Slovenian"
!insertmacro MUI_LANGUAGE "Serbian"
!insertmacro MUI_LANGUAGE "SerbianLatin"
!insertmacro MUI_LANGUAGE "Arabic"
!insertmacro MUI_LANGUAGE "Farsi"
!insertmacro MUI_LANGUAGE "Hebrew"
!insertmacro MUI_LANGUAGE "Indonesian"
!insertmacro MUI_LANGUAGE "Mongolian"
!insertmacro MUI_LANGUAGE "Luxembourgish"
!insertmacro MUI_LANGUAGE "Albanian"
!insertmacro MUI_LANGUAGE "Breton"
!insertmacro MUI_LANGUAGE "Belarusian"
!insertmacro MUI_LANGUAGE "Icelandic"
!insertmacro MUI_LANGUAGE "Malay"
!insertmacro MUI_LANGUAGE "Bosnian"
!insertmacro MUI_LANGUAGE "Kurdish"
!insertmacro MUI_LANGUAGE "Irish"
!insertmacro MUI_LANGUAGE "Uzbek"
!insertmacro MUI_LANGUAGE "Galician"
!insertmacro MUI_LANGUAGE "Afrikaans"
!insertmacro MUI_LANGUAGE "Catalan"
!insertmacro MUI_LANGUAGE "Esperanto"

section "install"
    # Files for the install directory - to build the installer, these should be in the same directory as the install script (this file)
    setOutPath $INSTDIR
    # Files added here should be removed by the uninstaller (see section "uninstall")
    file "cantata.exe"
    file "cantata-tags.exe"
    file "Cantata License (GPL V3).txt"
    file "Cantata README.txt"
    file "Qt5Core.dll"
    file "Qt5Gui.dll"
    file "Qt5Network.dll"
    file "Qt5Svg.dll"
    file "Qt5Widgets.dll"
    file "Qt5WinExtras.dll"
    file "Qt5Sql.dll"
    file "Qt5Multimedia.dll"

    #file "icudt52.dll"
    #file "icuin52.dll"
    #file "icuuc52.dll"
    file "libgcc_s_dw2-1.dll"
    file "libstdc++-6.dll"
    file "libtag.dll"
    file "libwinpthread-1.dll"
    file "libz-1.dll"
    @CANTATA_SSL_WIN_NSIS_INSTALL@
    setOutPath $INSTDIR\iconengines
    file "iconengines\qsvgicon.dll"
    setOutPath $INSTDIR\sqldrivers
    file "sqldrivers\qsqlite.dll"
    setOutPath $INSTDIR\platforms
    file "platforms\qwindows.dll"
    setOutPath $INSTDIR\mediaservice
    file "mediaservice\dsengine.dll"
    file "mediaservice\qtmedia_audioengine.dll"
    setOutPath $INSTDIR\icons
    file "icons\cantata.png"
    file "icons\podcasts.png"
    file "icons\soundcloud.png"
    file "icons\stream.png"

    setOutPath $INSTDIR\imageformats
    file "imageformats\qjpeg.dll"
    file "imageformats\qsvg.dll"
    setOutPath $INSTDIR\translations
    file "translations\cantata_cs.qm"
    file "translations\cantata_da.qm"
    file "translations\cantata_de.qm"
    file "translations\cantata_en_GB.qm"
    file "translations\cantata_es.qm"
    file "translations\cantata_fi.qm"
    file "translations\cantata_fr.qm"
    file "translations\cantata_hu.qm"
    file "translations\cantata_it.qm"
    file "translations\cantata_ja.qm"
    file "translations\cantata_ko.qm"
    file "translations\cantata_nl.qm"
    file "translations\cantata_pl.qm"
    file "translations\cantata_pt_BR.qm"
    file "translations\cantata_ru.qm"
    file "translations\cantata_zh_CN.qm"

    file "translations\qt_ar.qm"
    file "translations\qt_cs.qm"
    file "translations\qt_da.qm"
    file "translations\qt_de.qm"
    file "translations\qt_es.qm"
    file "translations\qt_fa.qm"
    file "translations\qt_fi.qm"
    file "translations\qt_fr.qm"
    file "translations\qt_gl.qm"
    file "translations\qt_he.qm"
    file "translations\qt_hu.qm"
    file "translations\qt_it.qm"
    file "translations\qt_ja.qm"
    file "translations\qt_ko.qm"
    file "translations\qt_lt.qm"
    file "translations\qt_pl.qm"
    file "translations\qt_pt.qm"
    file "translations\qt_ru.qm"
    file "translations\qt_sk.qm"
    file "translations\qt_sl.qm"
    file "translations\qt_sv.qm"
    file "translations\qt_uk.qm"
    file "translations\qt_zh_CN.qm"
    file "translations\qt_zh_TW.qm"
 
    writeUninstaller "$INSTDIR\uninstall.exe"
 
    # Start Menu
    createShortCut "$SMPROGRAMS\@WINDOWS_APP_NAME@.lnk" "$INSTDIR\cantata.exe" "" "$INSTDIR\cantata.exe"
 
    # Registry information for add/remove programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "DisplayName" "@WINDOWS_APP_NAME@"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "InstallLocation" "$\"$INSTDIR$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "DisplayIcon" "$\"$INSTDIR\cantata.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "Publisher" "@WINDOWS_COMPANY_NAME@"
#    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "HelpLink" "$\"${HELPURL}$\""
#    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "URLUpdateInfo" "$\"${UPDATEURL}$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "URLInfoAbout" "$\"@WINDOWS_URL@$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "DisplayVersion" "@CANTATA_VERSION_WITH_SPIN@"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "VersionMajor" @CPACK_PACKAGE_VERSION_MAJOR@
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "VersionMinor" @CPACK_PACKAGE_VERSION_MINOR@
    # There is no option for modifying or repairing the install
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "NoRepair" 1
    # Set the INSTALLSIZE constant (!defined at the top of this script) so Add/Remove Programs can accurately report the size
    # WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@" "EstimatedSize" ${INSTALLSIZE}
sectionEnd
 
# Uninstaller
 
section "uninstall"
    # Remove Start Menu launcher
    delete "$SMPROGRAMS\@WINDOWS_APP_NAME@.lnk"
 
    delete "$INSTDIR\cantata.exe"
    delete "$INSTDIR\cantata-tags.exe"
    delete "$INSTDIR\Cantata README.txt"
    delete "$INSTDIR\Cantata License (GPL V3).txt"
    delete "$INSTDIR\config\lyrics_providers.xml"
    delete "$INSTDIR\config\podcast_directories.xml"
    delete "$INSTDIR\config\scrobblers.xml"
    delete "$INSTDIR\config\tag_fixes.xml"
    delete "$INSTDIR\config\weblinks.xml"
    delete "$INSTDIR\helpers\cantata-tags.exe"
    delete "$INSTDIR\iconengines\qsvgicon4.dll"
    delete "$INSTDIR\iconengines\qsvgicon.dll"
    delete "$INSTDIR\sqldrivers\qsqlite4.dll"
    delete "$INSTDIR\sqldrivers\qsqlite.dll"
    delete "$INSTDIR\mediaservice\dsengine.dll"
    delete "$INSTDIR\mediaservice\qtmedia_audioengine.dll"
 
    delete "$INSTDIR\fonts\fontawesome-4.3.0.ttf"
    delete "$INSTDIR\fonts\fontawesome-webfont.ttf"
    delete "$INSTDIR\fonts\Cantata-FontAwesome.ttf"
    delete "$INSTDIR\icons\bbc.svg"
    delete "$INSTDIR\icons\cbc.svg"
    delete "$INSTDIR\icons\npr.svg"
    delete "$INSTDIR\icons\cantata.png"
    delete "$INSTDIR\icons\podcasts.png"
    delete "$INSTDIR\icons\soundcloud.png"
    delete "$INSTDIR\icons\stream.png"

    delete "$INSTDIR\icons\cantata\index.theme"
    delete "$INSTDIR\icons\cantata\LICENSE"
    delete "$INSTDIR\icons\cantata\AUTHORS"
    delete "$INSTDIR\icons\cantata\128\media-optical.png"
    delete "$INSTDIR\icons\cantata\128\cantata.png"
    delete "$INSTDIR\icons\cantata\64\media-optical.png"
    delete "$INSTDIR\icons\cantata\64\cantata.png"
    delete "$INSTDIR\icons\cantata\48\media-optical.png"
    delete "$INSTDIR\icons\cantata\48\cantata.png"
    delete "$INSTDIR\icons\cantata\32\media-optical.png"
    delete "$INSTDIR\icons\cantata\32\cantata.png"
    delete "$INSTDIR\icons\cantata\22\media-optical.png"
    delete "$INSTDIR\icons\cantata\22\cantata.png"
    delete "$INSTDIR\icons\cantata\16\media-optical.png"
    delete "$INSTDIR\icons\cantata\16\cantata.png"
    delete "$INSTDIR\icons\cantata\svg\audio-x-generic.svg"
    delete "$INSTDIR\icons\cantata\svg\dynamic-playlist.svg"
    delete "$INSTDIR\icons\cantata\svg\folder-downloads.svg"
    delete "$INSTDIR\icons\cantata\svg\folder-temp.svg"
    delete "$INSTDIR\icons\cantata\svg\fork.svg"
    delete "$INSTDIR\icons\cantata\svg\information.svg"
    delete "$INSTDIR\icons\cantata\svg\inode-directory.svg"
    delete "$INSTDIR\icons\cantata\svg\key.svg"
    delete "$INSTDIR\icons\cantata\svg\network-server-database.svg"
    delete "$INSTDIR\icons\cantata\svg\playlist.svg"
    delete "$INSTDIR\icons\cantata\svg\preferences-desktop-keyboard.svg"
    delete "$INSTDIR\icons\cantata\svg\preferences-other.svg"
    delete "$INSTDIR\icons\cantata\svg\speaker.svg"
    delete "$INSTDIR\icons\cantata\svg\cantata.svg"

    # Proxy icon
    delete "$INSTDIR\icons\cantata\svg\preferences-system-network.svg"
    # Device icons
    delete "$INSTDIR\icons\cantata\svg\drive-removable-media-usb-pendrive.svg"
    delete "$INSTDIR\icons\cantata\svg\multimedia-player.svg"
    # Remote device icons
    delete "$INSTDIR\icons\cantata\svg\folder-network.svg"
    delete "$INSTDIR\icons\cantata\svg\folder-samba.svg"

    delete "$INSTDIR\icons\cantata\svg64\dialog-error.svg"
    delete "$INSTDIR\icons\cantata\svg64\dialog-information.svg"
    delete "$INSTDIR\icons\cantata\svg64\dialog-question.svg"
    delete "$INSTDIR\icons\cantata\svg64\dialog-warning.svg"

    # Remove Cantata 1.x oxygen icons...
    delete "$INSTDIR\icons\oxygen\index.theme"
    delete "$INSTDIR\icons\oxygen\Oxygen License (Creative Common Attribution-ShareAlike 3.0).html"
    delete "$INSTDIR\icons\oxygen\Oxygen README.txt"
    delete "$INSTDIR\icons\oxygen\128x128\categories\applications-internet.png"
    delete "$INSTDIR\icons\oxygen\128x128\devices\media-optical.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\application-exit.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\bookmark-new.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\configure.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\dialog-cancel.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\dialog-close.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\dialog-ok.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\document-edit.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\document-export.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\document-import.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\document-new.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\document-open.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\document-save-as.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\document-save.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\edit-clear-list.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\edit-clear-locationbar-ltr.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\edit-clear-locationbar-rtl.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\edit-delete.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\edit-find.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\edit-rename.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\folder-sync.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\go-down.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\go-next.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\go-previous.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\go-up.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\list-add.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\list-remove.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\media-playback-pause.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\media-playback-start.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\media-playback-stop.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\media-skip-backward.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\media-skip-forward.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\process-stop.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\speaker.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\tools-wizard.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\fork.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\view-fullscreen.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\view-media-artist.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\view-media-playlist.png"
    delete "$INSTDIR\icons\oxygen\16x16\actions\view-refresh.png"
    delete "$INSTDIR\icons\oxygen\16x16\apps\cantata.png"
    delete "$INSTDIR\icons\oxygen\16x16\apps\clock.png"
    delete "$INSTDIR\icons\oxygen\16x16\apps\preferences-desktop-keyboard.png"
    delete "$INSTDIR\icons\oxygen\16x16\apps\system-file-manager.png"
    delete "$INSTDIR\icons\oxygen\16x16\categories\applications-internet.png"
    delete "$INSTDIR\icons\oxygen\16x16\categories\preferences-other.png"
    delete "$INSTDIR\icons\oxygen\16x16\categories\preferences-system-network.png"
    delete "$INSTDIR\icons\oxygen\16x16\devices\media-optical.png"
    delete "$INSTDIR\icons\oxygen\16x16\devices\multimedia-player.png"
    delete "$INSTDIR\icons\oxygen\16x16\mimetypes\audio-x-generic.png"
    delete "$INSTDIR\icons\oxygen\16x16\mimetypes\inode-directory.png"
    delete "$INSTDIR\icons\oxygen\16x16\places\bookmarks.png"
    delete "$INSTDIR\icons\oxygen\16x16\places\document-multiple.png"
    delete "$INSTDIR\icons\oxygen\16x16\places\favorites.png"
    delete "$INSTDIR\icons\oxygen\16x16\places\network-server.png"
    delete "$INSTDIR\icons\oxygen\16x16\places\server-database.png"
    delete "$INSTDIR\icons\oxygen\16x16\places\folder-temp.png"
    delete "$INSTDIR\icons\oxygen\16x16\places\folder-downloads.png"
    delete "$INSTDIR\icons\oxygen\16x16\status\dialog-error.png"
    delete "$INSTDIR\icons\oxygen\16x16\status\dialog-information.png"
    delete "$INSTDIR\icons\oxygen\16x16\status\dialog-warning.png"
    delete "$INSTDIR\icons\oxygen\16x16\status\media-playlist-shuffle.png"
    delete "$INSTDIR\icons\oxygen\16x16\status\object-locked.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\application-exit.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\bookmark-new.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\bookmarks.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\configure.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\dialog-cancel.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\dialog-close.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\dialog-ok.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\document-edit.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\document-export.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\document-import.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\document-new.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\document-open.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\document-save-as.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\document-save.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\edit-clear-list.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\edit-clear-locationbar-ltr.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\edit-clear-locationbar-rtl.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\edit-delete.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\edit-find.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\edit-rename.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\folder-sync.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\go-down.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\go-next.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\go-previous.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\go-up.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\list-add.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\list-remove.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\media-playback-pause.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\media-playback-start.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\media-playback-stop.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\media-skip-backward.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\media-skip-forward.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\process-stop.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\speaker.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\tools-wizard.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\fork.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\view-fullscreen.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\view-media-artist.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\view-media-playlist.png"
    delete "$INSTDIR\icons\oxygen\22x22\actions\view-refresh.png"
    delete "$INSTDIR\icons\oxygen\22x22\apps\cantata.png"
    delete "$INSTDIR\icons\oxygen\22x22\apps\clock.png"
    delete "$INSTDIR\icons\oxygen\22x22\apps\preferences-desktop-keyboard.png"
    delete "$INSTDIR\icons\oxygen\22x22\apps\system-file-manager.png"
    delete "$INSTDIR\icons\oxygen\22x22\categories\applications-internet.png"
    delete "$INSTDIR\icons\oxygen\22x22\categories\preferences-other.png"
    delete "$INSTDIR\icons\oxygen\22x22\categories\preferences-system-network.png"
    delete "$INSTDIR\icons\oxygen\22x22\devices\media-optical.png"
    delete "$INSTDIR\icons\oxygen\22x22\devices\multimedia-player.png"
    delete "$INSTDIR\icons\oxygen\22x22\mimetypes\audio-x-generic.png"
    delete "$INSTDIR\icons\oxygen\22x22\mimetypes\inode-directory.png"
    delete "$INSTDIR\icons\oxygen\22x22\places\bookmarks.png"
    delete "$INSTDIR\icons\oxygen\22x22\places\document-multiple.png"
    delete "$INSTDIR\icons\oxygen\22x22\places\favorites.png"
    delete "$INSTDIR\icons\oxygen\22x22\places\network-server.png"
    delete "$INSTDIR\icons\oxygen\22x22\places\server-database.png"
    delete "$INSTDIR\icons\oxygen\22x22\places\folder-temp.png"
    delete "$INSTDIR\icons\oxygen\22x22\places\folder-downloads.png"
    delete "$INSTDIR\icons\oxygen\22x22\status\dialog-error.png"
    delete "$INSTDIR\icons\oxygen\22x22\status\dialog-information.png"
    delete "$INSTDIR\icons\oxygen\22x22\status\dialog-warning.png"
    delete "$INSTDIR\icons\oxygen\22x22\status\media-playlist-shuffle.png"
    delete "$INSTDIR\icons\oxygen\22x22\status\object-locked.png"
    delete "$INSTDIR\icons\oxygen\24x24\apps\cantata.png"
    delete "$INSTDIR\icons\oxygen\256x256\categories\applications-internet.png"
    delete "$INSTDIR\icons\oxygen\256x256\devices\media-optical.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\application-exit.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\bookmark-new.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\configure.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\dialog-cancel.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\dialog-close.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\dialog-ok.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\document-edit.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\document-export.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\document-import.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\document-new.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\document-open.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\document-save-as.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\document-save.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\edit-clear-list.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\edit-clear-locationbar-ltr.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\edit-clear-locationbar-rtl.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\edit-delete.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\edit-find.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\edit-rename.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\folder-sync.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\go-down.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\go-next.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\go-previous.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\go-up.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\list-add.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\list-remove.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\media-playback-pause.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\media-playback-start.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\media-playback-stop.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\media-skip-backward.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\media-skip-forward.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\process-stop.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\speaker.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\tools-wizard.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\fork.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\view-fullscreen.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\view-media-artist.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\view-media-playlist.png"
    delete "$INSTDIR\icons\oxygen\32x32\actions\view-refresh.png"
    delete "$INSTDIR\icons\oxygen\32x32\apps\cantata.png"
    delete "$INSTDIR\icons\oxygen\32x32\apps\clock.png"
    delete "$INSTDIR\icons\oxygen\32x32\apps\preferences-desktop-keyboard.png"
    delete "$INSTDIR\icons\oxygen\32x32\apps\system-file-manager.png"
    delete "$INSTDIR\icons\oxygen\32x32\categories\applications-internet.png"
    delete "$INSTDIR\icons\oxygen\32x32\categories\preferences-other.png"
    delete "$INSTDIR\icons\oxygen\32x32\categories\preferences-system-network.png"
    delete "$INSTDIR\icons\oxygen\32x32\devices\media-optical.png"
    delete "$INSTDIR\icons\oxygen\32x32\devices\multimedia-player.png"
    delete "$INSTDIR\icons\oxygen\32x32\mimetypes\audio-x-generic.png"
    delete "$INSTDIR\icons\oxygen\32x32\mimetypes\inode-directory.png"
    delete "$INSTDIR\icons\oxygen\32x32\places\bookmarks.png"
    delete "$INSTDIR\icons\oxygen\32x32\places\document-multiple.png"
    delete "$INSTDIR\icons\oxygen\32x32\places\favorites.png"
    delete "$INSTDIR\icons\oxygen\32x32\places\network-server.png"
    delete "$INSTDIR\icons\oxygen\32x32\places\server-database.png"
    delete "$INSTDIR\icons\oxygen\32x32\places\folder-temp.png"
    delete "$INSTDIR\icons\oxygen\32x32\places\folder-downloads.png"
    delete "$INSTDIR\icons\oxygen\32x32\status\dialog-error.png"
    delete "$INSTDIR\icons\oxygen\32x32\status\dialog-information.png"
    delete "$INSTDIR\icons\oxygen\32x32\status\dialog-warning.png"
    delete "$INSTDIR\icons\oxygen\32x32\status\media-playlist-shuffle.png"
    delete "$INSTDIR\icons\oxygen\32x32\status\object-locked.png"
    delete "$INSTDIR\icons\oxygen\48x48\actions\bookmark-new.png"
    delete "$INSTDIR\icons\oxygen\48x48\actions\edit-find.png"
    delete "$INSTDIR\icons\oxygen\48x48\actions\go-down.png"
    delete "$INSTDIR\icons\oxygen\48x48\actions\fork.png"
    delete "$INSTDIR\icons\oxygen\48x48\actions\view-fullscreen.png"
    delete "$INSTDIR\icons\oxygen\48x48\actions\view-media-playlist.png"
    delete "$INSTDIR\icons\oxygen\48x48\apps\cantata.png"
    delete "$INSTDIR\icons\oxygen\48x48\apps\clock.png"
    delete "$INSTDIR\icons\oxygen\48x48\apps\preferences-desktop-keyboard.png"
    delete "$INSTDIR\icons\oxygen\48x48\categories\applications-internet.png"
    delete "$INSTDIR\icons\oxygen\48x48\categories\preferences-other.png"
    delete "$INSTDIR\icons\oxygen\48x48\categories\preferences-system-network.png"
    delete "$INSTDIR\icons\oxygen\48x48\devices\media-optical.png"
    delete "$INSTDIR\icons\oxygen\48x48\devices\multimedia-player.png"
    delete "$INSTDIR\icons\oxygen\48x48\places\bookmarks.png"
    delete "$INSTDIR\icons\oxygen\48x48\places\document-multiple.png"
    delete "$INSTDIR\icons\oxygen\48x48\places\folder-temp.png"
    delete "$INSTDIR\icons\oxygen\48x48\places\folder-downloads.png"
    delete "$INSTDIR\icons\oxygen\48x48\status\dialog-error.png"
    delete "$INSTDIR\icons\oxygen\48x48\status\dialog-information.png"
    delete "$INSTDIR\icons\oxygen\48x48\status\dialog-warning.png"
    delete "$INSTDIR\icons\oxygen\48x48\status\media-playlist-shuffle.png"
    delete "$INSTDIR\icons\oxygen\48x48\status\object-locked.png"
    delete "$INSTDIR\icons\oxygen\64x64\actions\bookmark-new.png"
    delete "$INSTDIR\icons\oxygen\64x64\actions\edit-find.png"
    delete "$INSTDIR\icons\oxygen\64x64\actions\go-down.png"
    delete "$INSTDIR\icons\oxygen\64x64\actions\view-media-playlist.png"
    delete "$INSTDIR\icons\oxygen\64x64\apps\cantata.png"
    delete "$INSTDIR\icons\oxygen\64x64\apps\clock.png"
    delete "$INSTDIR\icons\oxygen\64x64\apps\preferences-desktop-keyboard.png"
    delete "$INSTDIR\icons\oxygen\64x64\categories\applications-internet.png"
    delete "$INSTDIR\icons\oxygen\64x64\categories\preferences-other.png"
    delete "$INSTDIR\icons\oxygen\64x64\categories\preferences-system-network.png"
    delete "$INSTDIR\icons\oxygen\64x64\devices\media-optical.png"
    delete "$INSTDIR\icons\oxygen\64x64\devices\multimedia-player.png"
    delete "$INSTDIR\icons\oxygen\64x64\places\bookmarks.png"
    delete "$INSTDIR\icons\oxygen\64x64\places\document-multiple.png"
    delete "$INSTDIR\icons\oxygen\64x64\places\folder-temp.png"
    delete "$INSTDIR\icons\oxygen\64x64\places\folder-downloads.png"
    delete "$INSTDIR\icons\oxygen\64x64\status\dialog-error.png"
    delete "$INSTDIR\icons\oxygen\64x64\status\dialog-information.png"
    delete "$INSTDIR\icons\oxygen\64x64\status\dialog-warning.png"
    delete "$INSTDIR\icons\oxygen\scalable\apps\cantata.svg"

    delete "$INSTDIR\imageformats\qjpeg4.dll"
    delete "$INSTDIR\imageformats\qsvg4.dll"
    delete "$INSTDIR\imageformats\qjpeg.dll"
    delete "$INSTDIR\imageformats\qsvg.dll"
    delete "$INSTDIR\platforms\qwindows.dll"
    delete "$INSTDIR\Qt4 README.txt"
    delete "$INSTDIR\Qt License (LGPL V2).txt"
    delete "$INSTDIR\QtNetwork4.dll"
    delete "$INSTDIR\TagLib README.txt"

    delete "$INSTDIR\QtNetwork4.dll"
    delete "$INSTDIR\QtSvg4.dll"
    delete "$INSTDIR\QtXml4.dll"
    delete "$INSTDIR\QtCore4.dll"
    delete "$INSTDIR\QtGui4.dll"
    delete "$INSTDIR\QtSql4.dll"
    delete "$INSTDIR\libgcc_s_dw2-1.dll"
    delete "$INSTDIR\libtag.dll"
    delete "$INSTDIR\mingwm10.dll"

    delete "$INSTDIR\Qt5Core.dll"
    delete "$INSTDIR\Qt5Gui.dll"
    delete "$INSTDIR\Qt5Network.dll"
    delete "$INSTDIR\Qt5Svg.dll"
    delete "$INSTDIR\Qt5Widgets.dll"
    delete "$INSTDIR\Qt5WinExtras.dll"
    delete "$INSTDIR\Qt5Sql.dll"
    delete "$INSTDIR\Qt5Multimedia.dll"

    delete "$INSTDIR\icudt52.dll"
    delete "$INSTDIR\icuin52.dll"
    delete "$INSTDIR\icuuc52.dll"
    delete "$INSTDIR\libgcc_s_dw2-1.dll"
    delete "$INSTDIR\libstdc++-6.dll"
    delete "$INSTDIR\libwinpthread-1.dll"
    delete "$INSTDIR\zlib1.dll"
    delete "$INSTDIR\libz-1.dll"
    delete "$INSTDIR\libeay32.dll"
    delete "$INSTDIR\ssleay32.dll"

    delete "$INSTDIR\translations\cantata_cs.qm"
    delete "$INSTDIR\translations\cantata_da.qm"
    delete "$INSTDIR\translations\cantata_de.qm"
    delete "$INSTDIR\translations\cantata_en_GB.qm"
    delete "$INSTDIR\translations\cantata_es.qm"
    delete "$INSTDIR\translations\cantata_fi.qm"
    delete "$INSTDIR\translations\cantata_fr.qm"
    delete "$INSTDIR\translations\cantata_hu.qm"
    delete "$INSTDIR\translations\cantata_it.qm"
    delete "$INSTDIR\translations\cantata_ja.qm"
    delete "$INSTDIR\translations\cantata_ko.qm"
    delete "$INSTDIR\translations\cantata_nl.qm"
    delete "$INSTDIR\translations\cantata_pl.qm"
    delete "$INSTDIR\translations\cantata_pt_BR.qm"
    delete "$INSTDIR\translations\cantata_ru.qm"
    delete "$INSTDIR\translations\cantata_zh_CN.qm"
    delete "$INSTDIR\translations\qt_ar.qm"
    delete "$INSTDIR\translations\qt_cs.qm"
    delete "$INSTDIR\translations\qt_da.qm"
    delete "$INSTDIR\translations\qt_de.qm"
    delete "$INSTDIR\translations\qt_es.qm"
    delete "$INSTDIR\translations\qt_fa.qm"
    delete "$INSTDIR\translations\qt_fi.qm"
    delete "$INSTDIR\translations\qt_fr.qm"
    delete "$INSTDIR\translations\qt_gl.qm"
    delete "$INSTDIR\translations\qt_he.qm"
    delete "$INSTDIR\translations\qt_hu.qm"
    delete "$INSTDIR\translations\qt_it.qm"
    delete "$INSTDIR\translations\qt_ja.qm"
    delete "$INSTDIR\translations\qt_ko.qm"
    delete "$INSTDIR\translations\qt_lt.qm"
    delete "$INSTDIR\translations\qt_pl.qm"
    delete "$INSTDIR\translations\qt_pt.qm"
    delete "$INSTDIR\translations\qt_ru.qm"
    delete "$INSTDIR\translations\qt_sk.qm"
    delete "$INSTDIR\translations\qt_sl.qm"
    delete "$INSTDIR\translations\qt_sv.qm"
    delete "$INSTDIR\translations\qt_uk.qm"
    delete "$INSTDIR\translations\qt_zh_CN.qm"
    delete "$INSTDIR\translations\qt_zh_TW.qm"

    rmDir $INSTDIR\config
    rmDir $INSTDIR\helpers
    rmDir $INSTDIR\iconengines
    rmDir $INSTDIR\sqldrivers
    rmDir $INSTDIR\mediaservice
    rmDir $INSTDIR\fonts

    rmDir $INSTDIR\icons\cantata\128
    rmDir $INSTDIR\icons\cantata\64
    rmDir $INSTDIR\icons\cantata\48
    rmDir $INSTDIR\icons\cantata\32
    rmDir $INSTDIR\icons\cantata\22
    rmDir $INSTDIR\icons\cantata\16
    rmDir $INSTDIR\icons\cantata\svg
    rmDir $INSTDIR\icons\cantata\svg64
    rmDir $INSTDIR\icons\cantata

    # Remove Cantata 1.x oxygen icon folders...
    rmDir $INSTDIR\icons\oxygen\128x128\categories
    rmDir $INSTDIR\icons\oxygen\128x128\devices
    rmDir $INSTDIR\icons\oxygen\128x128
    rmDir $INSTDIR\icons\oxygen\16x16\actions
    rmDir $INSTDIR\icons\oxygen\16x16\apps
    rmDir $INSTDIR\icons\oxygen\16x16\categories
    rmDir $INSTDIR\icons\oxygen\16x16\devices
    rmDir $INSTDIR\icons\oxygen\16x16\mimetypes
    rmDir $INSTDIR\icons\oxygen\16x16\places
    rmDir $INSTDIR\icons\oxygen\16x16\status
    rmDir $INSTDIR\icons\oxygen\16x16
    rmDir $INSTDIR\icons\oxygen\22x22\actions
    rmDir $INSTDIR\icons\oxygen\22x22\apps
    rmDir $INSTDIR\icons\oxygen\22x22\categories
    rmDir $INSTDIR\icons\oxygen\22x22\devices
    rmDir $INSTDIR\icons\oxygen\22x22\mimetypes
    rmDir $INSTDIR\icons\oxygen\22x22\places
    rmDir $INSTDIR\icons\oxygen\22x22\status
    rmDir $INSTDIR\icons\oxygen\22x22\apps
    rmDir $INSTDIR\icons\oxygen\22x22
    rmDir $INSTDIR\icons\oxygen\256x256\categories
    rmDir $INSTDIR\icons\oxygen\256x256\devices
    rmDir $INSTDIR\icons\oxygen\256x256
    rmDir $INSTDIR\icons\oxygen\32x32\actions
    rmDir $INSTDIR\icons\oxygen\32x32\apps
    rmDir $INSTDIR\icons\oxygen\32x32\categories
    rmDir $INSTDIR\icons\oxygen\32x32\devices
    rmDir $INSTDIR\icons\oxygen\32x32\mimetypes
    rmDir $INSTDIR\icons\oxygen\32x32\places
    rmDir $INSTDIR\icons\oxygen\32x32\status
    rmDir $INSTDIR\icons\oxygen\32x32
    rmDir $INSTDIR\icons\oxygen\48x48\actions
    rmDir $INSTDIR\icons\oxygen\48x48\apps
    rmDir $INSTDIR\icons\oxygen\48x48\categories
    rmDir $INSTDIR\icons\oxygen\48x48\devices
    rmDir $INSTDIR\icons\oxygen\48x48\places
    rmDir $INSTDIR\icons\oxygen\48x48\status
    rmDir $INSTDIR\icons\oxygen\48x48
    rmDir $INSTDIR\icons\oxygen\64x64\actions
    rmDir $INSTDIR\icons\oxygen\64x64\apps
    rmDir $INSTDIR\icons\oxygen\64x64\categories
    rmDir $INSTDIR\icons\oxygen\64x64\devices
    rmDir $INSTDIR\icons\oxygen\64x64\places
    rmDir $INSTDIR\icons\oxygen\64x64\status
    rmDir $INSTDIR\icons\oxygen\64x64
    rmDir $INSTDIR\icons\oxygen\scalable\apps
    rmDir $INSTDIR\icons\oxygen\scalable
    rmDir $INSTDIR\icons\oxygen

    rmDir $INSTDIR\icons
    rmDir $INSTDIR\imageformats
    rmDir $INSTDIR\platforms
    rmDir $INSTDIR\translations

    # Always delete uninstaller as the last action
    delete $INSTDIR\uninstall.exe
 
    # Try to remove the install directory - this will only happen if it is empty
    rmDir $INSTDIR
 
    # Remove uninstaller information from the registry
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@"
sectionEnd
