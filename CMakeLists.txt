project(tint3)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_C_FLAGS_DEBUG    "${CMAKE_C_FLAGS_DEBUG} -O0 -D_TINT3_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} -O0 -D_TINT3_DEBUG")
set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -Wall -pthread")
set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS} -Wall -pthread -std=c++11")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

include(FindPkgConfig)
include(CheckLibraryExists)
pkg_check_modules(X11 REQUIRED x11 xcomposite xdamage xinerama xrender xrandr>=1.3)
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)
pkg_check_modules(PANGO REQUIRED pango)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GOBJECT2 REQUIRED gobject-2.0)
pkg_check_modules(IMLIB2 REQUIRED imlib2>=1.4.2)
find_library(RT_LIBRARY rt)

if(NOT X11_FOUND OR
   NOT PANGOCAIRO_FOUND OR
   NOT PANGO_FOUND OR
   NOT CAIRO_FOUND OR
   NOT GLIB2_FOUND OR
   NOT GOBJECT2_FOUND OR
   NOT IMLIB2_FOUND)
    message(FATAL_ERROR "Not all dependencies are satisfied. See README.source.")
endif()

string(REPLACE ";" " " FLAGS_REPLACED "${IMLIB2_LDFLAGS}")
set(CMAKE_REQUIRED_FLAGS "${FLAGS_REPLACED}")
check_library_exists("${IMLIB2_LIBRARIES}" "imlib_context_set_display" "${IMLIB2_LIBRARY_DIRS}" IMLIB_BUILT_WITH_X)
if(NOT IMLIB_BUILT_WITH_X)
    message(FATAL_ERROR "Imlib is not built with X11 support")
endif()

option(ENABLE_BATTERY "Enable battery status plugin" ON)
option(ENABLE_EXAMPLES "Install additional tint2rc examples" OFF)
option(ENABLE_SN "Startup notification support" ON)
if(ENABLE_SN)
    pkg_check_modules(SN REQUIRED libstartup-notification-1.0>=0.12)
    add_definitions(-DHAVE_SN -DSN_API_NOT_YET_FROZEN)
endif()

set(MANDIR share/man CACHE PATH "Directory for man pages")
set(DATADIR share CACHE PATH "Directory for shared data")
set(SYSCONFDIR /etc CACHE PATH "Directory for configuration files")
set(DOCDIR share/doc/tint3 CACHE PATH "Directory for documentation files")

# {{{ version.h
#
# This section inspired by:
#       http://xit0.org/2013/04/cmake-use-git-branch-and-commit-details-in-project/

# Get the current working branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(
  ${CMAKE_SOURCE_DIR}/src/version.h.in
  ${CMAKE_BINARY_DIR}/generated/version.h)

include_directories(${CMAKE_BINARY_DIR}/generated)

# }}} version.h

# {{{ common rules

include_directories(${PROJECT_BINARY_DIR}
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
                    ${SN_INCLUDE_DIRS})

link_directories(${X11_LIBRARY_DIRS}
                 ${PANGOCAIRO_LIBRARY_DIRS}
                 ${PANGO_LIBRARY_DIRS}
                 ${CAIRO_LIBRARY_DIRS}
                 ${GLIB2_LIBRARY_DIRS}
                 ${GOBJECT2_LIBRARY_DIRS}
                 ${IMLIB2_LIBRARY_DIRS}
                 ${SN_LIBRARY_DIRS})

set(TINT3_LINK_LIBRARIES
    ${X11_LIBRARIES}
    ${PANGOCAIRO_LIBRARIES}
    ${PANGO_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${IMLIB2_LIBRARIES}
    ${SN_LIBRARIES})

if(RT_LIBRARY)
    set(TINT3_LINK_LIBRARIES ${TINT3_LINK_LIBRARIES} ${RT_LIBRARY})
endif()

# This is purposely excluding src/tint3.cc as these sources are shared with
# test case definitions, which redefine their own main() entry point.
set(TINT3_SOURCES
    src/version.h.in
    src/config.cc
    src/panel.cc
    src/server.cc
    src/clock/clock.cc
    src/systray/systraybar.cc
    src/launcher/launcher.cc
    src/launcher/xsettings-client.cc
    src/launcher/xsettings-common.cc
    src/taskbar/task.cc
    src/taskbar/taskbar.cc
    src/taskbar/taskbarbase.cc
    src/taskbar/taskbarname.cc
    src/tooltip/tooltip.cc
    src/util/area.cc
    src/util/common.cc
    src/util/fs.cc
    src/util/log.cc
    src/util/timer.cc
    src/util/window.cc
    src/util/x11.cc
    src/util/xdg.cc)

if(ENABLE_BATTERY)
    set(TINT3_SOURCES ${TINT3_SOURCES} src/battery/battery.cc)
    add_definitions(-DENABLE_BATTERY)
endif()

add_library(tint3_common_lib STATIC ${TINT3_SOURCES})

# }}} common rules

# {{{ build rules

add_executable(tint3 src/tint.cc)
target_link_libraries(tint3 tint3_common_lib ${TINT3_LINK_LIBRARIES})

# }}} build rules

# {{{ test rules

enable_testing()
include_directories(test)

add_executable(config_test src/config_test.cc)
target_link_libraries(config_test tint3_common_lib ${TINT3_LINK_LIBRARIES})
add_test(NAME config_test COMMAND config_test)

add_executable(util_common_test src/util/common_test.cc)
target_link_libraries(util_common_test tint3_common_lib ${TINT3_LINK_LIBRARIES})
add_test(NAME util_common_test COMMAND util_common_test)

# }}} test rules

# {{{ install rules

install(TARGETS tint3 DESTINATION bin)
install(FILES sample/tint3rc DESTINATION ${SYSCONFDIR}/xdg/tint3)
install(FILES default_icon.png DESTINATION ${DATADIR}/tint3)
install(FILES AUTHORS README.md DESTINATION ${DOCDIR})
install(FILES doc/tint3.1 DESTINATION ${MANDIR}/man1)
if(ENABLE_EXAMPLES)
    file(GLOB SAMPLEFILES sample/*.tint3rc)
    install(FILES ${SAMPLEFILES} DESTINATION ${DOCDIR}/examples)
endif()

# }}} install rules