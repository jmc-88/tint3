cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(tint3)

# Workaround for: https://gitlab.kitware.com/cmake/cmake/issues/16517
if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
  set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "tint3")
endif()

# Additional CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Custom compiler flags
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -O0 -D_TINT3_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -Wall -pthread")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pthread")

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
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)
pkg_check_modules(PANGO REQUIRED pango)
pkg_check_modules(CAIRO REQUIRED cairo cairo-xlib)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GOBJECT2 REQUIRED gobject-2.0)
pkg_check_modules(IMLIB2 REQUIRED imlib2>=1.4.2)

find_package(
  X11 REQUIRED
  COMPONENTS
    X11 Xcomposite Xdamage Xfixes Xinerama Xrender Xrandr>=1.3)

string(REPLACE ";" " " FLAGS_REPLACED "${IMLIB2_LDFLAGS}")
set(CMAKE_REQUIRED_FLAGS "${FLAGS_REPLACED}")
check_library_exists("${IMLIB2_LIBRARIES}" "imlib_context_set_display" "${IMLIB2_LIBRARY_DIRS}" IMLIB_BUILT_WITH_X)
if(NOT IMLIB_BUILT_WITH_X)
  message(FATAL_ERROR "Imlib is not built with X11 support")
endif()

option(ENABLE_BATTERY "Enable battery status plugin" ON)
if(ENABLE_BATTERY)
  add_definitions(-DENABLE_BATTERY)
endif()

option(ENABLE_EXAMPLES "Install additional tint2rc examples" OFF)

option(ENABLE_SN "Startup notification support" ON)
if(ENABLE_SN)
  pkg_check_modules(SN REQUIRED libstartup-notification-1.0>=0.12)
  add_definitions(-DHAVE_SN -DSN_API_NOT_YET_FROZEN)
endif()

option(ENABLE_CURL "Enable CURL for fetching remote resources" ON)
if(ENABLE_CURL)
  find_package(CURL 7.30 REQUIRED)
  add_definitions(-DHAVE_CURL)
endif()

# Prefer GNU gold if available and requested.
option(ENABLE_GNU_GOLD "Enable the GNU gold linker if available" ON)
if(ENABLE_GNU_GOLD)
  execute_process(
    COMMAND
      ${CMAKE_CXX_COMPILER} -fuse-ld=gold -Wl,--version
    OUTPUT_VARIABLE LD_VERSION
    ERROR_QUIET)
  if("${LD_VERSION}" MATCHES "GNU gold")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold")
  endif()
endif()

set(MANDIR share/man CACHE PATH "Directory for man pages")
set(DATADIR share CACHE PATH "Directory for shared data")
set(SYSCONFDIR /etc CACHE PATH "Directory for configuration files")
set(DOCDIR share/doc/tint3 CACHE PATH "Directory for documentation files")

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

include_directories(
  ${PROJECT_BINARY_DIR}
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/external_includes")

# }}} common rules

# {{{ coverage rules

# Coveralls integration is disabled by default, to avoid accidental uploads of
# coverage results from local runs. Only the CI server should upload them.
option(COVERALLS "Turn on Coveralls support" OFF)

if(COVERALLS)
  # This part adapted from the original at:
  #   https://github.com/JoakimSoderberg/coveralls-cmake/blob/master/cmake/Coveralls.cmake
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Coveralls: code coverage results with an optimised build may be misleading! Use -DCMAKE_BUILD_TYPE=Debug.")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
endif()

# }}} coverage rules

# {{{ test rules

include(TestTarget)
enable_testing()

# List of testdata files to copy
set(TESTDATA_SRCS
    src/launcher/testdata/applications/launcher_test.desktop
    src/launcher/testdata/.icons/UnitTestTheme/index.theme
    src/util/testdata/fs_test.txt)

# Generated list of output paths in the build directory
string(REGEX REPLACE "([^;]+)" "${CMAKE_BINARY_DIR}/\\1" TESTDATA_OUTS "${TESTDATA_SRCS}")

foreach(item IN LISTS TESTDATA_SRCS)
  add_custom_command(
    OUTPUT
      "${CMAKE_BINARY_DIR}/${item}"
    COMMAND
      ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/${item}" "${CMAKE_BINARY_DIR}/${item}"
    DEPENDS
      "${CMAKE_SOURCE_DIR}/${item}")
endforeach()

add_custom_target(
  testdata ALL
  DEPENDS
    ${TESTDATA_OUTS})

# }}} test rules

# {{{ install rules

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

add_subdirectory(abseil-cpp)
add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(test)
