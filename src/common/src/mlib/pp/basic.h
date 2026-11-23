/**
 * @file mlib/pp/basic.h
 * @brief Very basic preprocessor macros and utilities
 * @date 2025-07-29
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
 *
 */

#ifndef MLIB_PP_BASIC_H_INCLUDED
#define MLIB_PP_BASIC_H_INCLUDED

/**
 * @brief A function-like macro that expands to nothing
 *
 * Takes any number of arguments, and will always expand to no tokens.
 *
 * This macro is primarily useful to prevent or defer to the expansion of other
 * macros.
 *
 * ## Supressing function-like macros
 *
 * You can place this between a function-like macro name and the opening
 * parenthesis of its argument list to prevent/defer the expansion of that
 * function-like macro:
 *
 *      SOME_FUNC_MACRO MLIB_NOTHING("separator") (arg1, arg2, arg3)
 *
 * this causes the above to expand to
 *
 *      SOME_FUNC_MACRO(arg1, arg2, arg3)
 *
 * even if `SOME_FUNC_MACRO` is defined. Note that applying another expansion
 * *will* expand `SOME_FUNC_MACRO`.
 */
#define MLIB_NOTHING(...)

/**
 * @brief Expand to the argument list directly when invoked as a function-like macro
 *
 * This can be used to force another expansion pass of the arguments, since using
 * a function-like macro will cause the preprocessor to expand the argument list.
 */
#define MLIB_JUST(...) __VA_ARGS__

/**
 * @brief Expand to the first argument in the given argument list. If given no
 * arguments, expands to nothing.
 */
#define MLIB_FIRST_ARG(...) _mlibFirstArgJust(_mlibFirstArg MLIB_NOTHING("MSVC deferral")(__VA_ARGS__, ~))
#define _mlibFirstArg(X, ...) X
#define _mlibFirstArgJust(...) __VA_ARGS__

/**
 * @brief Perform token-pasting after expanding the macro arguments
 *
 * This is needed instead of direct token-pasting, because token pasting within
 * a macro definition will paste the immediately passed argument tokens without
 * expanding them. Passing it through this macro will cause the expected behavior:
 *
 *      #define FOO lhs
 *      #define BAR _rhs
 *
 *      #define BAD_PASTE(A, B) A ## B
 *
 *      BAD_PASTE(FOO, BAR)   // ← Bad:  Expands to `FOOBAR`
 *      MLIB_PASTE(FOO, BAR)  // ← Good: Expands to `lhs_rhs`
 *
 * Additionally, `MLIB_PASTE_<n>` is also defined for pasting 3, 4, and 5 tokens.
 *
 * The MSVC branch is to workaround the old bad MSVC preprocessor and can be removed
 * when the conforming preprocessor is reliably available.
 */
#ifndef _MSC_VER
#define MLIB_PASTE(A, ...) _mlibPaste(A, __VA_ARGS__)
#else
#define MLIB_PASTE(A, ...) MLIB_JUST(_mlibPaste(A, __VA_ARGS__))
#endif
// Paste three tokens
#define MLIB_PASTE_3(A, B, ...) MLIB_PASTE(A, MLIB_PASTE(B, __VA_ARGS__))
// Paste four tokens
#define MLIB_PASTE_4(A, B, C, ...) MLIB_PASTE(A, MLIB_PASTE_3(B, C, __VA_ARGS__))
// Paste five tokens
#define MLIB_PASTE_5(A, B, C, D, ...) MLIB_PASTE(A, MLIB_PASTE_4(B, C, D, __VA_ARGS__))
#define _mlibPaste(A, ...) A##__VA_ARGS__

/**
 * @brief String-ify the given set of preprocessor tokens after macro expansion
 *
 * This is needed for the same reason that `MLIB_PASTE` is needed: string-ifying
 * a macro parameter directly will string-ify the unexpanded argument:
 *
 *      #define FOO the_expanded_macro
 *
 *      #define BAD_STR(...) #__VA_ARGS__
 *
 *      BAD_STR(FOO)  // ← Bad:  Expands to "FOO"
 *      MLIB_STR(FOO) // ← Good: Expands to "the_expanded_macro"
 */
