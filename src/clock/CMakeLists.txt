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
    subprocess_lib
    timer_lib
    window_lib
    absl::time
    ${CAIRO_LIBRARIES}
  PUBLIC
    area_lib
    common_lib
    pango_lib
    timer_lib
    ${PANGOCAIRO_LIBRARIES})

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
