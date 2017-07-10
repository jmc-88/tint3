include(CMakeParseArguments)
option(TINT3_ENABLE_PANDOC "Whether to use Pandoc to generate man pages" ON)
option(TINT3_ENABLE_GZIP_MAN_PAGES "Whether to use Gzip to compress the generated man pages" ON)

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

if(TINT3_ENABLE_GZIP_MAN_PAGES)
  if(NOT EXISTS ${GZIP_EXECUTABLE})
    find_program(GZIP_EXECUTABLE gzip)
    mark_as_advanced(GZIP_EXECUTABLE)
    if(NOT EXISTS ${GZIP_EXECUTABLE})
        message(FATAL_ERROR "Gzip not found. Install Gzip (https://www.gnu.org/software/gzip/) or set cache variable GZIP_EXECUTABLE.")
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
      DEPENDS "${target_name}_output_directory"
              "${ADD_PANDOC_MAN_PAGE_INPUT}"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    if(TINT3_ENABLE_GZIP_MAN_PAGES)
      set(MAN_PAGE_INSTALL_TARGET
        "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}.gz")
      add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}.gz"
        COMMAND ${GZIP_EXECUTABLE} -f9 "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}"
        DEPENDS "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}"
        WORKING_DIRECTORY ${OUTPUT_DIRECTORY})
    else()
      set(MAN_PAGE_INSTALL_TARGET
          "${CMAKE_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}")
    endif()

    add_custom_target(
      ${target_name} ALL
      DEPENDS "${MAN_PAGE_INSTALL_TARGET}")

    install(
      FILES "${MAN_PAGE_INSTALL_TARGET}"
      DESTINATION ${ADD_PANDOC_MAN_PAGE_DESTINATION})
  endif()
endfunction(add_pandoc_man_page)
