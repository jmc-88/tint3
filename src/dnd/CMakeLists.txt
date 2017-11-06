add_library(
  dnd_lib STATIC
  dnd.cc)

target_include_directories(
  dnd_lib
  PUBLIC
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  dnd_lib
  PRIVATE
    common_lib
    server_lib
  PUBLIC
    ${X11_X11_LIB})

test_target(
  dnd_test
  SOURCES
    dnd_test.cc
  LINK_LIBRARIES
    dnd_lib
    testmain)
