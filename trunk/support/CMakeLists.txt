SET( SUPPORT_SRCS
    action.cpp
    actioncollection.cpp
    fancytabwidget.cpp
    messagewidget.cpp
    icon.cpp
    gtkstyle.cpp
    utils.cpp
    spinner.cpp
)

SET( SUPPORT_MOC_HDRS
    action.h
    actioncollection.h
    fancytabwidget.h
    messagewidget.h
)

if (NOT WIN32)
    SET( SUPPORT_SRCS ${SUPPORT_SRCS} onoffbutton.cpp spinbox.cpp windowmanager.cpp)
    SET( SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} spinbox.h windowmanager.h)
endif (NOT WIN32)

if (ENABLE_KDE_SUPPORT )
    macro_ensure_version("4.7.0" ${KDE_VERSION} KDE_VERSION_47)
    if (NOT KDE_VERSION_47)
        SET( SUPPORT_SRCS ${SUPPORT_SRCS} kmessagewidget.cpp )
        SET( SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} kmessagewidget.h )
    endif (NOT KDE_VERSION_47)
    KDE4_ADD_UI_FILES( SUPPORT_UI_HDRS ${SUPPORT_UIS} )
else (ENABLE_KDE_SUPPORT)
    SET( SUPPORT_SRCS ${SUPPORT_SRCS} pathrequester.cpp lineedit.cpp kmessagewidget.cpp
        dialog.cpp messagebox.cpp pagewidget.cpp shortcutsmodel.cpp shortcutssettingspage.cpp
        keysequencewidget.cpp )
    SET( SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} pathrequester.h lineedit.h kmessagewidget.h urllabel.h
         dialog.h shortcutsmodel.h shortcutssettingspage.h keysequencewidget.h spinner.h )
    SET( SUPPORT_UIS shortcutssettingspage.ui )
    if (ENABLE_QT5)
        QT5_WRAP_UI( SUPPORT_UI_HDRS ${SUPPORT_UIS} )
    else (ENABLE_QT5)
        QT4_WRAP_UI( SUPPORT_UI_HDRS ${SUPPORT_UIS} )
    endif (ENABLE_QT5)
endif  (ENABLE_KDE_SUPPORT)

if (ENABLE_QT5)
    QT5_WRAP_CPP( SUPPORT_MOC_SRCS ${SUPPORT_MOC_HDRS} )
else (ENABLE_QT5)
    QT4_WRAP_CPP( SUPPORT_MOC_SRCS ${SUPPORT_MOC_HDRS} )
endif (ENABLE_QT5)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${QTINCLUDES} )
add_library( support STATIC ${SUPPORT_MOC_SRCS} ${SUPPORT_SRCS} ${SUPPORT_UI_HDRS} )
