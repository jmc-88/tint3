cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(tint3)

# Additional CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Custom compiler flags
set(CMAKE_C_FLAGS_DEBUG    "${CMAKE_C_FLAGS_DEBUG} -O0 -D_TINT3_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} -O0 -D_TINT3_DEBUG")
set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -Wall -pthread")
set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS} -Wall -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

# Require C++11 support
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link to libc++ whenever possible, please.
include(HasLibCxx)
if(HAS_LIBCXX)
  set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -stdlib=libc++")
endif()

include(FindPkgConfig)
include(CheckLibraryExists)
pkg_check_modules(X11 REQUIRED x11 xcomposite xdamage xfixes xinerama xrender xrandr>=1.3)
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

option(COVERALLS "Turn on coveralls support" OFF)
option(COVERALLS_UPLOAD "Upload the generated coveralls json" OFF)

if (COVERALLS)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
                        "${CMAKE_SOURCE_DIR}/cmake/coveralls-cmake/cmake/")
	include(Coveralls)
endif()

# {{{ configuration files
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
  ${CMAKE_SOURCE_DIR}/src/version.hh.in
  ${CMAKE_BINARY_DIR}/generated/version.hh)

include(CheckCXXSourceRuns)
set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_CXX11_STANDARD_COMPILE_OPTION})
check_cxx_source_runs(
  "#include <cmath>
  int main() {
    std::nearbyint(0.5);
  }"
  TINT3_HAVE_STD_NEARBYINT)
check_cxx_source_runs(
  "#include <cmath>
  int main() {
    std::round(0.5);
  }"
  TINT3_HAVE_STD_ROUND)

configure_file(
  ${CMAKE_SOURCE_DIR}/src/cxx_features.hh.in
  ${CMAKE_BINARY_DIR}/generated/cxx_features.hh)

include(CheckSymbolExists)
check_symbol_exists(shm_open "sys/mman.h" TINT3_HAVE_SHM_OPEN)

configure_file(
  ${CMAKE_SOURCE_DIR}/src/unix_features.hh.in
  ${CMAKE_BINARY_DIR}/generated/unix_features.hh)

include_directories(${CMAKE_BINARY_DIR}/generated)

# }}} configuration files

# {{{ common rules

include_directories(${PROJECT_BINARY_DIR}
                    src
                    src/battery
                    src/clock
                    src/dnd
                    src/parser
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
    src/version.hh.in
    src/config.cc
    src/panel.cc
    src/server.cc
    src/clock/clock.cc
    src/clock/time_utils.cc
    src/dnd/dnd.cc
    src/parser/lexer.cc
    src/parser/parser.cc
    src/systray/systraybar.cc
    src/systray/tray_window.cc
    src/launcher/desktop_entry.cc
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
    src/util/color.cc
    src/util/environment.cc
    src/util/geometry.cc
    src/util/gradient.cc
    src/util/fs.cc
    src/util/imlib2.cc
    src/util/log.cc
    src/util/pipe.cc
    src/util/timer.cc
    src/util/window.cc
    src/util/x11.cc
    src/util/xdg.cc)

if(ENABLE_BATTERY)
    set(TINT3_SOURCES
        ${TINT3_SOURCES}
        src/battery/battery.cc)

    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
      set(TINT3_SOURCES ${TINT3_SOURCES} src/battery/linux_sysfs.cc)
    endif()

    if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
      set(TINT3_SOURCES ${TINT3_SOURCES} src/battery/freebsd_acpiio.cc)
    endif()

    add_definitions(-DENABLE_BATTERY)
endif()

add_library(tint3_common_lib STATIC ${TINT3_SOURCES})

# }}} common rules

# {{{ doc rules

include(Pandoc)
add_pandoc_man_page(
  tint3_1
  INPUT doc/tint3.1.md
  OUTPUT doc/tint3.1
  DESTINATION "${MANDIR}/man1")
add_pandoc_man_page(
  tint3rc_5
  INPUT doc/tint3rc.5.md
  OUTPUT doc/tint3rc.5
  DESTINATION "${MANDIR}/man5")

