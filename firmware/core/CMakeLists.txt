# Host build of the hardware-independent core plus its unit tests.
#   cmake -S firmware/core -B firmware/core/build
#   cmake --build firmware/core/build
#   ctest --test-dir firmware/core/build --output-on-failure
cmake_minimum_required(VERSION 3.20)
project(kb_core LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake)

add_library(kb_core STATIC ${KB_CORE_SOURCES})
target_include_directories(kb_core PUBLIC ${KB_CORE_INCLUDE_DIRS})
if(MSVC)
  target_compile_options(kb_core PRIVATE /W4 /WX)
else()
  target_compile_options(kb_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()

function(kb_add_test name)
  add_executable(${name} tests/${name}.c)
  target_link_libraries(${name} PRIVATE kb_core)
  target_include_directories(${name} PRIVATE tests)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

kb_add_test(test_core)
kb_add_test(test_hold)
kb_add_test(test_screen)
kb_add_test(test_version)
