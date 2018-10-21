add_library(
  area_lib STATIC
  area.cc)

target_include_directories(
  area_lib
  PRIVATE
    ${X11_Xrender_INCLUDE_DIRS}
  PUBLIC
    ${CAIRO_INCLUDE_DIRS}
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  area_lib
  PRIVATE
    common_lib
    geometry_lib
    panel_lib
    server_lib
    ${X11_Xrender_LIB}
  PUBLIC
    color_lib
    absl::optional
    ${CAIRO_LIBRARIES}
    ${X11_X11_LIB})

test_target(
  area_test
  SOURCES
    area_test.cc
  LINK_LIBRARIES
    area_lib
    environment_lib
    panel_lib
    server_lib
    timer_lib
    testmain
  USE_XVFB_RUN)

add_library(
  bimap_lib INTERFACE)

target_sources(
  bimap_lib
  INTERFACE
    "${PROJECT_SOURCE_DIR}/src/util/bimap.hh")

test_target(
  bimap_test
  SOURCES
    bimap_test.cc
  LINK_LIBRARIES
    bimap_lib
    testmain)

add_library(
  collection_lib INTERFACE)

target_sources(
  collection_lib
  INTERFACE
    "${PROJECT_SOURCE_DIR}/src/util/collection.hh")

test_target(
  collection_test
  SOURCES
    collection_test.cc
  LINK_LIBRARIES
    collection_lib
    testmain)

add_library(
  common_lib STATIC
  common.cc)

target_include_directories(
  common_lib
  PRIVATE
    ${X11_X11_INCLUDE_DIRS}
    ${X11_Xrender_INCLUDE_DIRS}
  PUBLIC
    ${GOBJECT2_INCLUDE_DIRS}
    ${IMLIB2_INCLUDE_DIRS})

target_link_libraries(
  common_lib
  PRIVATE
    server_lib
    ${X11_X11_LIB}
    ${X11_Xrender_LIB}
  PUBLIC
    log_lib
    absl::strings
    ${GOBJECT2_LIBRARIES}
    ${IMLIB2_LIBRARIES})

test_target(
  common_test
  SOURCES
    common_test.cc
  LINK_LIBRARIES
    common_lib
    testmain)

add_library(
  color_lib STATIC
  color.cc)

target_link_libraries(
  color_lib
  PRIVATE
    area_lib
  PUBLIC
    geometry_lib
    absl::optional)

test_target(
  color_test
  SOURCES
    color_test.cc
  DEPENDS
    behavior_control_lib
  LINK_LIBRARIES
    color_lib
    testmain)

add_library(
  environment_lib STATIC
  environment.cc)

target_link_libraries(
  environment_lib
  PUBLIC
    common_lib
    log_lib)

test_target(
  environment_test
  SOURCES
    environment_test.cc
  LINK_LIBRARIES
    environment_lib
    testmain)

add_library(
  fs_lib STATIC
  fs.cc)

target_link_libraries(
  fs_lib
  PRIVATE
    common_lib
    environment_lib
  PUBLIC
    absl::strings)

test_target(
  fs_test
  SOURCES
    fs_test.cc
  DEPENDS
    testdata
    behavior_control_lib
    absl::strings
  LINK_LIBRARIES
    environment_lib
    fs_lib
    fs_test_utils_lib
    testmain)

add_library(
  fs_test_utils_lib STATIC
  fs_test_utils.cc)

target_link_libraries(
  fs_test_utils_lib
  PUBLIC
    fs_lib)

add_library(
  geometry_lib STATIC
  geometry.cc)

test_target(
  geometry_test
  SOURCES
    geometry_test.cc
  LINK_LIBRARIES
    geometry_lib
    testmain)

add_library(
  gradient_lib STATIC
  gradient.cc)

target_include_directories(
  gradient_lib
  PUBLIC
    ${CAIRO_INCLUDE_DIRS})

target_link_libraries(
  gradient_lib
  PRIVATE
    log_lib
  PUBLIC
    color_lib
    geometry_lib
    ${CAIRO_LIBRARIES})

test_target(
  gradient_test
  SOURCES
    gradient_test.cc
  LINK_LIBRARIES
    color_lib
    gradient_lib
    testmain)

add_library(
  imlib2_lib STATIC
  imlib2.cc)

target_include_directories(
  imlib2_lib
  PUBLIC
    ${IMLIB2_INCLUDE_DIRS})

target_link_libraries(
  imlib2_lib
  PRIVATE
    common_lib
  PUBLIC
    ${IMLIB2_LIBRARIES})

test_target(
  imlib2_test
  SOURCES
    imlib2_test.cc
  DEPENDS
    behavior_control_lib
  LINK_LIBRARIES
    imlib2_lib
    testmain)

add_library(
  log_lib STATIC
  log.cc)

add_library(
  pango_lib STATIC
  pango.cc)

target_include_directories(
  pango_lib
  PUBLIC
    ${PANGO_INCLUDE_DIRS})

target_link_libraries(
  pango_lib
  PUBLIC
    ${PANGO_LIBRARIES})

test_target(
  pango_test
  SOURCES
    pango_test.cc
  LINK_LIBRARIES
    pango_lib
    testmain)

add_library(
  pipe_lib STATIC
  pipe.cc)

target_link_libraries(
  pipe_lib
  PRIVATE
    log_lib)

test_target(
  pipe_test
  SOURCES
    pipe_test.cc
  LINK_LIBRARIES
    log_lib
    pipe_lib
    testmain)

add_library(
  timer_lib STATIC
  timer.cc)

target_link_libraries(
  timer_lib
  PRIVATE
    log_lib
  PUBLIC
    bimap_lib
    absl::optional
    absl::time)

test_target(
  timer_test
  SOURCES
    timer_test.cc
  LINK_LIBRARIES
    testmain
    timer_lib
    timer_test_utils_lib)

add_library(
  timer_test_utils_lib STATIC
  timer_test_utils.cc)

target_link_libraries(
  timer_test_utils_lib
  PUBLIC
    timer_lib)

add_library(
  window_lib STATIC
  window.cc)

target_include_directories(
  window_lib
  PRIVATE
    ${CAIRO_INCLUDE_DIRS}
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  window_lib
  PRIVATE
    common_lib
    imlib2_lib
    panel_lib
    server_lib
    taskbar_lib
    ${X11_X11_LIB}
    ${CAIRO_LIBRARIES}
  PUBLIC
    pango_lib)

add_library(
  x11_lib STATIC
  x11.cc)

target_include_directories(
  x11_lib
  PUBLIC
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  x11_lib
  PRIVATE
    log_lib
    panel_lib
    server_lib
  PUBLIC
    pipe_lib
    timer_lib
    ${X11_X11_LIB})

add_library(
  xdg_lib STATIC
  xdg.cc)

target_link_libraries(
  xdg_lib
  PRIVATE
    common_lib
    environment_lib
    log_lib
    absl::strings
  PUBLIC
    fs_lib)

test_target(
  xdg_test
  SOURCES
    xdg_test.cc
  LINK_LIBRARIES
    environment_lib
    testmain
    xdg_lib)