# }}} doc rules

# {{{ build rules

add_executable(tint3 src/tint.cc)
target_link_libraries(tint3 tint3_common_lib ${TINT3_LINK_LIBRARIES})

# }}} build rules

# {{{ coverage rules

if(COVERALLS)
  set(COVERAGE_SRCS ${TINT3_SOURCES} src/tint.cc)
  coveralls_turn_on_coverage()
  coveralls_setup("${COVERAGE_SRCS}" ${COVERALLS_UPLOAD})
endif()

# }}} coverage rules

# {{{ test rules

include(TestTarget)
enable_testing()
include_directories(test)

# List of testdata files to copy
set(TESTDATA_SRCS
    src/launcher/testdata/applications/launcher_test.desktop
    src/util/testdata/fs_test.txt)

# Generated list of output paths in the build directory
string(REGEX REPLACE "([^;]+)" "${CMAKE_BINARY_DIR}/\\1" TESTDATA_OUTS "${TESTDATA_SRCS}")

foreach(item IN LISTS TESTDATA_SRCS)
  add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/${item}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/${item}" "${CMAKE_BINARY_DIR}/${item}"
    DEPENDS "${CMAKE_SOURCE_DIR}/${item}")
endforeach()

add_custom_target(testdata ALL DEPENDS ${TESTDATA_OUTS})

# Main entry point for tests
add_library(testmain STATIC test/main.cc)

test_target(
  config_test
  SOURCES src/config_test.cc src/util/timer_test_utils.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  clock_time_utils_test
  SOURCES src/clock/time_utils_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  dnd_dnd_test
  SOURCES src/dnd/dnd_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  launcher_test
  SOURCES src/launcher/launcher_test.cc
  DEPENDS testdata
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  launcher_desktop_entry_test
  SOURCES src/launcher/desktop_entry_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  parser_lexer_test
  SOURCES src/parser/lexer_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  parser_parser_test
  SOURCES src/parser/parser_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  tooltip_tooltip_test
  SOURCES src/tooltip/tooltip_test.cc src/util/timer_test_utils.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES}
  USE_XVFB_RUN)

test_target(
  util_area_test
  SOURCES src/util/area_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_bimap_test
  SOURCES src/util/bimap_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_color_test
  SOURCES src/util/color_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_common_test
  SOURCES src/util/common_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_geometry_test
  SOURCES src/util/geometry_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_gradient_test
  SOURCES src/util/gradient_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_fs_test
  SOURCES src/util/fs_test.cc
  DEPENDS testdata
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_imlib2_test
  SOURCES src/util/imlib2_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_variant_test
  SOURCES src/util/variant_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_pipe_test
  SOURCES src/util/pipe_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_environment_test
  SOURCES src/util/environment_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_timer_test
  SOURCES src/util/timer_test.cc src/util/timer_test_utils.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

test_target(
  util_xdg_test
  SOURCES src/util/xdg_test.cc
  LINK_LIBRARIES tint3_common_lib testmain ${TINT3_LINK_LIBRARIES})

# }}} test rules

# {{{ install rules

install(TARGETS tint3 DESTINATION bin)
install(FILES sample/tint3rc DESTINATION ${SYSCONFDIR}/xdg/tint3)
install(FILES AUTHORS.md README.md DESTINATION ${DOCDIR})

install(FILES data/tint3.desktop DESTINATION ${DATADIR}/applications/tint3)
install(FILES data/default_icon.png DESTINATION ${DATADIR}/tint3)

install(FILES data/logo/tint3.svg
        DESTINATION ${DATADIR}/icons/hicolor/scalable/apps)
foreach(size 16 32 48 64 128 256)
  install(FILES data/logo/tint3-${size}.png
          DESTINATION ${DATADIR}/icons/hicolor/${size}x${size}/apps
          RENAME tint3.png)
endforeach()

if(ENABLE_EXAMPLES)
    file(GLOB SAMPLEFILES sample/*.tint3rc)
    install(FILES ${SAMPLEFILES} DESTINATION ${DOCDIR}/examples)
endif()

# }}} install rules
