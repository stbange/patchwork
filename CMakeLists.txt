cmake_minimum_required(VERSION 3.24)
project(Patchwork VERSION 0.1 LANGUAGES C)
include(CTest)
include(FetchContent)
option(PATCHWORK_COVERAGE "Measure unit-test coverage with GCC" OFF)

find_package(SDL3 3.2 CONFIG QUIET)
if(NOT SDL3_FOUND)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-3.2.28
    URL_HASH SHA256=5c908f07a93087037d3319bc5d820ba8df14ac98bdbdf24af9084ef460ce01b1)
  FetchContent_MakeAvailable(SDL)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# The text renderer bakes the two bundled faces once at startup.
set(stb_dir "${CMAKE_CURRENT_BINARY_DIR}/stb")
file(MAKE_DIRECTORY "${stb_dir}")
file(DOWNLOAD
  https://raw.githubusercontent.com/nothings/stb/6b5799bae8a1fe3792691ef4b72ae26434c3e799/stb_truetype.h
  "${stb_dir}/stb_truetype.h"
  EXPECTED_HASH SHA256=428cfe93520f2aed5e413a220b0db5bba5b17e2ebe5a64e4a8efa9eeeb67ec5f TLS_VERIFY ON)
file(WRITE "${stb_dir}/truetype.c" "#define STB_TRUETYPE_IMPLEMENTATION
#include \"stb_truetype.h\"
")
add_library(truetype STATIC "${stb_dir}/truetype.c")
add_library(workroom STATIC src/i18n.c src/quilt.c src/patterns.c src/palettes.c src/cutting.c src/pdf.c src/storage.c src/draw.c src/app.c)
target_include_directories(workroom PUBLIC src PRIVATE "${stb_dir}")
target_link_libraries(workroom PUBLIC SDL3::SDL3 PRIVATE truetype)
if(MSVC)
  target_compile_options(workroom PUBLIC /utf-8)
else()
  target_link_libraries(workroom PUBLIC m)
endif()
if(PATCHWORK_COVERAGE)
  if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "PATCHWORK_COVERAGE requires GCC and gcov")
  endif()
  target_compile_options(workroom PRIVATE --coverage -O0 -g)
  target_link_options(workroom PUBLIC --coverage)
endif()

add_executable(patchwork src/main.c)
target_link_libraries(patchwork PRIVATE workroom)
set_target_properties(patchwork PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/app")
if(EMSCRIPTEN)
  set_target_properties(patchwork PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_options(patchwork PRIVATE
    "--js-library=${CMAKE_CURRENT_SOURCE_DIR}/web/storage.js"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    "-sSINGLE_FILE=1" "--embed-file" "${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
    "-sALLOW_MEMORY_GROWTH=1" "-sENVIRONMENT=web" "-sEXIT_RUNTIME=0")
  set_property(TARGET patchwork APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/DM-Sans.ttf" "${CMAKE_CURRENT_SOURCE_DIR}/assets/InstrumentSerif-Regular.ttf")
  set_property(TARGET patchwork APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html" "${CMAKE_CURRENT_SOURCE_DIR}/web/storage.js")
else()
  add_custom_command(TARGET patchwork POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:patchwork>/assets")
  if(WIN32)
    set_target_properties(patchwork PROPERTIES WIN32_EXECUTABLE ON)
  endif()
  if(BUILD_TESTING)
    add_executable(unit_tests tests/unit.c)
    set_target_properties(unit_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/app")
    target_link_libraries(unit_tests PRIVATE workroom)
    add_test(NAME unit COMMAND unit_tests)
    set_tests_properties(unit PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      ENVIRONMENT "SDL_VIDEO_DRIVER=dummy" ENVIRONMENT_MODIFICATION "SDL_RENDER_DRIVER=unset:")
  endif()
endif()
