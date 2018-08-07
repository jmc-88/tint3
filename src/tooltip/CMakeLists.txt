add_library(
  tooltip_lib STATIC
  tooltip.cc)

target_include_directories(
  tooltip_lib
  PUBLIC
    ${CAIRO_INCLUDE_DIRS})

target_link_libraries(
  tooltip_lib
  PRIVATE
    common_lib
    log_lib
    panel_lib
    window_lib
  PUBLIC
    area_lib
    color_lib
    pango_lib
    server_lib
    timer_lib
    ${CAIRO_LIBRARIES}
    ${PANGOCAIRO_LIBRARIES})

test_target(
  tooltip_test
  SOURCES
    tooltip_test.cc
  INCLUDE_DIRS
    ${X11_X11_INCLUDE_DIRS}
  LINK_LIBRARIES
    area_lib
    environment_lib
    panel_lib
    server_lib
    testmain
    timer_test_utils_lib
    tooltip_lib
    ${X11_X11_LIB}
  USE_XVFB_RUN)
