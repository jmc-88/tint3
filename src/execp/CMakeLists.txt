add_library(
  execp_lib STATIC
  execp.cc)

target_link_libraries(
  execp_lib
  PRIVATE
    common_lib
    #environment_lib
    log_lib
    panel_lib
    subprocess_lib
    window_lib
  PUBLIC
    area_lib
    color_lib
    pango_lib)

test_target(
  execp_test
  SOURCES
    execp_test.cc
  LINK_LIBRARIES
    execp_lib
    testmain)
