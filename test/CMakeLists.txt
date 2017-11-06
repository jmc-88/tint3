add_library(
  testmain STATIC
  main.cc)

target_include_directories(
  testmain
  PRIVATE
    ${CAIRO_INCLUDE_DIRS}
    ${PANGOCAIRO_INCLUDE_DIRS})

target_link_libraries(
  testmain
  PRIVATE
    ${CAIRO_LIBRARIES}
    ${PANGOCAIRO_LIBRARIES})
