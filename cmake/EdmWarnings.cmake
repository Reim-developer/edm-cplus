# edm_warnings(target): the project's warning flags. MSVC gets /W4 /WX,
# everything else gets -Wall -Wextra -Werror -pedantic. Applies to our
# targets only — never to the gtest targets.
function(edm_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -pedantic)
  endif()
endfunction()
