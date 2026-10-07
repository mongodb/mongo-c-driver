#include <mlib/platform/attributes.h>
#include <mlib/pp/args.h>
#include <mlib/pp/basic.h>
#include <mlib/pp/boolean.h>
#include <mlib/pp/is-empty.h>
#include <mlib/pp/map.h>
#include <mlib/static_assert.h>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

/// Define a simple static assert that does not depend on our preprocessor machinery
#define SUPER_SIMPLE_STATIC_ASSERT(Name, ...) \
   extern int _static_assertion__##Name##_lineno_##__LINE__[(__VA_ARGS__) ? 1 : -1]

// basic.h
SUPER_SIMPLE_STATIC_ASSERT(trivial, 1);
SUPER_SIMPLE_STATIC_ASSERT(unequal, 2 != 5);
SUPER_SIMPLE_STATIC_ASSERT(nothing_is_empty, 1 == MLIB_NOTHING() 1);
SUPER_SIMPLE_STATIC_ASSERT(nothing_is_empty, 1 == MLIB_NOTHING(one_arg) 1);
SUPER_SIMPLE_STATIC_ASSERT(nothing_is_empty, 1 == MLIB_NOTHING("content", "ignored") 1);
SUPER_SIMPLE_STATIC_ASSERT(just_is_simple, 1 == MLIB_JUST(1));
SUPER_SIMPLE_STATIC_ASSERT(just_is_simple, 1 == MLIB_JUST(/* empty */) 1);
SUPER_SIMPLE_STATIC_ASSERT(first_arg, 1 == MLIB_FIRST_ARG(1, 2, 3));
SUPER_SIMPLE_STATIC_ASSERT(first_arg_empty, 1 == MLIB_FIRST_ARG() 1);
SUPER_SIMPLE_STATIC_ASSERT(first_arg_single, 9 == MLIB_FIRST_ARG(9));

// MLIB_PASTE performs the paste *after* expanding its arguments. Both operands
// here are macros, so a naive `##` would produce `PASTE_LHSPASTE_RHS`; MLIB_PASTE
// must instead expand them to `lhs` and `_rhs`, paste to `lhs_rhs`, and rescan
// that into its value.
#define PASTE_LHS lhs
#define PASTE_RHS _rhs
#define lhs_rhs 123
SUPER_SIMPLE_STATIC_ASSERT(paste_expands_args, MLIB_PASTE(PASTE_LHS, PASTE_RHS) == 123);
// The 3/4/5-token paste variants (operands here are not macros, so they paste
// literally into a name that then expands):
#define abc 7
#define abcd 8
#define abcde 9
SUPER_SIMPLE_STATIC_ASSERT(paste_3, MLIB_PASTE_3(a, b, c) == 7);
SUPER_SIMPLE_STATIC_ASSERT(paste_4, MLIB_PASTE_4(a, b, c, d) == 8);
SUPER_SIMPLE_STATIC_ASSERT(paste_5, MLIB_PASTE_5(a, b, c, d, e) == 9);

// MLIB_STR stringizes *after* expansion. We can't compare strings in an array-size
// constant expression, but sizeof (== string length + 1) lets us prove the
// argument was expanded: the result matches the expansion's length and differs
// from the length of the unexpanded spelling.
#define STR_ME the_expanded_macro
SUPER_SIMPLE_STATIC_ASSERT(str_expands_arg, sizeof(MLIB_STR(STR_ME)) == sizeof("the_expanded_macro"));
SUPER_SIMPLE_STATIC_ASSERT(str_not_verbatim, sizeof(MLIB_STR(STR_ME)) != sizeof("STR_ME"));
SUPER_SIMPLE_STATIC_ASSERT(str_empty, sizeof(MLIB_STR()) == sizeof(""));

