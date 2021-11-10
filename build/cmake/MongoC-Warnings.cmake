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
      set (is_gnu_like "$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>")
      set (is_msvc "$<C_COMPILER_ID:MSVC>")
      if (CMAKE_MATCH_1 STREQUAL "gnu-like")
         add_compile_options ("$<${is_gnu_like}:${CMAKE_MATCH_2}>")
      elseif (CMAKE_MATCH_1 STREQUAL "msvc")
         add_compile_options ("$<${is_msvc}:${CMAKE_MATCH_2}>")
      else ()
         message (SEND_ERROR "Invalid option to mongoc_add_platform_compile_options(): '${opt}'")
      endif ()
   endforeach ()
endfunction ()

if (CMAKE_VERSION VERSION_LESS 3.3)
   # On older CMake versions, we'll just always pass the warning options, even
   # if the generate warnings for the C++ check file
   set (is_c_lang "1")
else ()
   # $<COMPILE_LANGUAGE> is only valid in CMake 3.3+
   set (is_c_lang "$<COMPILE_LANGUAGE:C>")
endif ()

# Warnings that should always be unconditional hard errors, as the code is
# almost definitely broken
mongoc_add_platform_compile_options (
     # Implicit function or variable declarations
     gnu-like:$<${is_c_lang}:-Werror=implicit> msvc:/we4013 msvc:/we4431
     # Missing return types/statements
     gnu-like:-Werror=return-type msvc:/we4716
     # Incompatible pointer types
     gnu-like:$<${is_c_lang}:-Werror=incompatible-pointer-types> msvc:/we4113
     # Integral/pointer conversions
     gnu-like:$<${is_c_lang}:-Werror=int-conversion> msvc:/we4047
     # Discarding qualifiers
     gnu-like:$<${is_c_lang}:-Werror=discarded-qualifiers> msvc:/we4090
     # Definite use of uninitialized value
     gnu-like:-Werror=uninitialized msvc:/we4700

     # Aside: Disable CRT insecurity warnings
     msvc:/D_CRT_SECURE_NO_WARNINGS
     )
