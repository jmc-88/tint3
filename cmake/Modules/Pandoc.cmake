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
    add_custom_command(
      OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}"
      COMMAND
        ${PANDOC_EXECUTABLE}
        --standalone --from=markdown --to=man
        --output="${ADD_PANDOC_MAN_PAGE_OUTPUT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/${ADD_PANDOC_MAN_PAGE_INPUT}"
      DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${ADD_PANDOC_MAN_PAGE_INPUT}")

    if(TINT3_ENABLE_GZIP_MAN_PAGES)
      set(MAN_PAGE_INSTALL_TARGET
          "${CMAKE_CURRENT_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}.gz")
      add_custom_command(
        OUTPUT
          "${CMAKE_CURRENT_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}.gz"
        COMMAND 
          ${GZIP_EXECUTABLE} -k -f -n -9 "${ADD_PANDOC_MAN_PAGE_OUTPUT}"
        DEPENDS
          "${CMAKE_CURRENT_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}")
    else()
      set(MAN_PAGE_INSTALL_TARGET
          "${CMAKE_CURRENT_BINARY_DIR}/${ADD_PANDOC_MAN_PAGE_OUTPUT}")
    endif()

    add_custom_target(
      ${target_name} ALL
      DEPENDS
        "${MAN_PAGE_INSTALL_TARGET}")

    install(
      FILES
        "${MAN_PAGE_INSTALL_TARGET}"
      DESTINATION
        ${ADD_PANDOC_MAN_PAGE_DESTINATION})
  endif()
endfunction(add_pandoc_man_page)
