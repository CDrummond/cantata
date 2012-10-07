SET( SUPPORT_SRCS
    action.cpp
    actioncollection.cpp
    fancytabwidget.cpp
    messagewidget.cpp
    icon.cpp
    gtkstyle.cpp
)

SET( SUPPORT_MOC_HDRS
    action.h
    actioncollection.h
    fancytabwidget.h
    messagewidget.h
)

if (NOT WIN32)
    SET( SUPPORT_SRCS ${SUPPORT_SRCS} onoffbutton.cpp spinbox.cpp)
    SET( SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} spinbox.h)
endif (NOT WIN32)

if (ENABLE_KDE_SUPPORT )
    macro_ensure_version("4.7.0" ${KDE_VERSION} KDE_VERSION_47)
    if (NOT KDE_VERSION_47)
        SET( SUPPORT_SRCS ${SUPPORT_SRCS} kmessagewidget.cpp )
        SET( SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} kmessagewidget.h )
    endif (NOT KDE_VERSION_47)
else (ENABLE_KDE_SUPPORT)
    SET( SUPPORT_SRCS ${SUPPORT_SRCS} dirrequester.cpp lineedit.cpp kmessagewidget.cpp
        dialog.cpp messagebox.cpp pagewidget.cpp shortcutsmodel.cpp shortcutssettingspage.cpp
        keysequencewidget.cpp )
    SET( SUPPORT_MOC_HDRS ${SUPPORT_MOC_HDRS} dirrequester.h lineedit.h kmessagewidget.h urllabel.h
         dialog.h shortcutsmodel.h shortcutssettingspage.h keysequencewidget.h )
    SET( SUPPORT_UIS shortcutssettingspage.ui )
endif  (ENABLE_KDE_SUPPORT)

QT4_WRAP_CPP( SUPPORT_MOC_SRCS ${SUPPORT_MOC_HDRS} )

include_directories( ${QT_INCLUDES})
add_library( support STATIC ${SUPPORT_MOC_SRCS} ${SUPPORT_SRCS} )