// boolean.h
SUPER_SIMPLE_STATIC_ASSERT(bool_true, MLIB_BOOLEAN(1));
SUPER_SIMPLE_STATIC_ASSERT(bool_false, !MLIB_BOOLEAN(0));
// NOTE: MLIB_BOOLEAN/MLIB_NEGATE also accept the `true` and `false` word
// spellings, but we deliberately do not test them here: a standard library may
// #define `true`/`false` to a non-trivial token sequence (e.g. a `(_Bool)` cast
// in some C99 <stdbool.h> implementations), which would defeat the required
// token-paste and make such a test unreliable across toolchains.
SUPER_SIMPLE_STATIC_ASSERT(bool_negate, !MLIB_NEGATE(1));
SUPER_SIMPLE_STATIC_ASSERT(bool_negate, MLIB_NEGATE(0));
// The empty spelling normalizes to `0`:
SUPER_SIMPLE_STATIC_ASSERT(bool_empty, !MLIB_BOOLEAN());
SUPER_SIMPLE_STATIC_ASSERT(bool_negate_empty, MLIB_NEGATE());
SUPER_SIMPLE_STATIC_ASSERT(bool_or, MLIB_OR(0, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_or, MLIB_OR(1, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_or, MLIB_OR(1, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_or, !MLIB_OR(0, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_and, !MLIB_AND(0, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_and, !MLIB_AND(1, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_and, MLIB_AND(1, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_and, !MLIB_AND(0, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor, !MLIB_XOR(0, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor, MLIB_XOR(1, 0));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor, MLIB_XOR(0, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor, !MLIB_XOR(1, 1));
// Empty operands normalize to `0`, exercising the empty spelling through the
// binary operators:
SUPER_SIMPLE_STATIC_ASSERT(bool_or_empty, !MLIB_OR(, ));
SUPER_SIMPLE_STATIC_ASSERT(bool_or_empty, MLIB_OR(, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_and_empty, !MLIB_AND(, 1));
SUPER_SIMPLE_STATIC_ASSERT(bool_xor_empty, MLIB_XOR(, 1));

// if-else.h
SUPER_SIMPLE_STATIC_ASSERT(if_else_1, MLIB_IF_ELSE(1)(1)(0));
SUPER_SIMPLE_STATIC_ASSERT(if_else_0, !MLIB_IF_ELSE(0)(1)(0));
// The condition accepts every spelling that MLIB_BOOLEAN does. The empty
// spelling is falsey. (The `true`/`false` word spellings are intentionally
// untested here for the same reason noted in the boolean.h section above.)
SUPER_SIMPLE_STATIC_ASSERT(if_else_empty, !MLIB_IF_ELSE()(1)(0));
// Branches may be arbitrary token sequences, not just single tokens:
SUPER_SIMPLE_STATIC_ASSERT(if_else_multi, MLIB_IF_ELSE(1)(2 + 3 + 4)(0) == 9);
// The unused branch is discarded entirely: `1 / 0` would be a hard compile
// error (division by zero in a constant expression) if it ever reached the
// compiler, so these pass only if the branch is truly dropped.
SUPER_SIMPLE_STATIC_ASSERT(if_else_drops_else, MLIB_IF_ELSE(1)(7)(1 / 0) == 7);
SUPER_SIMPLE_STATIC_ASSERT(if_else_drops_then, MLIB_IF_ELSE(0)(1 / 0)(7) == 7);
SUPER_SIMPLE_STATIC_ASSERT(if, MLIB_IF(1)(1));
SUPER_SIMPLE_STATIC_ASSERT(if, 42 == MLIB_IF(0)("discarded tokens") 42);
// Empty condition is falsey, so MLIB_IF expands to nothing (dropping `1 / 0`):
SUPER_SIMPLE_STATIC_ASSERT(if_empty, 5 == MLIB_IF()(1 / 0) 5);
SUPER_SIMPLE_STATIC_ASSERT(unless, 1 == MLIB_UNLESS(0)(1));
SUPER_SIMPLE_STATIC_ASSERT(unless, 1 == MLIB_UNLESS(1)() 1);
// Empty condition is falsey, so MLIB_UNLESS expands its content:
SUPER_SIMPLE_STATIC_ASSERT(unless_empty, 42 == MLIB_UNLESS()(42));

// is-empty.h
SUPER_SIMPLE_STATIC_ASSERT(is_empty, MLIB_IS_EMPTY());
SUPER_SIMPLE_STATIC_ASSERT(is_empty, !MLIB_IS_EMPTY(a));
SUPER_SIMPLE_STATIC_ASSERT(is_empty, !MLIB_IS_EMPTY(a, b));
SUPER_SIMPLE_STATIC_ASSERT(is_empty, !MLIB_IS_EMPTY(()));
SUPER_SIMPLE_STATIC_ASSERT(is_not_empty, !MLIB_IS_NOT_EMPTY());
SUPER_SIMPLE_STATIC_ASSERT(is_not_empty, MLIB_IS_NOT_EMPTY(a));
SUPER_SIMPLE_STATIC_ASSERT(is_not_empty, MLIB_IS_NOT_EMPTY(a, ));
SUPER_SIMPLE_STATIC_ASSERT(is_not_empty, MLIB_IS_NOT_EMPTY(()));
SUPER_SIMPLE_STATIC_ASSERT(if_not_empty, MLIB_IF_NOT_EMPTY(a)(1) == 1);
SUPER_SIMPLE_STATIC_ASSERT(if_not_empty, MLIB_IF_NOT_EMPTY()(1) 4 == 4);
SUPER_SIMPLE_STATIC_ASSERT(if_not_empty_multi, MLIB_IF_NOT_EMPTY(a, b, c)(1) == 1);
SUPER_SIMPLE_STATIC_ASSERT(if_not_empty_paren, MLIB_IF_NOT_EMPTY(())(1) == 1);

// MLIB_OPT_COMMA expands to a single comma when non-empty, and to nothing when
// empty. Feeding the result into MLIB_ARG_COUNT proves whether a separating
// comma was produced: `a , b` counts as 2 arguments, `a b` as 1.
SUPER_SIMPLE_STATIC_ASSERT(opt_comma_nonempty, MLIB_ARG_COUNT(a MLIB_OPT_COMMA(x) b) == 2);
SUPER_SIMPLE_STATIC_ASSERT(opt_comma_empty, MLIB_ARG_COUNT(a MLIB_OPT_COMMA() b) == 1);

// MLIB_IS_PARENTHESIZED detects only whether the *first* token is an opening
// parenthesis; trailing tokens are ignored.
SUPER_SIMPLE_STATIC_ASSERT(is_paren_group, MLIB_IS_PARENTHESIZED((a, b)));
SUPER_SIMPLE_STATIC_ASSERT(is_paren_single, MLIB_IS_PARENTHESIZED((foo)));
SUPER_SIMPLE_STATIC_ASSERT(is_paren_trailing, MLIB_IS_PARENTHESIZED((a)b c));
SUPER_SIMPLE_STATIC_ASSERT(is_paren_not, !MLIB_IS_PARENTHESIZED(foo));
// `foo(bar)` does not begin with a parenthesis, so it is not parenthesized:
SUPER_SIMPLE_STATIC_ASSERT(is_paren_call, !MLIB_IS_PARENTHESIZED(foo(bar)));

// args.h
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT() == 0);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT(a) == 1);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT(a, b) == 2);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT(, ) == 2);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT(()) == 1);
SUPER_SIMPLE_STATIC_ASSERT(arg_count, MLIB_ARG_COUNT((a, b)) == 1);
SUPER_SIMPLE_STATIC_ASSERT(arg_count_ten, MLIB_ARG_COUNT(a, b, c, d, e, f, g, h, i, j) == 10);
// clang-format off
// The documented maximum of 63 arguments (bounded by _mlibPick64th):
SUPER_SIMPLE_STATIC_ASSERT(arg_count_max,
                           MLIB_ARG_COUNT(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  10
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  20
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  30
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  40
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  50
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //  60
                                          1, 1, 1) == 63);              //  63
// clang-format on

// MLIB_ARGC_PASTE forms the `<prefix>_argc_<N>` token without invoking it. Here
// it forms `argc_paste_helper_argc_2` (2 arguments), which we define to a value.
#define argc_paste_helper_argc_2 55
SUPER_SIMPLE_STATIC_ASSERT(argc_paste, MLIB_ARGC_PASTE(argc_paste_helper, x, y) == 55);

#define foo_argc_0() 1729
#define foo_argc_1(A) (42 + A)
#define foo(...) MLIB_ARGC_PICK(foo, __VA_ARGS__)
SUPER_SIMPLE_STATIC_ASSERT(pick, foo() == 1729);
SUPER_SIMPLE_STATIC_ASSERT(pick, foo(8) == 50);

mlib_extern_c_begin();
mlib_diagnostic_push();
mlib_diagnostic_pop();
mlib_extern_c_end();

#define DECLARE_VAR(Name, _nil, Counter) int Name = Counter;
MLIB_MAP_MACRO(DECLARE_VAR, @, foo, bar, baz)

#define SUM(N, _nil, _counter) +N
SUPER_SIMPLE_STATIC_ASSERT(sum, MLIB_MAP_MACRO(SUM, ~, 1, 2, 1) == 4);

// An empty list expands to nothing at all, so the surrounding tokens are left
// adjacent: `7 == 7`.
//
// Note the trailing comma: the empty list must be spelled as an explicit empty
// *argument* (`SUM, ~, `) rather than by omitting it (`SUM, ~`). C99 requires
// that the `...` of a variadic macro receive at least one argument; an argument
// consisting of no preprocessing tokens satisfies that, whereas omitting it
// entirely does not, and GCC/Clang reject the omitted form under `-pedantic
// -Werror`. Supporting the omitted form as well is not possible on MSVC's legacy
// preprocessor — see the note on MLIB_MAP_MACRO before trying.
SUPER_SIMPLE_STATIC_ASSERT(map_empty, 7 == MLIB_MAP_MACRO(SUM, ~, ) 7);
// A single-element list still applies the action exactly once:
SUPER_SIMPLE_STATIC_ASSERT(map_single, MLIB_MAP_MACRO(SUM, ~, 5) == 5);

// The "constant" (second) argument is forwarded unchanged to every invocation
// of the action, as the second parameter. Here it multiplies each element:
#define TIMES_K(N, K, _counter) +(N * K)
SUPER_SIMPLE_STATIC_ASSERT(map_constant, MLIB_MAP_MACRO(TIMES_K, 3, 1, 2, 3) == 18);

// The "counter" (third) argument is the zero-based index of each element. For
// four elements the indices are 0, 1, 2, 3, which sum to 6. This also exercises
// the documented representation of the counter as a parenthesized `(0 + 1 ...)`.
#define COUNTER(_n, _k, Counter) +Counter
SUPER_SIMPLE_STATIC_ASSERT(map_counter, MLIB_MAP_MACRO(COUNTER, ~, a, b, c, d) == 6);

// Nested maps: per the warning on MLIB_MAP_MACRO, an inner map must invoke
// `_mlibMapMacro` (not MLIB_MAP_MACRO) so that it is not "painted blue" by the
// outer expansion and the outer MLIB_EVAL is free to drive it. Here the outer
// map iterates {3, 4}; for each outer element the inner map runs over the fixed
// list {p, q} with the outer element as its constant, emitting `+(K)` twice.
// Result: (3 + 3) + (4 + 4) == 14.
#define INNER_ADD(_item, K, _counter) +(K)
#define OUTER_MAP(Elem, _k, _counter) _mlibMapMacro(INNER_ADD, Elem, p, q)
SUPER_SIMPLE_STATIC_ASSERT(map_nested, MLIB_MAP_MACRO(OUTER_MAP, ~, 3, 4) == 14);

// Singleton maps use the final helper immediately, including at nested levels.
#define OUTER_MAP_ONE(Elem, _k, _counter) _mlibMapMacro(INNER_ADD, Elem, p)
SUPER_SIMPLE_STATIC_ASSERT(map_nested_single, MLIB_MAP_MACRO(OUTER_MAP_ONE, ~, 3) == 3);

// Exercise mapping, not just argument counting, at the documented 63-item limit.
// Summing the indices also checks that the final item is expanded exactly once.
#define MAP_MAX_ITEMS                                                                                                \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
SUPER_SIMPLE_STATIC_ASSERT(map_max_count, MLIB_MAP_MACRO(SUM, ~, MAP_MAX_ITEMS) == 63);
SUPER_SIMPLE_STATIC_ASSERT(map_max_counter, MLIB_MAP_MACRO(COUNTER, ~, MAP_MAX_ITEMS) == 1953);

// Each outer element drives a full-length inner map through the same evaluator.
#define OUTER_MAP_MAX(Elem, _k, _counter) _mlibMapMacro(INNER_ADD, Elem, MAP_MAX_ITEMS)
SUPER_SIMPLE_STATIC_ASSERT(map_nested_max, MLIB_MAP_MACRO(OUTER_MAP_MAX, ~, 3, 4) == 441);

/*-
 * Test: Try to use an MLIB_ARGC_PICK macro definition that ultimately expands to
 * the same identifier as the original macro expansion. Macro blue-painting rules
 * state that the final macro should not expand again, but we need to check that
 * here.
 */
int
foo_func(int a)
{
   return a;
}
#define foo_func(...) MLIB_ARGC_PICK(_foo_func, __VA_ARGS__)
#define _foo_func_argc_1(A) foo_func(A)


int
use_argc_func(void)
{
   // Force additional macro expansions, which will detect a faulty ARGC_PICK
   return MLIB_JUST(MLIB_JUST(foo_func(31) == 31));
}
