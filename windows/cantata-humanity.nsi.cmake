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
    file "Qt4 README.txt"
    file "Qt License (LGPL V2).txt"
    file "TagLib README.txt"
    file "libz-1.dll"
    @CANTATA_SSL_WIN_NSIS_INSTALL@
    file "libgcc_s_dw2-1.dll"
    file "libtag.dll"
    file "mingwm10.dll"
    file "QtCore4.dll"
    file "QtGui4.dll"
    file "QtNetwork4.dll"
    file "QtSvg4.dll"
    file "QtXml4.dll"
    file "QtSql4.dll"
    file "libtag.dll"
    setOutPath $INSTDIR\config
    file "config\lyrics_providers.xml"
    file "config\podcast_directories.xml"
    file "config\scrobblers.xml"
    file "config\tag_fixes.xml"
    file "config\weblinks.xml"
    setOutPath $INSTDIR\iconengines
    file "iconengines\qsvgicon4.dll"
    setOutPath $INSTDIR\sqldrivers
    file "sqldrivers\qsqlite4.dll"
    setOutPath $INSTDIR\icons
    file "icons\bbc.svg"
    file "icons\cbc.svg"
    file "icons\npr.svg"
    file "icons\podcasts.png"
    file "icons\soundcloud.png"
    file "icons\stream.png"
    setOutPath $INSTDIR\icons\humanity
    file "icons\humanity\gnome.copyright"
    file "icons\humanity\Humanity.copyright"
    file "icons\humanity\index.theme"
    file "icons\humanity\README"
    setOutPath $INSTDIR\icons\humanity\status\16
    file "icons\humanity\status\16\dialog-information.svg"
    setOutPath $INSTDIR\icons\humanity\status\48
    file "icons\humanity\status\48\media-playlist-shuffle.svg"
    file "icons\humanity\status\48\dialog-information.svg"
    setOutPath $INSTDIR\icons\humanity\status\128
    file "icons\humanity\status\128\dialog-information.svg"
    setOutPath $INSTDIR\icons\humanity\status\24
    file "icons\humanity\status\24\media-playlist-shuffle.svg"
    file "icons\humanity\status\24\dialog-information.svg"
    setOutPath $INSTDIR\icons\humanity\status\32
    file "icons\humanity\status\32\dialog-information.svg"
    setOutPath $INSTDIR\icons\humanity\status\64
    file "icons\humanity\status\64\dialog-information.svg"
    setOutPath $INSTDIR\icons\humanity\status\22
    file "icons\humanity\status\22\dialog-information.svg"
    setOutPath $INSTDIR\icons\humanity\devices\16
    file "icons\humanity\devices\16\drive-harddisk.svg"
    file "icons\humanity\devices\16\media-optical.svg"
    file "icons\humanity\devices\16\media-optical-cd.svg"
    file "icons\humanity\devices\16\audio-speakers.png"
    setOutPath $INSTDIR\icons\humanity\devices\48
    file "icons\humanity\devices\48\drive-harddisk.svg"
    file "icons\humanity\devices\48\media-optical.svg"
    file "icons\humanity\devices\48\media-optical-cd.svg"
    file "icons\humanity\devices\48\audio-speakers.png"
    setOutPath $INSTDIR\icons\humanity\devices\128
    file "icons\humanity\devices\128\drive-harddisk.svg"
    file "icons\humanity\devices\128\media-optical.svg"
    file "icons\humanity\devices\128\media-optical-cd.svg"
    setOutPath $INSTDIR\icons\humanity\devices\24
    file "icons\humanity\devices\24\drive-harddisk.svg"
    file "icons\humanity\devices\24\media-optical.svg"
    file "icons\humanity\devices\24\media-optical-cd.svg"
    file "icons\humanity\devices\24\audio-speakers.png"
    setOutPath $INSTDIR\icons\humanity\devices\32
    file "icons\humanity\devices\32\drive-harddisk.svg"
    file "icons\humanity\devices\32\media-optical.svg"
    file "icons\humanity\devices\32\media-optical-cd.svg"
    file "icons\humanity\devices\32\audio-speakers.png"
    setOutPath $INSTDIR\icons\humanity\devices\64
    file "icons\humanity\devices\64\drive-harddisk.svg"
    file "icons\humanity\devices\64\media-optical.svg"
    file "icons\humanity\devices\64\media-optical-cd.svg"
    setOutPath $INSTDIR\icons\humanity\devices\22
    file "icons\humanity\devices\22\drive-harddisk.svg"
    file "icons\humanity\devices\22\media-optical.svg"
    file "icons\humanity\devices\22\media-optical-cd.svg"
    file "icons\humanity\devices\22\audio-speakers.png"
    setOutPath $INSTDIR\icons\humanity\places\16
    file "icons\humanity\places\16\folder-documents.svg"
    file "icons\humanity\places\16\folder-templates.svg"
    file "icons\humanity\places\16\folder-publicshare.svg"
    file "icons\humanity\places\16\inode-directory.svg"
    file "icons\humanity\places\16\folder-download.svg"
    file "icons\humanity\places\16\folder.svg"
    file "icons\humanity\places\16\user-desktop.svg"
    file "icons\humanity\places\16\folder-downloads.svg"
    file "icons\humanity\places\16\folder-music.svg"
    setOutPath $INSTDIR\icons\humanity\places\48
    file "icons\humanity\places\48\folder-documents.svg"
    file "icons\humanity\places\48\folder-templates.svg"
    file "icons\humanity\places\48\folder-publicshare.svg"
    file "icons\humanity\places\48\inode-directory.svg"
    file "icons\humanity\places\48\folder-download.svg"
    file "icons\humanity\places\48\folder.svg"
    file "icons\humanity\places\48\user-desktop.svg"
    file "icons\humanity\places\48\folder-downloads.svg"
    file "icons\humanity\places\48\folder-music.svg"
    setOutPath $INSTDIR\icons\humanity\places\128
    file "icons\humanity\places\128\user-desktop.svg"
    setOutPath $INSTDIR\icons\humanity\places\24
    file "icons\humanity\places\24\folder-documents.svg"
    file "icons\humanity\places\24\folder-templates.svg"
    file "icons\humanity\places\24\folder-publicshare.svg"
    file "icons\humanity\places\24\inode-directory.svg"
    file "icons\humanity\places\24\folder-download.svg"
    file "icons\humanity\places\24\folder.svg"
    file "icons\humanity\places\24\user-desktop.svg"
    file "icons\humanity\places\24\folder-downloads.svg"
    file "icons\humanity\places\24\folder-music.svg"
    setOutPath $INSTDIR\icons\humanity\places\32
    file "icons\humanity\places\32\folder-documents.svg"
    file "icons\humanity\places\32\folder-templates.svg"
    file "icons\humanity\places\32\folder-publicshare.svg"
    file "icons\humanity\places\32\inode-directory.svg"
    file "icons\humanity\places\32\folder-download.svg"
    file "icons\humanity\places\32\folder.svg"
    file "icons\humanity\places\32\user-desktop.svg"
    file "icons\humanity\places\32\folder-downloads.svg"
    file "icons\humanity\places\32\folder-music.svg"
    setOutPath $INSTDIR\icons\humanity\places\64
    file "icons\humanity\places\64\folder-documents.svg"
    file "icons\humanity\places\64\folder-templates.svg"
    file "icons\humanity\places\64\folder-publicshare.svg"
    file "icons\humanity\places\64\inode-directory.svg"
    file "icons\humanity\places\64\folder-download.svg"
    file "icons\humanity\places\64\folder.svg"
    file "icons\humanity\places\64\user-desktop.svg"
    file "icons\humanity\places\64\folder-downloads.svg"
    file "icons\humanity\places\64\folder-music.svg"
    setOutPath $INSTDIR\icons\humanity\places\22
    file "icons\humanity\places\22\folder-documents.svg"
    file "icons\humanity\places\22\folder-templates.svg"
    file "icons\humanity\places\22\folder-publicshare.svg"
    file "icons\humanity\places\22\inode-directory.svg"
    file "icons\humanity\places\22\folder-download.svg"
    file "icons\humanity\places\22\folder.svg"
    file "icons\humanity\places\22\user-desktop.svg"
    file "icons\humanity\places\22\folder-downloads.svg"
    file "icons\humanity\places\22\folder-music.svg"
    setOutPath $INSTDIR\icons\humanity\actions\16
    file "icons\humanity\actions\16\go-down.svg"
    file "icons\humanity\actions\16\gtk-edit.svg"
    file "icons\humanity\actions\16\edit-clear.svg"
    file "icons\humanity\actions\16\go-previous.svg"
    file "icons\humanity\actions\16\go-up.svg"
    file "icons\humanity\actions\16\document-open.svg"
    file "icons\humanity\actions\16\edit-find.png"
    file "icons\humanity\actions\16\document-new.svg"
    file "icons\humanity\actions\16\go-next.svg"
    setOutPath $INSTDIR\icons\humanity\actions\48
    file "icons\humanity\actions\48\go-down.svg"
    file "icons\humanity\actions\48\gtk-edit.svg"
    file "icons\humanity\actions\48\edit-clear.svg"
    file "icons\humanity\actions\48\gtk-execute.svg"
    file "icons\humanity\actions\48\document-open-recent.svg"
    file "icons\humanity\actions\48\go-previous.svg"
    file "icons\humanity\actions\48\go-up.svg"
    file "icons\humanity\actions\48\document-open.svg"
    file "icons\humanity\actions\48\edit-find.png"
    file "icons\humanity\actions\48\document-new.svg"
    file "icons\humanity\actions\48\go-next.svg"
    setOutPath $INSTDIR\icons\humanity\actions\128
    file "icons\humanity\actions\128\document-new.svg"
    setOutPath $INSTDIR\icons\humanity\actions\24
    file "icons\humanity\actions\24\go-down.svg"
    file "icons\humanity\actions\24\gtk-edit.svg"
    file "icons\humanity\actions\24\edit-clear.svg"
    file "icons\humanity\actions\24\gtk-execute.svg"
    file "icons\humanity\actions\24\document-open-recent.svg"
    file "icons\humanity\actions\24\go-previous.svg"
    file "icons\humanity\actions\24\go-up.svg"
    file "icons\humanity\actions\24\document-open.svg"
    file "icons\humanity\actions\24\edit-find.png"
    file "icons\humanity\actions\24\document-new.svg"
    file "icons\humanity\actions\24\go-next.svg"
    setOutPath $INSTDIR\icons\humanity\actions\32
    file "icons\humanity\actions\32\edit-find.png"
    file "icons\humanity\actions\32\document-new.svg"
    setOutPath $INSTDIR\icons\humanity\actions\64
    file "icons\humanity\actions\64\document-new.svg"
    setOutPath $INSTDIR\icons\humanity\actions\22
    file "icons\humanity\actions\22\go-down.svg"
    file "icons\humanity\actions\22\gtk-edit.svg"
    file "icons\humanity\actions\22\document-open-recent.svg"
    file "icons\humanity\actions\22\go-previous.svg"
    file "icons\humanity\actions\22\go-up.svg"
    file "icons\humanity\actions\22\edit-find.png"
    file "icons\humanity\actions\22\document-new.svg"
    file "icons\humanity\actions\22\go-next.svg"
    setOutPath $INSTDIR\icons\humanity\categories\48
    file "icons\humanity\categories\48\preferences-other.svg"
    setOutPath $INSTDIR\icons\humanity\categories\24
    file "icons\humanity\categories\24\preferences-other.svg"
    setOutPath $INSTDIR\icons\humanity\categories\64
    file "icons\humanity\categories\64\preferences-other.svg"
    setOutPath $INSTDIR\icons\humanity\mimes\16
    file "icons\humanity\mimes\16\image-png.svg"
    file "icons\humanity\mimes\16\audio-x-mp3-playlist.svg"
    file "icons\humanity\mimes\16\audio-x-generic.svg"
    setOutPath $INSTDIR\icons\humanity\mimes\48
    file "icons\humanity\mimes\48\image-png.svg"
    file "icons\humanity\mimes\48\audio-x-mp3-playlist.svg"
    file "icons\humanity\mimes\48\audio-x-generic.svg"
    setOutPath $INSTDIR\icons\humanity\mimes\24
    file "icons\humanity\mimes\24\image-png.svg"
    file "icons\humanity\mimes\24\audio-x-mp3-playlist.svg"
    file "icons\humanity\mimes\24\audio-x-generic.svg"
    setOutPath $INSTDIR\icons\humanity\mimes\22
    file "icons\humanity\mimes\22\image-png.svg"
    file "icons\humanity\mimes\22\audio-x-mp3-playlist.svg"
    file "icons\humanity\mimes\22\audio-x-generic.svg"
    setOutPath $INSTDIR\icons\humanity\apps\16
    file "icons\humanity\apps\16\preferences-desktop-keyboard.svg"
    file "icons\humanity\apps\16\cantata.png"
    setOutPath $INSTDIR\icons\humanity\apps\48
    file "icons\humanity\apps\48\preferences-desktop-keyboard.svg"
    file "icons\humanity\apps\48\cantata.png"
    setOutPath $INSTDIR\icons\humanity\apps\24
    file "icons\humanity\apps\24\preferences-desktop-keyboard.svg"
    file "icons\humanity\apps\24\cantata.png"
    setOutPath $INSTDIR\icons\humanity\apps\22
    file "icons\humanity\apps\22\cantata.png"
    setOutPath $INSTDIR\icons\humanity\apps\32
    file "icons\humanity\apps\32\cantata.png"
    setOutPath $INSTDIR\icons\humanity\apps\64
    file "icons\humanity\apps\64\cantata.png"
    setOutPath $INSTDIR\icons\humanity\apps\scalable
    file "icons\humanity\apps\scalable\cantata.svg"
    setOutPath $INSTDIR\imageformats
    file "imageformats\qjpeg4.dll"
    file "imageformats\qsvg4.dll"
    setOutPath $INSTDIR\translations
    file "translations\cantata_cs.qm"
    file "translations\cantata_de.qm"
    file "translations\cantata_en_GB.qm"
    file "translations\cantata_es.qm"
    file "translations\cantata_fr.qm"
    file "translations\cantata_hu.qm"
    file "translations\cantata_ko.qm"
    file "translations\cantata_pl.qm"
    file "translations\cantata_ru.qm"
    file "translations\cantata_zh_CN.qm"
    file "translations\qt_ar.qm"
    file "translations\qt_cs.qm"
    file "translations\qt_da.qm"
    file "translations\qt_de.qm"
    file "translations\qt_es.qm"
    file "translations\qt_fa.qm"
    file "translations\qt_fr.qm"
    file "translations\qt_gl.qm"
    file "translations\qt_he.qm"
    file "translations\qt_hu.qm"
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
    delete "$INSTDIR\sqldrivers\qsqlite4.dll"
    delete "$INSTDIR\icons\bbc.svg"
    delete "$INSTDIR\icons\cbc.svg"
    delete "$INSTDIR\icons\npr.svg"
    delete "$INSTDIR\icons\podcasts.png"
    delete "$INSTDIR\icons\soundcloud.png"
    delete "$INSTDIR\icons\stream.png"
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
    delete "$INSTDIR\icons\humanity\gnome.copyright"
    delete "$INSTDIR\icons\humanity\Humanity.copyright"
    delete "$INSTDIR\icons\humanity\index.theme"
    delete "$INSTDIR\icons\humanity\README"
    delete "$INSTDIR\icons\humanity\status\16\dialog-information.svg"
    delete "$INSTDIR\icons\humanity\status\48\media-playlist-shuffle.svg"
    delete "$INSTDIR\icons\humanity\status\48\dialog-information.svg"
    delete "$INSTDIR\icons\humanity\status\128\dialog-information.svg"
    delete "$INSTDIR\icons\humanity\status\24\media-playlist-shuffle.svg"
    delete "$INSTDIR\icons\humanity\status\24\dialog-information.svg"
    delete "$INSTDIR\icons\humanity\status\32\dialog-information.svg"
    delete "$INSTDIR\icons\humanity\status\64\dialog-information.svg"
    delete "$INSTDIR\icons\humanity\status\22\dialog-information.svg"
    delete "$INSTDIR\icons\humanity\devices\16\drive-harddisk.svg"
    delete "$INSTDIR\icons\humanity\devices\16\media-optical.svg"
    delete "$INSTDIR\icons\humanity\devices\16\media-optical-cd.svg"
    delete "$INSTDIR\icons\humanity\devices\16\audio-speakers.png"
    delete "$INSTDIR\icons\humanity\devices\48\drive-harddisk.svg"
    delete "$INSTDIR\icons\humanity\devices\48\media-optical.svg"
    delete "$INSTDIR\icons\humanity\devices\48\media-optical-cd.svg"
    delete "$INSTDIR\icons\humanity\devices\48\audio-speakers.png"
    delete "$INSTDIR\icons\humanity\devices\128\drive-harddisk.svg"
    delete "$INSTDIR\icons\humanity\devices\128\media-optical.svg"
    delete "$INSTDIR\icons\humanity\devices\128\media-optical-cd.svg"
    delete "$INSTDIR\icons\humanity\devices\24\drive-harddisk.svg"
    delete "$INSTDIR\icons\humanity\devices\24\media-optical.svg"
    delete "$INSTDIR\icons\humanity\devices\24\media-optical-cd.svg"
    delete "$INSTDIR\icons\humanity\devices\24\audio-speakers.png"
    delete "$INSTDIR\icons\humanity\devices\32\drive-harddisk.svg"
    delete "$INSTDIR\icons\humanity\devices\32\media-optical.svg"
    delete "$INSTDIR\icons\humanity\devices\32\media-optical-cd.svg"
    delete "$INSTDIR\icons\humanity\devices\32\audio-speakers.png"
    delete "$INSTDIR\icons\humanity\devices\64\drive-harddisk.svg"
    delete "$INSTDIR\icons\humanity\devices\64\media-optical.svg"
    delete "$INSTDIR\icons\humanity\devices\64\media-optical-cd.svg"
    delete "$INSTDIR\icons\humanity\devices\22\drive-harddisk.svg"
    delete "$INSTDIR\icons\humanity\devices\22\media-optical.svg"
    delete "$INSTDIR\icons\humanity\devices\22\media-optical-cd.svg"
    delete "$INSTDIR\icons\humanity\devices\22\audio-speakers.png"
    delete "$INSTDIR\icons\humanity\places\16\folder-documents.svg"
    delete "$INSTDIR\icons\humanity\places\16\folder-templates.svg"
    delete "$INSTDIR\icons\humanity\places\16\folder-publicshare.svg"
    delete "$INSTDIR\icons\humanity\places\16\inode-directory.svg"
    delete "$INSTDIR\icons\humanity\places\16\folder-download.svg"
    delete "$INSTDIR\icons\humanity\places\16\folder.svg"
    delete "$INSTDIR\icons\humanity\places\16\user-desktop.svg"
    delete "$INSTDIR\icons\humanity\places\16\folder-downloads.svg"
    delete "$INSTDIR\icons\humanity\places\16\folder-music.svg"
    delete "$INSTDIR\icons\humanity\places\48\folder-documents.svg"
    delete "$INSTDIR\icons\humanity\places\48\folder-templates.svg"
    delete "$INSTDIR\icons\humanity\places\48\folder-publicshare.svg"
    delete "$INSTDIR\icons\humanity\places\48\inode-directory.svg"
    delete "$INSTDIR\icons\humanity\places\48\folder-download.svg"
    delete "$INSTDIR\icons\humanity\places\48\folder.svg"
    delete "$INSTDIR\icons\humanity\places\48\user-desktop.svg"
    delete "$INSTDIR\icons\humanity\places\48\folder-downloads.svg"
    delete "$INSTDIR\icons\humanity\places\48\folder-music.svg"
    delete "$INSTDIR\icons\humanity\places\128\user-desktop.svg"
    delete "$INSTDIR\icons\humanity\places\24\folder-documents.svg"
    delete "$INSTDIR\icons\humanity\places\24\folder-templates.svg"
    delete "$INSTDIR\icons\humanity\places\24\folder-publicshare.svg"
    delete "$INSTDIR\icons\humanity\places\24\inode-directory.svg"
    delete "$INSTDIR\icons\humanity\places\24\folder-download.svg"
    delete "$INSTDIR\icons\humanity\places\24\folder.svg"
    delete "$INSTDIR\icons\humanity\places\24\user-desktop.svg"
    delete "$INSTDIR\icons\humanity\places\24\folder-downloads.svg"
    delete "$INSTDIR\icons\humanity\places\24\folder-music.svg"
    delete "$INSTDIR\icons\humanity\places\32\folder-documents.svg"
    delete "$INSTDIR\icons\humanity\places\32\folder-templates.svg"
    delete "$INSTDIR\icons\humanity\places\32\folder-publicshare.svg"
    delete "$INSTDIR\icons\humanity\places\32\inode-directory.svg"
    delete "$INSTDIR\icons\humanity\places\32\folder-download.svg"
    delete "$INSTDIR\icons\humanity\places\32\folder.svg"
    delete "$INSTDIR\icons\humanity\places\32\user-desktop.svg"
    delete "$INSTDIR\icons\humanity\places\32\folder-downloads.svg"
    delete "$INSTDIR\icons\humanity\places\32\folder-music.svg"
    delete "$INSTDIR\icons\humanity\places\64\folder-documents.svg"
    delete "$INSTDIR\icons\humanity\places\64\folder-templates.svg"
    delete "$INSTDIR\icons\humanity\places\64\folder-publicshare.svg"
    delete "$INSTDIR\icons\humanity\places\64\inode-directory.svg"
    delete "$INSTDIR\icons\humanity\places\64\folder-download.svg"
    delete "$INSTDIR\icons\humanity\places\64\folder.svg"
    delete "$INSTDIR\icons\humanity\places\64\user-desktop.svg"
    delete "$INSTDIR\icons\humanity\places\64\folder-downloads.svg"
    delete "$INSTDIR\icons\humanity\places\64\folder-music.svg"
    delete "$INSTDIR\icons\humanity\places\22\folder-documents.svg"
    delete "$INSTDIR\icons\humanity\places\22\folder-templates.svg"
    delete "$INSTDIR\icons\humanity\places\22\folder-publicshare.svg"
    delete "$INSTDIR\icons\humanity\places\22\inode-directory.svg"
    delete "$INSTDIR\icons\humanity\places\22\folder-download.svg"
    delete "$INSTDIR\icons\humanity\places\22\folder.svg"
    delete "$INSTDIR\icons\humanity\places\22\user-desktop.svg"
    delete "$INSTDIR\icons\humanity\places\22\folder-downloads.svg"
    delete "$INSTDIR\icons\humanity\places\22\folder-music.svg"
    delete "$INSTDIR\icons\humanity\actions\16\go-down.svg"
    delete "$INSTDIR\icons\humanity\actions\16\gtk-edit.svg"
    delete "$INSTDIR\icons\humanity\actions\16\edit-clear.svg"
    delete "$INSTDIR\icons\humanity\actions\16\go-previous.svg"
    delete "$INSTDIR\icons\humanity\actions\16\go-up.svg"
    delete "$INSTDIR\icons\humanity\actions\16\document-open.svg"
    delete "$INSTDIR\icons\humanity\actions\16\edit-find.png"
    delete "$INSTDIR\icons\humanity\actions\16\document-new.svg"
    delete "$INSTDIR\icons\humanity\actions\16\go-next.svg"
    delete "$INSTDIR\icons\humanity\actions\48\go-down.svg"
    delete "$INSTDIR\icons\humanity\actions\48\gtk-edit.svg"
    delete "$INSTDIR\icons\humanity\actions\48\edit-clear.svg"
    delete "$INSTDIR\icons\humanity\actions\48\gtk-execute.svg"
    delete "$INSTDIR\icons\humanity\actions\48\document-open-recent.svg"
    delete "$INSTDIR\icons\humanity\actions\48\go-previous.svg"
    delete "$INSTDIR\icons\humanity\actions\48\go-up.svg"
    delete "$INSTDIR\icons\humanity\actions\48\document-open.svg"
    delete "$INSTDIR\icons\humanity\actions\48\edit-find.png"
    delete "$INSTDIR\icons\humanity\actions\48\document-new.svg"
    delete "$INSTDIR\icons\humanity\actions\48\go-next.svg"
    delete "$INSTDIR\icons\humanity\actions\128\document-new.svg"
    delete "$INSTDIR\icons\humanity\actions\24\go-down.svg"
    delete "$INSTDIR\icons\humanity\actions\24\gtk-edit.svg"
    delete "$INSTDIR\icons\humanity\actions\24\edit-clear.svg"
    delete "$INSTDIR\icons\humanity\actions\24\gtk-execute.svg"
    delete "$INSTDIR\icons\humanity\actions\24\document-open-recent.svg"
    delete "$INSTDIR\icons\humanity\actions\24\go-previous.svg"
    delete "$INSTDIR\icons\humanity\actions\24\go-up.svg"
    delete "$INSTDIR\icons\humanity\actions\24\document-open.svg"
    delete "$INSTDIR\icons\humanity\actions\24\edit-find.png"
    delete "$INSTDIR\icons\humanity\actions\24\document-new.svg"
    delete "$INSTDIR\icons\humanity\actions\24\go-next.svg"
    delete "$INSTDIR\icons\humanity\actions\32\edit-find.png"
    delete "$INSTDIR\icons\humanity\actions\32\document-new.svg"
    delete "$INSTDIR\icons\humanity\actions\64\document-new.svg"
    delete "$INSTDIR\icons\humanity\actions\22\go-down.svg"
    delete "$INSTDIR\icons\humanity\actions\22\gtk-edit.svg"
    delete "$INSTDIR\icons\humanity\actions\22\document-open-recent.svg"
    delete "$INSTDIR\icons\humanity\actions\22\go-previous.svg"
    delete "$INSTDIR\icons\humanity\actions\22\go-up.svg"
    delete "$INSTDIR\icons\humanity\actions\22\edit-find.png"
    delete "$INSTDIR\icons\humanity\actions\22\document-new.svg"
    delete "$INSTDIR\icons\humanity\actions\22\go-next.svg"
    delete "$INSTDIR\icons\humanity\categories\48\preferences-other.svg"
    delete "$INSTDIR\icons\humanity\categories\24\preferences-other.svg"
    delete "$INSTDIR\icons\humanity\categories\64\preferences-other.svg"
    delete "$INSTDIR\icons\humanity\mimes\16\image-png.svg"
    delete "$INSTDIR\icons\humanity\mimes\16\audio-x-mp3-playlist.svg"
    delete "$INSTDIR\icons\humanity\mimes\16\audio-x-generic.svg"
    delete "$INSTDIR\icons\humanity\mimes\48\image-png.svg"
    delete "$INSTDIR\icons\humanity\mimes\48\audio-x-mp3-playlist.svg"
    delete "$INSTDIR\icons\humanity\mimes\48\audio-x-generic.svg"
    delete "$INSTDIR\icons\humanity\mimes\24\image-png.svg"
    delete "$INSTDIR\icons\humanity\mimes\24\audio-x-mp3-playlist.svg"
    delete "$INSTDIR\icons\humanity\mimes\24\audio-x-generic.svg"
    delete "$INSTDIR\icons\humanity\mimes\22\image-png.svg"
    delete "$INSTDIR\icons\humanity\mimes\22\audio-x-mp3-playlist.svg"
    delete "$INSTDIR\icons\humanity\mimes\22\audio-x-generic.svg"
    delete "$INSTDIR\icons\humanity\apps\16\preferences-desktop-keyboard.svg"
    delete "$INSTDIR\icons\humanity\apps\48\preferences-desktop-keyboard.svg"
    delete "$INSTDIR\icons\humanity\apps\24\preferences-desktop-keyboard.svg"
    delete "$INSTDIR\icons\humanity\apps\16\cantata.png"
    delete "$INSTDIR\icons\humanity\apps\22\cantata.png"
    delete "$INSTDIR\icons\humanity\apps\24\cantata.png"
    delete "$INSTDIR\icons\humanity\apps\32\cantata.png"
    delete "$INSTDIR\icons\humanity\apps\48\cantata.png"
    delete "$INSTDIR\icons\humanity\apps\64\cantata.png"
    delete "$INSTDIR\icons\humanity\apps\scalable\cantata.svg"
    delete "$INSTDIR\imageformats\qjpeg4.dll"
    delete "$INSTDIR\imageformats\qsvg4.dll"
    delete "$INSTDIR\Qt4 README.txt"
    delete "$INSTDIR\Qt License (LGPL V2).txt"
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
    delete "$INSTDIR\translations\cantata_cs.qm"
    delete "$INSTDIR\translations\cantata_de.qm"
    delete "$INSTDIR\translations\cantata_en_GB.qm"
    delete "$INSTDIR\translations\cantata_es.qm"
    delete "$INSTDIR\translations\cantata_fr.qm"
    delete "$INSTDIR\translations\cantata_hu.qm"
    delete "$INSTDIR\translations\cantata_ko.qm"
    delete "$INSTDIR\translations\cantata_pl.qm"
    delete "$INSTDIR\translations\cantata_ru.qm"
    delete "$INSTDIR\translations\cantata_zh_CN.qm"
    delete "$INSTDIR\translations\qt_ar.qm"
    delete "$INSTDIR\translations\qt_cs.qm"
    delete "$INSTDIR\translations\qt_da.qm"
    delete "$INSTDIR\translations\qt_de.qm"
    delete "$INSTDIR\translations\qt_es.qm"
    delete "$INSTDIR\translations\qt_fa.qm"
    delete "$INSTDIR\translations\qt_fr.qm"
    delete "$INSTDIR\translations\qt_gl.qm"
    delete "$INSTDIR\translations\qt_he.qm"
    delete "$INSTDIR\translations\qt_hu.qm"
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
    delete "$INSTDIR\zlib1.dll"
    delete "$INSTDIR\libz-1.dll"
    delete "$INSTDIR\libeay32.dll"
    delete "$INSTDIR\ssleay32.dll"
 
    rmDir $INSTDIR\config
    rmDir $INSTDIR\helpers
    rmDir $INSTDIR\iconengines
    rmDir $INSTDIR\sqldrivers
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
    rmDir $INSTDIR\icons\humanity\status\16
    rmDir $INSTDIR\icons\humanity\status\48
    rmDir $INSTDIR\icons\humanity\status\128
    rmDir $INSTDIR\icons\humanity\status\24
    rmDir $INSTDIR\icons\humanity\status\32
    rmDir $INSTDIR\icons\humanity\status\64
    rmDir $INSTDIR\icons\humanity\status\22
    rmDir $INSTDIR\icons\humanity\devices\16
    rmDir $INSTDIR\icons\humanity\devices\48
    rmDir $INSTDIR\icons\humanity\devices\128
    rmDir $INSTDIR\icons\humanity\devices\24
    rmDir $INSTDIR\icons\humanity\devices\32
    rmDir $INSTDIR\icons\humanity\devices\64
    rmDir $INSTDIR\icons\humanity\devices\22
    rmDir $INSTDIR\icons\humanity\places\16
    rmDir $INSTDIR\icons\humanity\places\48
    rmDir $INSTDIR\icons\humanity\places\128
    rmDir $INSTDIR\icons\humanity\places\24
    rmDir $INSTDIR\icons\humanity\places\32
    rmDir $INSTDIR\icons\humanity\places\64
    rmDir $INSTDIR\icons\humanity\places\22
    rmDir $INSTDIR\icons\humanity\actions\16
    rmDir $INSTDIR\icons\humanity\actions\48
    rmDir $INSTDIR\icons\humanity\actions\128
    rmDir $INSTDIR\icons\humanity\actions\24
    rmDir $INSTDIR\icons\humanity\actions\32
    rmDir $INSTDIR\icons\humanity\actions\64
    rmDir $INSTDIR\icons\humanity\actions\22
    rmDir $INSTDIR\icons\humanity\categories\48
    rmDir $INSTDIR\icons\humanity\categories\24
    rmDir $INSTDIR\icons\humanity\categories\64
    rmDir $INSTDIR\icons\humanity\mimes\16
    rmDir $INSTDIR\icons\humanity\mimes\48
    rmDir $INSTDIR\icons\humanity\mimes\24
    rmDir $INSTDIR\icons\humanity\mimes\22
    rmDir $INSTDIR\icons\humanity\apps\16
    rmDir $INSTDIR\icons\humanity\apps\48
    rmDir $INSTDIR\icons\humanity\apps\24
    rmDir $INSTDIR\icons\humanity\apps\22
    rmDir $INSTDIR\icons\humanity\apps\32
    rmDir $INSTDIR\icons\humanity\apps\64
    rmDir $INSTDIR\icons\humanity\apps\scalable
    rmDir $INSTDIR\icons\humanity\status
    rmDir $INSTDIR\icons\humanity\devices
    rmDir $INSTDIR\icons\humanity\places
    rmDir $INSTDIR\icons\humanity\actions
    rmDir $INSTDIR\icons\humanity\categories
    rmDir $INSTDIR\icons\humanity\mimes
    rmDir $INSTDIR\icons\humanity\apps
    rmDir $INSTDIR\icons\humanity
    rmDir $INSTDIR\icons
    rmDir $INSTDIR\imageformats
    rmDir $INSTDIR\translations

    # Always delete uninstaller as the last action
    delete $INSTDIR\uninstall.exe
 
    # Try to remove the install directory - this will only happen if it is empty
    rmDir $INSTDIR
 
    # Remove uninstaller information from the registry
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@WINDOWS_COMPANY_NAME@ @WINDOWS_APP_NAME@"
sectionEnd
