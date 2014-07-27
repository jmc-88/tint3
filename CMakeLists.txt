project( tint3 )
cmake_minimum_required( VERSION 2.8 )

set(CMAKE_C_FLAGS_DEBUG   "-O0 -ggdb")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb")

include( FindPkgConfig )
include( CheckLibraryExists )
pkg_check_modules( X11 REQUIRED x11 xcomposite xdamage xinerama xrender xrandr>=1.3 )
pkg_check_modules( PANGOCAIRO REQUIRED pangocairo )
pkg_check_modules( PANGO REQUIRED pango )
pkg_check_modules( CAIRO REQUIRED cairo )
pkg_check_modules( GLIB2 REQUIRED glib-2.0 )
pkg_check_modules( GOBJECT2 REQUIRED gobject-2.0 )
pkg_check_modules( IMLIB2 REQUIRED imlib2>=1.4.2 )
pkg_check_modules( SN libstartup-notification-1.0>=0.12 )
find_library( RT_LIBRARY rt )

if( NOT X11_FOUND OR NOT PANGOCAIRO_FOUND OR NOT PANGO_FOUND OR NOT CAIRO_FOUND OR NOT GLIB2_FOUND OR NOT GOBJECT2_FOUND OR NOT IMLIB2_FOUND )
    message( FATAL_ERROR "Not all dependencies are satisfied. See README.source." )
endif()

string( REPLACE ";" " " FLAGS_REPLACED "${IMLIB2_LDFLAGS}" )
set( CMAKE_REQUIRED_FLAGS "${FLAGS_REPLACED}" )
check_library_exists( "${IMLIB2_LIBRARIES}" "imlib_context_set_display" "${IMLIB2_LIBRARY_DIRS}" IMLIB_BUILD_WITH_X )
if( NOT IMLIB_BUILD_WITH_X )
    message( FATAL_ERROR "Imlib is not build with x support" )
endif()

include_directories( ${PROJECT_BINARY_DIR}
    src
    src/battery
    src/clock
    src/systray
    src/taskbar
    src/launcher
    src/tooltip
    src/util
    ${X11_INCLUDE_DIRS}
    ${PANGOCAIRO_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
    ${GOBJECT2_INCLUDE_DIRS}
    ${IMLIB2_INCLUDE_DIRS}
    ${SN_INCLUDE_DIRS} )

set( SOURCES
    src/config.cc
    src/panel.cc
    src/server.cc
    src/tint.cc
    src/clock/clock.cc
    src/systray/systraybar.cc
    src/launcher/launcher.cc
    src/launcher/xsettings-client.cc
    src/launcher/xsettings-common.cc
    src/taskbar/task.cc
    src/taskbar/taskbar.cc
    src/taskbar/taskbarname.cc
    src/tooltip/tooltip.cc
    src/util/area.cc
    src/util/common.cc
    src/util/fs.cc
    src/util/timer.cc
    src/util/window.cc
    src/util/xdg.cc )

option( ENABLE_BATTERY "Enable battery status plugin" ON )
option( ENABLE_EXAMPLES "Install additional tin2rc examples" OFF )
option( ENABLE_SN "Startup notification support" ON )
if( ENABLE_SN )
    if( SN_FOUND )
        add_definitions( -DHAVE_SN -DSN_API_NOT_YET_FROZEN )
    endif()
endif()

if( ENABLE_BATTERY )
    set( SOURCES ${SOURCES} src/battery/battery.cc )
    add_definitions( -DENABLE_BATTERY )
endif()

set( MANDIR share/man CACHE PATH "Directory for man pages" )
set( DATADIR share CACHE PATH "Directory for shared data" )
set( SYSCONFDIR /etc CACHE PATH "Directory for configuration files" )
set( DOCDIR share/doc/tint3 CACHE PATH "Directory for documentation files" )

add_custom_target( version ALL "${PROJECT_SOURCE_DIR}/get_svnrev.sh" "\"${PROJECT_SOURCE_DIR}\"" )

link_directories( ${X11_LIBRARY_DIRS}
    ${PANGOCAIRO_LIBRARY_DIRS}
    ${PANGO_LIBRARY_DIRS}
    ${CAIRO_LIBRARY_DIRS}
    ${GLIB2_LIBRARY_DIRS}
    ${GOBJECT2_LIBRARY_DIRS}
    ${IMLIB2_LIBRARY_DIRS}
    ${SN_LIBRARY_DIRS} )
add_executable(tint3 ${SOURCES})
target_link_libraries( tint3 ${X11_LIBRARIES}
    ${PANGOCAIRO_LIBRARIES}
    ${PANGO_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${IMLIB2_LIBRARIES}
    ${SN_LIBRARIES} )
if( RT_LIBRARY )
    target_link_libraries( tint3 ${RT_LIBRARY} )
endif()

add_dependencies( tint3 version )
set_target_properties(tint3 PROPERTIES COMPILE_FLAGS "-Wall -pthread -std=c++11")
set_target_properties(tint3 PROPERTIES LINK_FLAGS "-pthread")

install( TARGETS tint3 DESTINATION bin )
install( FILES sample/tint3rc DESTINATION ${SYSCONFDIR}/xdg/tint3 )
install( FILES default_icon.png DESTINATION ${DATADIR}/tint3 )
install( FILES AUTHORS README.md DESTINATION ${DOCDIR} )
install( FILES doc/tint3.1 DESTINATION ${MANDIR}/man1 )
if( ENABLE_EXAMPLES )
    file( GLOB SAMPLEFILES sample/*.tint3rc )
    install( FILES ${SAMPLEFILES} DESTINATION ${DOCDIR}/examples )
endif()
