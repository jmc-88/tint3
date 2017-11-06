add_library(
  clock_lib STATIC
  clock.cc)

target_include_directories(
  clock_lib
  PRIVATE
    ${CAIRO_INCLUDE_DIRS})

target_link_libraries(
  clock_lib
  PRIVATE
    common_lib
    panel_lib
    server_lib
    time_utils_lib
    timer_lib
    window_lib
    ${CAIRO_LIBRARIES}
  PUBLIC
    area_lib
    common_lib
    pango_lib
    timer_lib)

test_target(
  clock_test
  SOURCES
    clock_test.cc
  INCLUDE_DIRS
    ${X11_X11_INCLUDE_DIRS}
  LINK_LIBRARIES
    clock_lib
    testmain
    ${X11_X11_LIB})

add_library(
  time_utils_lib STATIC
  time_utils.cc)

target_link_libraries(
  time_utils_lib
  PRIVATE
    environment_lib)

test_target(
  time_utils_test
  SOURCES
    time_utils_test.cc
  LINK_LIBRARIES
    environment_lib
    testmain
    time_utils_lib)
