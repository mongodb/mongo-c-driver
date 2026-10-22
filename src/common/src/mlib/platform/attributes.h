/**
 * @file mlib/platform/attributes.h
 * @date 2025-10-31
 *
 * This file defines macros for common compiler/platform extensions and attributes.
 *
 * @copyright Copyright 2009-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MLIB_PLATFORM_ATTRIBUTES_H_INCLUDED
#define MLIB_PLATFORM_ATTRIBUTES_H_INCLUDED

#include "./compiler.h"

#include <mlib/static_assert.h>

/**
 * @brief Expand to a _Pragma or __pragma declaration for the currrent compiler
 *
 *    mlib_pragma(<tokens>)
 *
 * The `<tokens>` should be the same tokens that would appear following a `#pragma`
 * preprocessor directive. This macro will automatically string-ify the tokens
 * if required by the underlying _Pragma directive.
 */
#define mlib_pragma _mlibPragma
// note: Bug on old GCC preprocessor prevents us from using if/else trick to omit MSVC code
#if mlib_is_msvc()
#define _mlibPragma(...) __pragma(__VA_ARGS__) mlib_static_assert(1, "")
#else
#define _mlibPragma(...) _Pragma(#__VA_ARGS__) mlib_static_assert(1, "")
#endif

#define MLIB_PRAGMA_IF_CLANG(...) MLIB_IF_CLANG(_Pragma(#__VA_ARGS__))
#define MLIB_PRAGMA_IF_GCC(...) MLIB_IF_GCC(_Pragma(#__VA_ARGS__))
#define MLIB_PRAGMA_IF_GNU_LIKE(...) MLIB_IF_GNU_LIKE(_Pragma(#__VA_ARGS__))
#define MLIB_PRAGMA_IF_MSVC(...) MLIB_IF_MSVC(__pragma(__VA_ARGS__))

/**
 * @brief Expands to the compiler's magic token for a C string that identifies
 * the current function.
 */
#define MLIB_FUNC MLIB_IF_GNU_LIKE(__func__) MLIB_IF_MSVC(__FUNCTION__)

/**
 * @brief Push the compiler's diagnostic setting state
 */
#define mlib_diagnostic_push()                         \
   MLIB_IF_GNU_LIKE(mlib_pragma(GCC diagnostic push);) \
   MLIB_PRAGMA_IF_MSVC(warning(push))                  \
   mlib_static_assert(1, "")

/**
 * @brief Pop the compiler's diagnostic state stack
 *
 */
#define mlib_diagnostic_pop()                         \
   MLIB_IF_GNU_LIKE(mlib_pragma(GCC diagnostic pop);) \
   MLIB_PRAGMA_IF_MSVC(warning(pop))                  \
   mlib_static_assert(1, "")

/**
 * @brief Disable the given warning if we are compiling with GCC
 *
 * @param Warning A string literal specifying the warning option to be disabled.
 */
#define mlib_gcc_warning_disable(Warning)                    \
   MLIB_IF_GCC(mlib_pragma(GCC diagnostic ignored Warning);) \
   mlib_static_assert(1, "")

/**
 * @brief Disable a compiler warning if we are on a GNU-like compiler
 *
 * @param Warning A string literal specifying the warning option to be disabled.
 */
#define mlib_gnu_warning_disable(Warning)                         \
   MLIB_IF_GNU_LIKE(mlib_pragma(GCC diagnostic ignored Warning);) \
   mlib_static_assert(1, "")

/**
 * @brief Emit an MSVC "warning()" pragma if we are compiling with MSVC
 */
#define mlib_msvc_warning(...)               \
   MLIB_PRAGMA_IF_MSVC(warning(__VA_ARGS__)) \
   mlib_static_assert(1, "")

/**
 * @brief Attribute macro that forces the function to be inlined at all call sites.
 *
 * Don't use this unless you really know that you need it, lest you generate code
 * bloat when the compiler's heuristics would do a better job.
 */
#define mlib_always_inline MLIB_IF_GNU_LIKE(__attribute__((always_inline)) inline) MLIB_IF_MSVC(__forceinline)

/**
 * @brief Annotate a variable as having thread-local storage duration
 *
 * This expands to the compiler's approximate equivalent of C11's _Thread_local
 * specifier.
 */
#define mlib_thread_local MLIB_IF_GNU_LIKE(__thread) MLIB_IF_MSVC(__declspec(thread))

/**
 * @brief Annotate an entity as possibly-unused
 *
 * This expands to an attribute that specifies the attached entity might be unused.
 * Roughly equivalent to the [[maybe_unused]] attribute. Not supported on all
 * compilers yet.
 */
#define mlib_maybe_unused MLIB_IF_GNU_LIKE(__attribute__((unused)))


/**
 * @brief Emit a _Pragma that will disable warnings about the use of deprecated entities.
 */
#define mlib_disable_deprecation_warnings()               \
   mlib_gnu_warning_disable("-Wdeprecated-declarations"); \
   mlib_msvc_warning(disable : 4996)

#if mlib_is_gnu_like()
#define mlib_have_typeof() 1
#elif defined _MSC_VER && _MSC_VER >= 1939 && !defined(__cplusplus)
// We can __typeof__ in MSVC 19.39+
#define mlib_have_typeof() 1
#else
#define mlib_have_typeof() 0
#endif

/**
 * @brief Equivalent to C23's `typeof()`, if it is supported by the current compiler.
 *
 * This expands to `__typeof__`, which is supported even on newer MSVC compilers,
 * even when not in C23 mode.
 */
#define mlib_typeof(...) MLIB_IF_ELSE(mlib_have_typeof())(__typeof__)(__mlib_typeof_is_not_supported)(__VA_ARGS__)

/**
 * @brief Disable warnings for constant conditional expressions.
 */
#define mlib_disable_constant_conditional_expression_warnings() mlib_msvc_warning(disable : 4127)

/**
 * @brief Disable warnings for potentially unused parameters.
 */
#define mlib_disable_unused_parameter_warnings()                     \
   MLIB_IF_GNU_LIKE(mlib_gnu_warning_disable("-Wunused-parameter");) \
   MLIB_IF_MSVC(mlib_msvc_warning(disable : 4100);) mlib_static_assert(1, "")

/**
 * @brief Annotate a function as having formatting semantics
 *
 * @param FormatIdx An integer literal for the 1-based index of the argument
 * that will be treated as a format string.
 * @param VaIdx An integer literal for the 1-based index of the variadic argument
 * list. If zero, then there is no variadic argument list.
 */
#define mlib_printf_attribute _mlibPrintfAttribute

#if mlib_is_clang()
#define _mlibPrintfAttribute(f, v) __attribute__((format(printf, f, v)))
#elif mlib_is_gcc()
#define _mlibPrintfAttribute(f, v) __attribute__((format(gnu_printf, f, v)))
#else
#define _mlibPrintfAttribute(f, v)
#endif

/**
 * @brief Annotate a boolean expression as "likely to be true" to guide the optimizer.
 * Use this very sparingly.
 */
#define mlib_likely(...) MLIB_IF_ELSE(mlib_is_gnu_like())(__builtin_expect(!!(__VA_ARGS__), 1))((__VA_ARGS__))
/**
 * @brief Annotate a boolean expression as "likely to be untrue" to guide the optimizer.
 * Use this very sparingly.
 */
#define mlib_unlikely(...) MLIB_IF_ELSE(mlib_is_gnu_like())(__builtin_expect(!!(__VA_ARGS__), 0))((__VA_ARGS__))

#endif // MLIB_PLATFORM_ATTRIBUTES_H_INCLUDED
