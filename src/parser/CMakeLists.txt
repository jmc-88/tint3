add_library(
  parser_lib STATIC
  parser.cc)

target_link_libraries(
  parser_lib
  PUBLIC
    common_lib
    lexer_lib)

test_target(
  parser_test
  SOURCES
    parser_test.cc
  LINK_LIBRARIES
    parser_lib
    testmain)

add_library(
  lexer_lib STATIC
  lexer.cc)

test_target(
  lexer_test
  SOURCES
    lexer_test.cc
  LINK_LIBRARIES
    lexer_lib
    testmain)
