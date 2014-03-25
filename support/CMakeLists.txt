set(SUPPORT_SRCS action.cpp actioncollection.cpp fancytabwidget.cpp messagewidget.cpp icon.cpp buddylabel.cpp
    lineedit.cpp gtkstyle.cpp utils.cpp spinner.cpp messagebox.cpp inputdialog.cpp thread.cpp squeezedtextlabel.cpp
    configuration.cpp)

set(SUPPORT_MOC_HDRS action.h actioncollection.h fancytabwidget.h messagewidget.h inputdialog.h thread.h pagewidget.h)

if (NOT WIN32)
    set(SUPPORT_SRCS ${SUPPORT_SRCS} windowmanager.cpp gtkproxystyle.cpp combobox.cpp shortcuthandler.cpp)
    set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} windowmanager.h gtkproxystyle.h shortcuthandler.h)
    add_subdirectory(themes)
endif (NOT WIN32)

if (ENABLE_KDE_SUPPORT)
    include_directories(${KDE4_INCLUDES})
    # macro_ensure_version("4.7.0" ${KDE_VERSION} KDE_VERSION_47)
    # if (NOT KDE_VERSION_47)
        set(SUPPORT_SRCS ${SUPPORT_SRCS} kmessagewidget.cpp)
        set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} kmessagewidget.h)
    # endif (NOT KDE_VERSION_47)
    kde4_add_ui_files(SUPPORT_UI_HDRS ${SUPPORT_UIS})
    set(SUPPORT_SRCS ${SUPPORT_SRCS} dialog.cpp)
else (ENABLE_KDE_SUPPORT)
    set(SUPPORT_SRCS ${SUPPORT_SRCS} pathrequester.cpp kmessagewidget.cpp dialog.cpp pagewidget.cpp shortcutsmodel.cpp
        shortcutssettingswidget.cpp keysequencewidget.cpp acceleratormanager.cpp proxystyle.cpp urllabel.cpp)
    set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} pathrequester.h kmessagewidget.h urllabel.h
         dialog.h shortcutsmodel.h shortcutssettingswidget.h keysequencewidget.h spinner.h acceleratormanager_private.h)
    set(SUPPORT_UIS shortcutssettingswidget.ui)
    if (ENABLE_QT5)
        if (Qt5Gui_VERSION_MAJOR LESS 5 OR Qt5Gui_VERSION_MINOR LESS 2)
            set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} lineedit.h)
        endif (Qt5Gui_VERSION_MAJOR LESS 5 OR Qt5Gui_VERSION_MINOR LESS 2)
        qt5_wrap_ui(SUPPORT_UI_HDRS ${SUPPORT_UIS})
    else (ENABLE_QT5)
        set(SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} lineedit.h)
        qt4_wrap_ui(SUPPORT_UI_HDRS ${SUPPORT_UIS})
    endif (ENABLE_QT5)
endif  (ENABLE_KDE_SUPPORT)

if (ENABLE_QT5)
    qt5_wrap_cpp(SUPPORT_MOC_SRCS ${SUPPORT_MOC_HDRS})
else (ENABLE_QT5)
    qt4_wrap_cpp(SUPPORT_MOC_SRCS ${SUPPORT_MOC_HDRS})
endif (ENABLE_QT5)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${QTINCLUDES})
add_library(support STATIC ${SUPPORT_MOC_SRCS} ${SUPPORT_SRCS} ${SUPPORT_UI_HDRS})

if (X11_FOUND)
    target_link_libraries(support ${X11_LIBRARIES})
endif (X11_FOUND)
