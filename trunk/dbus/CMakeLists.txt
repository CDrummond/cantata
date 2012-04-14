set(tasks_SRCS
    windowtaskitem.cpp tasks.cpp taskitemlayout.cpp abstracttaskitem.cpp taskgroupitem.cpp applauncheritem.cpp
    jobmanager.cpp dockmanager.cpp dockitem.cpp dockhelper.cpp dockconfig.cpp mediabuttons.cpp unity.cpp unityitem.cpp
    dbusstatus.cpp recentdocuments.cpp
    tooltips/tooltipcontent.cpp tooltips/tooltip.cpp tooltips/tooltipmanager.cpp tooltips/windowpreview.cpp)

# Generate DBUS XML files, would like to use
# qt4_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/dockmanager.h net.launchpad.DockManager.xml)
# qt4_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/dockitem.h net.launchpad.DockItem.xml)
# ...but this does not work, as we also depend on the xml files in qt4_add_dbus_adaptor :-(
# So, need to manually create via:
#
# qdbuscpp2xml -M -P -S dockmanager.h -o net.launchpad.DockManager.xml
# qdbuscpp2xml -M -P -S dockitem.h -o net.launchpad.DockItem.xml

qt4_add_dbus_adaptor(tasks_SRCS net.launchpad.DockManager.xml dockmanager.h DockManager)
qt4_add_dbus_adaptor(tasks_SRCS net.launchpad.DockItem.xml dockitem.h DockItem)

qt4_add_dbus_interface(tasks_SRCS org.mpris.MediaPlayer2.Player.xml playerv2interface)
qt4_add_dbus_interface(tasks_SRCS org.mpris.MediaPlayer2.root.xml rootv2interface)

MACRO(QT4_ADD_DBUS_INTERFACE_EX _sources _interface _basename _extrainclude)
#   GET_FILENAME_COMPONENT(_infile ${_interface} ABSOLUTE)
  set(_infile ${CMAKE_CURRENT_SOURCE_DIR}/${_interface})

  SET(_header ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.h)
  SET(_impl   ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.cpp)
  SET(_moc    ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.moc)

  # handling more arguments (as in FindQt4.cmake from KDE4) will come soon, then
  # _params will be used for more than just -m
  SET(_params -m)

  ADD_CUSTOM_COMMAND(OUTPUT ${_impl} ${_header}
      COMMAND ${QT_DBUSXML2CPP_EXECUTABLE} ${_params} -i ${_extrainclude}  -p ${_basename} ${_infile}
      DEPENDS ${_infile})

  SET_SOURCE_FILES_PROPERTIES(${_impl} PROPERTIES SKIP_AUTOMOC TRUE)

  QT4_GENERATE_MOC(${_header} ${_moc})

  SET(${_sources} ${${_sources}} ${_impl} ${_header} ${_moc})
  MACRO_ADD_FILE_DEPENDENCIES(${_impl} ${_moc})

ENDMACRO(QT4_ADD_DBUS_INTERFACE_EX)

qt4_add_dbus_interface_ex(tasks_SRCS org.freedesktop.MediaPlayer.player.xml playerv1interface dbusstatus.h)

kde4_add_ui_files(tasks_SRCS appearanceconfig.ui behaviourconfig.ui dockconfig.ui)
kde4_add_plugin(plasma_applet_icontasks ${tasks_SRCS})

target_link_libraries(plasma_applet_icontasks ${KDE4_KDEUI_LIBS} ${KDE4_PLASMA_LIBS} ${KDE4_KIO_LIBS} ${DBUSMENUQT_LIBRARIES})

if(USE_KDE_TASKMANAGER)
    target_link_libraries(plasma_applet_icontasks taskmanager)
else(USE_KDE_TASKMANAGER)
    target_link_libraries(plasma_applet_icontasks icontaskmanager)
endif(USE_KDE_TASKMANAGER)

if(Q_WS_X11)
    target_link_libraries(plasma_applet_icontasks ${X11_LIBRARIES})
endif(Q_WS_X11)

install(TARGETS plasma_applet_icontasks DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-icontasks.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES badge.svgz launcherseparator.svgz progress.svgz indicators.svgz dropindicators.svgz DESTINATION ${DATA_INSTALL_DIR}/desktoptheme/default/icontasks/)
install(FILES mediabuttonsrc DESTINATION ${DATA_INSTALL_DIR}/${CMAKE_PROJECT_NAME})
