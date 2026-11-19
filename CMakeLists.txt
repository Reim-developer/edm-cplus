cmake_minimum_required(VERSION 3.24)
project(edm-cplus VERSION 0.0.4 LANGUAGES CXX)

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
  src/sfx.cxx
  src/pattern.cxx
  src/synth_voice.cxx
  src/sequencer.cxx
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
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    URL_HASH SHA256=40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)

  # One gtest binary per suite; gtest_discover_tests registers each
  # TEST() as its own CTest test. edm_warnings applies to our targets
  # only — never to the gtest targets.
  function(edm_test name)
    add_executable(test_${name} tests/test_${name}.cxx)
    target_link_libraries(test_${name} PRIVATE edm GTest::gtest_main)
    edm_warnings(test_${name})
    gtest_discover_tests(test_${name})
  endfunction()

  edm_test(rng)
  edm_test(filter)
  edm_test(oscillator)
  edm_test(envelope)
  edm_test(wav)
  edm_test(sfx)
  edm_test(pattern)
  edm_test(voice)
  edm_test(sequencer)

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