#define MLIB_STR(...) _mlibStr(__VA_ARGS__)
// The leading "" empty string is intentional and forces the syntax to be a string literal
// with string token concatenation.
#define _mlibStr(...) "" #__VA_ARGS__

/**
 * @brief Repeatedly rescan the argument list to complete deferred macro calls
 *
 * This is useful in the case that macro expansion causes additional macro forms
 * to appear, which will need their own macro expansions.
 *
 * Each level has its own macro name so that an enclosing expansion does not
 * suppress a later level as recursive self-expansion. Traditional MSVC needs
 * more rescans than conforming preprocessors; keep its larger expansion tree
 * out of GNU/Clang builds, where it adds preprocessing cost and long backtraces.
 *
 * The expansion trees have 4096 and 64 leaves, respectively. These are not
 * portable recursion-depth guarantees: argument prescanning differs between
 * preprocessors. Both must complete full-length and nested MLIB_MAP_MACRO calls.
 */
#if defined(_MSC_VER) && !defined(__clang__) && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL)
#define MLIB_EVAL(...) _mlibEval4096(__VA_ARGS__)
#else
#define MLIB_EVAL(...) _mlibEval64(__VA_ARGS__)
#endif
#define _mlibEvalOnce(...) __VA_ARGS__
#define _mlibEvalFourTimes(...) _mlibEvalOnce(_mlibEvalOnce(_mlibEvalOnce(_mlibEvalOnce(__VA_ARGS__))))
#define _mlibEvalSixteenTimes(...) \
   _mlibEvalFourTimes(_mlibEvalFourTimes(_mlibEvalFourTimes(_mlibEvalFourTimes(__VA_ARGS__))))
#define _mlibEval64(...) \
   _mlibEvalSixteenTimes(_mlibEvalSixteenTimes(_mlibEvalSixteenTimes(_mlibEvalSixteenTimes(__VA_ARGS__))))
#define _mlibEval256(...) _mlibEval64(_mlibEval64(_mlibEval64(_mlibEval64(__VA_ARGS__))))
#define _mlibEval1024(...) _mlibEval256(_mlibEval256(_mlibEval256(_mlibEval256(__VA_ARGS__))))
#define _mlibEval4096(...) _mlibEval1024(_mlibEval1024(_mlibEval1024(_mlibEval1024(__VA_ARGS__))))

/**
 * @brief Pass a function-like macro name, inhibiting its expansion until the
 * next pass:
 *
 *      #define func_macro(x) x
 *
 *      MLIB_DEFERRED(func_macro)(foo)  // Expands to "func_macro(foo)", not "foo"
 */
#define MLIB_DEFERRED(MacroName)                                                                  \
   /* Expand to the macro name: */                                                                \
   MacroName /*-                                                                                  \
              * Place a separator between the function macro name and whatever comes next         \
              * in the file. Presumably, the next token will be the parens to invoke "MacroName", \
              * but this separator inhibits its expansion unless something else comes             \
              * along to do another expansion pass (e.g. MLIB_EVAL())                             \
              */                                                                                  \
      MLIB_NOTHING("[separator]")

// clang-format off
/*-
 * This helper macro expands to its 64th argument, no matter how many
 * arguments it is given. You must pass at least 65 arguments to this macro:
 * the 64 named parameters, plus at least one more to satisfy the requirement
 * that a variadic macro's `...` receive a non-empty argument list.
 *
 * This is the primitive behind the "shift an argument list along a ruler and
 * see what lands in the 64th slot" trick, and it is what bounds the argument
 * counts of everything built on top of it. It is shared by `mlib/pp/is-empty.h`
 * (`_mlibHasComma`) and `mlib/pp/args.h` (`MLIB_ARG_COUNT`), which is why it
 * lives here among the primitives rather than in either of those headers.
 * Widening any of those limits means extending this macro first.
 */
#define _mlibPick64th( \
        _0,  _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, \
        _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
        _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
        _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
        _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, \
        _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, \
        _60, _61, _62, _63, ...) \
    _63
// clang-format on

#endif // MLIB_PP_BASIC_H_INCLUDED
