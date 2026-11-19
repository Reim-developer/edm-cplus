cmake_minimum_required(VERSION 3.20)
project(edm-cplus VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The synth core. Non-trivial implementations live in src/*.cxx and are
# added to the target as they land (see target_sources below);
# templates and small value types live in include/edm/*.hxx.
add_library(edm
  src/filter.cxx
)
target_sources(edm PRIVATE
  src/wav.cxx
)
target_include_directories(edm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

function(edm_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
endfunction()

edm_warnings(edm)

option(EDM_BUILD_TESTS "Build edm-cplus tests" ON)
option(EDM_BUILD_EXAMPLES "Build edm-cplus examples" ON)

if(EDM_BUILD_TESTS)
  enable_testing()
  add_executable(test_rng tests/test_rng.cxx)
  target_link_libraries(test_rng PRIVATE edm)
  target_include_directories(test_rng PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  edm_warnings(test_rng)
  add_test(NAME rng COMMAND test_rng)

  add_executable(test_filter tests/test_filter.cxx)
  target_link_libraries(test_filter PRIVATE edm)
  target_include_directories(test_filter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  edm_warnings(test_filter)
  add_test(NAME filter COMMAND test_filter)

  add_executable(test_oscillator tests/test_oscillator.cxx)
  target_link_libraries(test_oscillator PRIVATE edm)
  target_include_directories(test_oscillator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  edm_warnings(test_oscillator)
  add_test(NAME oscillator COMMAND test_oscillator)

  add_executable(test_envelope tests/test_envelope.cxx)
  target_link_libraries(test_envelope PRIVATE edm)
  target_include_directories(test_envelope PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  edm_warnings(test_envelope)
  add_test(NAME envelope COMMAND test_envelope)

  add_executable(test_wav tests/test_wav.cxx)
  target_link_libraries(test_wav PRIVATE edm)
  target_include_directories(test_wav PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  edm_warnings(test_wav)
  add_test(NAME wav COMMAND test_wav)
endif()
