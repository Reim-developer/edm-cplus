cmake_minimum_required(VERSION 3.24)
project(edm-cplus VERSION 0.0.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project CMake modules: warning flags, dependencies, test helpers.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EdmWarnings)

# The synth core. Non-trivial implementations live in src/*.cxx and are
# added to the target as they land (see target_sources below);
# templates and small value types live in include/edm/*.hxx.
add_library(edm
  src/filter.cxx
)
target_sources(edm PRIVATE
  src/wav.cxx
  src/sfx.cxx
  src/plugins.cxx
  src/pattern.cxx
  src/synth_voice.cxx
  src/sequencer.cxx
  src/section.cxx
)
target_include_directories(edm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# The version header is generated from version.hxx.in: the version
# lives in exactly one place, project(... VERSION ...) above.
configure_file(include/edm/version.hxx.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/edm/version.hxx @ONLY)
target_include_directories(edm PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)

edm_warnings(edm)

option(EDM_BUILD_TESTS "Build edm-cplus tests" ON)
option(EDM_BUILD_EXAMPLES "Build edm-cplus examples" ON)

if(EDM_BUILD_EXAMPLES)
  add_executable(tone examples/tone.cxx)
  target_link_libraries(tone PRIVATE edm)
  edm_warnings(tone)

  add_executable(sfx_demo examples/sfx_demo.cxx)
  target_link_libraries(sfx_demo PRIVATE edm)
  edm_warnings(sfx_demo)

  add_executable(sequence_demo examples/sequence_demo.cxx)
  target_link_libraries(sequence_demo PRIVATE edm)
  edm_warnings(sequence_demo)
endif()

if(EDM_BUILD_TESTS)
  enable_testing()

  # GoogleTest via FetchContent (needs network at configure time).
  include(EdmDependencies)
  edm_fetch_googletest()

  include(EdmTests)

  edm_test(rng)
  edm_test(filter)
  edm_test(oscillator)
  edm_test(envelope)
  edm_test(wav)
  edm_test(sfx)
  edm_test(pattern)
  edm_test(voice)
  edm_test(sequencer)
  edm_test(buffer)
  edm_test(automation)

  if(EDM_BUILD_EXAMPLES)
    # tone_renders runs the tone example; wav_validity checks its
    # output with the C++ validator (no Python anywhere).
    add_test(NAME tone_renders COMMAND tone ${CMAKE_CURRENT_BINARY_DIR}/tone.wav)
    add_executable(check_wav tests/check_wav.cxx)
    edm_warnings(check_wav)
    add_test(NAME wav_validity
      COMMAND check_wav ${CMAKE_CURRENT_BINARY_DIR}/tone.wav 44100 1.0)
    set_tests_properties(wav_validity PROPERTIES DEPENDS tone_renders)

    add_test(NAME sfx_demo COMMAND sfx_demo ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME sequence_demo COMMAND sequence_demo ${CMAKE_CURRENT_BINARY_DIR})
  endif()
endif()
