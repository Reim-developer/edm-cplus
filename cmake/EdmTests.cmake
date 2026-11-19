# edm_test(name): one gtest binary per suite; gtest_discover_tests
# registers each TEST() as its own CTest test. edm_warnings applies
# to our targets only — never to the gtest targets.
function(edm_test name)
  add_executable(test_${name} tests/test_${name}.cxx)
  target_link_libraries(test_${name} PRIVATE edm GTest::gtest_main)
  edm_warnings(test_${name})
  gtest_discover_tests(test_${name})
endfunction()
