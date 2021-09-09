#[[
   Define additional compile options, conditional on the compiler being used.
   Each option should be prefixed by `gnu-like:` or `msvc:`. Those options will be
   enabled for GCC/Clang or MSVC respectively.

   These options are attached to the source directory and its children.
]]
function (mongoc_add_platform_compile_options)
   foreach (opt IN LISTS ARGV)
      if (NOT opt MATCHES "^(gnu-like|msvc):(.*)")
         message (SEND_ERROR "Invalid option '${opt}' (Should be prefixed by 'msvc:' or 'gnu-like:'")
         continue ()
      endif ()
      set (is_gnu_like "$<C_COMPILER_ID:GNU,Clang,AppleClang>")
      set (is_msvc "$<C_COMPILER_ID:MSVC>")
      if (CMAKE_MATCH_1 STREQUAL "gnu-like")
         add_compile_options ("$<${is_gnu_like}:${CMAKE_MATCH_2}>")
      else ()
         add_compile_options ("$<${is_msvc}:${CMAKE_MATCH_2}>")
      endif ()
   endforeach ()
endfunction ()

# Warnings that should always be unconditional hard errors, as the code is
# inherently broken
mongoc_add_platform_compile_options (
     # Implicit function or variable declarations
     gnu-like:-Werror=implicit msvc:/we4013 msvc:/we4431
     # Missing return types/statements
     gnu-like:-Werror=return-type msvc:/we4716
     )

