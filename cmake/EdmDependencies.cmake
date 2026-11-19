# Dependencies, fetched at configure time (needs network).
#
# edm_fetch_googletest(): fetches GoogleTest via FetchContent and makes
# GTest::gtest_main available. The SHA256 is the tamper check on the
# downloaded archive — it stays, as a named variable next to the
# version, never a literal inline.
#
# To bump the version: download the new release zip from
# https://github.com/google/googletest/releases, run `sha256sum` on
# it, and paste the hash into EDM_GOOGLETEST_SHA256 below.
#
# NOTE: include(GoogleTest) must stay at FILE scope, not inside
# edm_fetch_googletest(). The GoogleTest module sets its discovery
# script path (_GOOGLETEST_DISCOVER_TESTS_SCRIPT) at include scope;
# included inside a function, the variable vanishes when the
# function returns, and gtest_discover_tests then silently
# registers <test>_NOT_BUILT tests (no discovery ever runs).
# Proven 2026-11-19: the in-function include produced 9
# _NOT_BUILT tests; the file-scope include discovers all of them.
include(GoogleTest)

function(edm_fetch_googletest)
  set(EDM_GOOGLETEST_VERSION 1.17.0)
  set(EDM_GOOGLETEST_SHA256 40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v${EDM_GOOGLETEST_VERSION}.zip
    URL_HASH SHA256=${EDM_GOOGLETEST_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)
endfunction()
