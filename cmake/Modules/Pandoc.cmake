include(CMakeParseArguments)
option(TINT3_ENABLE_PANDOC "Whether to use Pandoc to generate man pages" ON)

if(TINT3_ENABLE_PANDOC)
  if(NOT EXISTS ${PANDOC_EXECUTABLE})
    find_program(PANDOC_EXECUTABLE pandoc)
    mark_as_advanced(PANDOC_EXECUTABLE)
    if(NOT EXISTS ${PANDOC_EXECUTABLE})
        message(FATAL_ERROR "Pandoc not found. Install Pandoc (http://pandoc.org/) or set cache variable PANDOC_EXECUTABLE.")
        return()
    endif()
  endif()
endif()

function(add_pandoc_man_page target_name)
  set(oneValueArgs INPUT OUTPUT DESTINATION)
  cmake_parse_arguments(ADD_PANDOC_MAN_PAGE "" "${oneValueArgs}" "" ${ARGN})

  if(TINT3_ENABLE_PANDOC)
    get_filename_component(
      OUTPUT_DIRECTORY
      "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}" DIRECTORY)
    add_custom_target(
      "${target_name}_output_directory" ALL
      COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIRECTORY})

    add_custom_command(
      OUTPUT "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}"
      COMMAND ${PANDOC_EXECUTABLE}
          --standalone --from=markdown --to=man
          --output="${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}"
          "${ADD_PANDOC_MAN_PAGE_INPUT}"
      DEPENDS ${OUTPUT_DIRECTORY}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_custom_target(
      ${target_name} ALL
      DEPENDS "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}")

    install(
      FILES "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}"
      DESTINATION ${ADD_PANDOC_MAN_PAGE_DESTINATION})
  endif()
endfunction(add_pandoc_man_page)
