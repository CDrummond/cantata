set(SUPPORT_CORE_SRCS utils.cpp thread.cpp)
set(SUPPORT_CORE_MOC_HDRS thread.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QTINCLUDES})

qt5_wrap_cpp(SUPPORT_CORE_MOC_SRCS ${SUPPORT_CORE_MOC_HDRS})

add_library(support-core STATIC ${SUPPORT_CORE_MOC_SRCS} ${SUPPORT_CORE_SRCS})
    
set (SUPPORT_SRCS icon.cpp fancytabwidget.cpp messagewidget.cpp buddylabel.cpp action.cpp actioncollection.cpp lineedit.cpp
     configuration.cpp gtkstyle.cpp spinner.cpp messagebox.cpp inputdialog.cpp thread.cpp squeezedtextlabel.cpp proxystyle.cpp
     touchproxystyle.cpp pagewidget.cpp combobox.cpp configdialog.cpp monoicon.cpp)

set(SUPPORT_MOC_HDRS fancytabwidget.h messagewidget.h inputdialog.h pagewidget.h action.h actioncollection.h configdialog.h)
if (APPLE)
    install(FILES fontawesome-webfont.ttf DESTINATION ${MACOSX_BUNDLE_RESOURCES}/fonts)
elseif (WIN32)
    install(FILES fontawesome-webfont.ttf DESTINATION ${CMAKE_INSTALL_PREFIX}/fonts)
else ()
    install(FILES fontawesome-webfont.ttf DESTINATION ${SHARE_INSTALL_PREFIX}/${CMAKE_PROJECT_NAME}/fonts)
endif ()

if (ENABLE_TOUCH_SUPPORT)
    set(SUPPORT_SRCS ${SUPPORT_SRCS} flickcharm.cpp)
    set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} flickcharm.h)
endif ()

set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} combobox.h)

if (NOT WIN32 AND NOT APPLE)
    set(SUPPORT_SRCS ${SUPPORT_SRCS} gtkproxystyle.cpp shortcuthandler.cpp)
    set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} shortcuthandler.h)
endif ()
if (APPLE)
    set(SUPPORT_SRCS ${SUPPORT_SRCS} osxstyle.cpp flattoolbutton.cpp windowmanager.cpp)
    set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} osxstyle.h windowmanager.h)
    qt5_add_resources(SUPPORT_RC_SRCS support_mac_icons.qrc)
endif ()

set(SUPPORT_SRCS ${SUPPORT_SRCS} pathrequester.cpp kmessagewidget.cpp dialog.cpp shortcutsmodel.cpp
    shortcutssettingswidget.cpp keysequencewidget.cpp acceleratormanager.cpp urllabel.cpp)
set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} pathrequester.h kmessagewidget.h urllabel.h
     dialog.h shortcutsmodel.h shortcutssettingswidget.h keysequencewidget.h spinner.h acceleratormanager_private.h)
set(SUPPORT_UIS shortcutssettingswidget.ui)
 if (Qt5Gui_VERSION_MAJOR LESS 5 OR Qt5Gui_VERSION_MINOR LESS 2)
    set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} lineedit.h)
 endif ()

qt5_wrap_ui(SUPPORT_UI_HDRS ${SUPPORT_UIS})
qt5_wrap_cpp(SUPPORT_MOC_SRCS ${SUPPORT_MOC_HDRS})
qt5_add_resources(SUPPORT_RC_SRCS ${SUPPORT_RCS})

add_library(support STATIC ${SUPPORT_MOC_SRCS} ${SUPPORT_SRCS} ${SUPPORT_UI_HDRS} ${SUPPORT_RC_SRCS})
