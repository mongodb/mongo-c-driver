/**
 * @file bson/validate.c
 * @brief Implementation of BSON document validation
 * @date 2025-05-28
 *
 * This file implements the backend for the `bson_validate` family of functions.
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

#include <bson/bson.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

/**
 * @brief User parameter's for validation behavior. These correspond to the various
 * flags that can be given when the user requests validation
 */
typedef struct {
   /**
    * @brief Should we allow invalid UTF-8 in string components?
    *
    * This affects the behavior of validation of key strings and string-like
    * elements that require UTF-8 encoding.
    *
    * Technically invalid UTF-8 is invalid in BSON, but applications may already
    * rely on this working.
    */
   bool allow_invalid_utf8;
   /// Should we allow element key strings to be empty?
   bool allow_empty_keys;
   /**
    * @brief Should we allow a zero-valued codepoint in text?
    *
    * Unicode U+0000 is a valid codepoint, but a lot of software doesn't like
    * it and handles it poorly. By default, we reject it, but the user may
    * want to allow it.
    *
    * Note that because element keys rely on null termination, element keys
    * cannot contain U+0000 by construction.
    */
   bool allow_null_in_utf8;
   /// Should we allow ASCII dot "." in element key strings?
   bool allow_dot_in_keys;
   /**
    * @brief Check for special element keys that begin with an ASCII dollar "$"
    *
    * By default, we ignore them and treat them as regular elements. If this is
    * enabled, we reject key strings that start with a dollar, unless it is a
    * special extended JSON DBref document.
    */
   bool check_special_dollar_keys;
} validation_params;

/**
 * @brief State for a validator.
 */
typedef struct {
   /// The parameters that control validation behavior
   const validation_params *params;
   /// Error storage that is updated if any validation encounters an error
   bson_error_t error;
   /// The zero-based index of the byte where validation stopped in case of an error.
   size_t error_offset;
} validator;

/**
 * @brief Check that the given condition is satisfied, or set an error and return `false`
 *
 * @param Condition The condition that should evaluate to `true`
 * @param Offset The byte offset where an error should be indicated.
 * @param Code The error code that should be set if the condition fails
 * @param ... The error string and format arguments to be used in the error message
 *
 * This macro assumes a `validator* self` is in scope. This macro will evaluate `return false`
 * if the given condition is not true.
 */
#define require_with_error(Condition, Offset, Code, ...)                    \
   if (!(Condition)) {                                                      \
      self->error_offset = (Offset);                                        \
      bson_set_error (&self->error, BSON_ERROR_INVALID, Code, __VA_ARGS__); \
      return false;                                                         \
   } else                                                                   \
      ((void) 0)

/**
 * @brief Check that the given condition is satisfied, or `return false` immediately.
 */
#define require(Cond) \
   if (!(Cond)) {     \
      return false;   \
   } else             \
      ((void) 0)

/**
 * @brief Advance the pointed-to iterator, check for errors, and test whether we are done.
 *
 * @param DoneVar An l-value of type `bool` that is set to `true` if the iterator hit the end of
 * the document, otherwise `false`
 * @param IteratorPointer An expression of type `bson_iter_t*`, which will be advanced.
 *
 * If advancing the iterator resulting in a decoding error, then this macro sets an error
 * on the `validator* self` that is in scope and will immediately `return false`.
 */
#define require_advance(DoneVar, IteratorPointer)                                                       \
   if ((DoneVar = !bson_iter_next (IteratorPointer))) {                                                 \
      /* The iterator indicates that it stopped */                                                      \
      if ((IteratorPointer)->err_off) {                                                                 \
         /* The iterator stopped because of a decoding error */                                         \
         require_with_error (false, (IteratorPointer)->err_off, BSON_VALIDATE_CORRUPT, "corrupt BSON"); \
      }                                                                                                 \
   } else                                                                                               \
      ((void) 0)

// Test if the element's key is equal to the given string
static bool
_key_is (bson_iter_t const *iter, const char *const key)
{
   return !strcmp (bson_iter_key (iter), key);
}

/**
 * @brief Validate a document or array object, recursively.
 *
 * @param self The validator which will be updated and used to do the validation
 * @param bson The object to be validated
 * @return true If the object is valid
 * @return false Otherwise
 */
static bool
_validate_doc (validator *self, const bson_t *bson);

/**
 * @brief Validate a UTF-8 string, if-and-only-if UTF-8 validation is requested
 *
 * @param self Pointer to the validator object
 * @param offset The byte-offset of the string, used to set the error offset
 * @param u8 Pointer to the first byte in a UTF-8 string
 * @param u8len The length of the array pointed-to by `u8`
 * @return true If the UTF-8 string is valid, or if UTF-8 validation is disabled
 * @return false If UTF-8 validation is requested, AND (the UTF-8 string is invalid OR (UTF-8 strings should not contain
 * null characters and the UTf-8 string contains a null character))
 */
static bool
_maybe_validate_u8 (validator *self, size_t offset, const char *u8, size_t u8len)
{
   if (self->params->allow_invalid_utf8) {
      // We are not doing UTF-8 checks, so always succeed
      return true;
   }
   // Validate UTF-8
   const bool u8okay = bson_utf8_validate (u8, u8len, self->params->allow_null_in_utf8);
   require_with_error (u8okay, offset, BSON_VALIDATE_UTF8, "Invalid UTF-8 string");
   return true;
}

// Same as `_maybe_validate_u8`, but relies on a null-terminated C string to get the string length
static bool
_maybe_validate_u8_cstring (validator *self, size_t offset, const char *const u8)
{
   return _maybe_validate_u8 (self, offset, u8, strlen (u8));
}

// Validate a UTF-8 element. Asserts that the given element is a UTF-8 element!
static bool
_validate_utf8_elem (validator *self, bson_iter_t const *iter)
{
   assert (BSON_ITER_HOLDS_UTF8 (iter));
   uint32_t u8len;
   const char *const u8 = bson_iter_utf8 (iter, &u8len);
   assert (u8);
   return _maybe_validate_u8 (self, iter->off, u8, u8len);
}


static bool
_validate_symbol_elem (validator *self, bson_iter_t const *iter)
{
   assert (BSON_ITER_HOLDS_SYMBOL (iter));
   uint32_t u8len;
   const char *const u8 = bson_iter_symbol (iter, &u8len);
   assert (u8);
   return _maybe_validate_u8 (self, iter->off, u8, u8len);
}

static bool
_validate_code_elem (validator *self, bson_iter_t const *iter)
{
   assert (BSON_ITER_HOLDS_CODE (iter));
   uint32_t u8len;
   const char *const u8 = bson_iter_code (iter, &u8len);
   assert (u8);
   return _maybe_validate_u8 (self, iter->off, u8, u8len);
}


static bool
_validate_dbpointer_elem (validator *self, bson_iter_t const *iter)
{
   assert (BSON_ITER_HOLDS_DBPOINTER (iter));
   uint32_t u8len;
   const char *u8;
   bson_iter_dbpointer (iter, &u8len, &u8, NULL);
   assert (u8);
   return _maybe_validate_u8 (self, iter->off, u8, u8len);
}

static bool
_validate_regex_elem (validator *self, bson_iter_t const *iter)
{
   assert (BSON_ITER_HOLDS_REGEX (iter));
   const char *opts;
   const char *const rx = bson_iter_regex (iter, &opts);
   assert (rx);
   assert (opts);
   return _maybe_validate_u8_cstring (self, iter->off, rx) //
          && _maybe_validate_u8_cstring (self, iter->off, opts);
}

static bool
_validate_codewscope_elem (validator *self, bson_iter_t const *iter)
{
   assert (BSON_ITER_HOLDS_CODEWSCOPE (iter));
   // Extract the code and the scope object
   uint8_t const *doc;
   uint32_t doc_len;
   uint32_t u8len;
   const char *const u8 = bson_iter_codewscope (iter, &u8len, &doc_len, &doc);
   bson_t scope;
   require_with_error (
      bson_init_static (&scope, doc, doc_len), iter->off, BSON_VALIDATE_CORRUPT, "corrupt scope document");

   // Validate the code string
   require (_maybe_validate_u8 (self, iter->off, u8, u8len));

   // Now we validate the scope object.
   // Don't validate the scope document using the parent parameters, because it should
   // be treated as an opaque closure of JS variables.
   validation_params const scope_params = {
      // JS vars cannot contain dots
      .allow_dot_in_keys = false,
      // JS vars cannot be empty
      .allow_empty_keys = false,
      // JS strings can contain null bytes
      .allow_null_in_utf8 = true,
      // JS strings need to encode properly
      .allow_invalid_utf8 = false,
      // JS allows variables to have dollars
      .check_special_dollar_keys = false,
   };
   validator scope_validator = {.params = &scope_params};
   const bool scope_okay = _validate_doc (&scope_validator, &scope);
   if (!scope_okay) {
      // Copy the error message, adding the name of the bad element
      bson_set_error (&self->error,
                      scope_validator.error.domain,
                      scope_validator.error.code,
                      "Error in scope document for element \"%s\": %s",
                      bson_iter_key (iter),
                      scope_validator.error.message);
      // Adjust the error offset by the offset of the iterator
      self->error_offset = scope_validator.error_offset + iter->off;
   }
   return scope_okay;
}

// Validate an element's key string according to the valiation rules
static bool
_validate_element_key (validator *self, bson_iter_t const *iter)
{
   const char *const key = bson_iter_key (iter);
   assert (key);

   // Check the UTF-8 of the key
   require (_maybe_validate_u8_cstring (self, iter->off, key));

   // Check for special keys
   if (self->params->check_special_dollar_keys) {
      // dollar-keys are checked during the startup of _validate_doc. If we get here, there's a problem.
      require_with_error (key[0] != '$', iter->off, BSON_VALIDATE_DOLLAR_KEYS, "Disallowed element key: \"%s\"", key);
   }

   if (!self->params->allow_empty_keys) {
      require_with_error (
         strlen (key) != 0, iter->off, BSON_VALIDATE_EMPTY_KEYS, "Element key cannot be an empty string");
   }

   if (!self->params->allow_dot_in_keys) {
      require_with_error (!strstr (key, "."), iter->off, BSON_VALIDATE_DOT_KEYS, "Disallowed element key: \"%s\"", key);
   }

   return true;
}

// Validate the value of an element, without checking its key
static bool
_validate_element_value (validator *self, bson_iter_t const *iter)
{
   const bson_type_t type = bson_iter_type (iter);
   switch (type) {
   default:
   case BSON_TYPE_EOD:
      assert (false && "Unreachable");
   case BSON_TYPE_DOUBLE:
   case BSON_TYPE_NULL:
   case BSON_TYPE_OID:
   case BSON_TYPE_INT32:
   case BSON_TYPE_INT64:
   case BSON_TYPE_MINKEY:
   case BSON_TYPE_MAXKEY:
   case BSON_TYPE_TIMESTAMP:
   case BSON_TYPE_UNDEFINED:
   case BSON_TYPE_DECIMAL128:
   case BSON_TYPE_DATE_TIME:
      // No validation on these simple scalar elements
      return true;
   case BSON_TYPE_UTF8:
      return _validate_utf8_elem (self, iter);

   case BSON_TYPE_ARRAY: {
      const uint8_t *data;
      uint32_t doclen;
      bson_iter_array (iter, &doclen, &data);
      bson_t doc;
      require_with_error (bson_init_static (&doc, data, doclen),
                          iter->off,
                          BSON_VALIDATE_CORRUPT,
                          "Invalid array \"%s\": corrupt BSON",
                          bson_iter_key (iter));
      return _validate_doc (self, &doc);
   }
   case BSON_TYPE_DOCUMENT: {
      const uint8_t *data;
      uint32_t doclen;
      bson_iter_document (iter, &doclen, &data);
      bson_t doc;
      require_with_error (bson_init_static (&doc, data, doclen),
                          iter->off,
                          BSON_VALIDATE_CORRUPT,
                          "Invalid subdocument \"%s\": corrupt BSON",
                          bson_iter_key (iter));
      return _validate_doc (self, &doc);
   }
   case BSON_TYPE_BINARY:
      // Note: BSON binary validation is handled by bson_iter_next, which checks the
      // internal structure properly. If we get here, then the binary data is okay.
      return true;
   case BSON_TYPE_BOOL:
      // Note: Boolean validation is checked by bson_iter_next, and is indicated as
      // corruption.
      return true;
   case BSON_TYPE_DBPOINTER:
      return _validate_dbpointer_elem (self, iter);
   case BSON_TYPE_REGEX:
      return _validate_regex_elem (self, iter);
   case BSON_TYPE_CODEWSCOPE:
      return _validate_codewscope_elem (self, iter);
   case BSON_TYPE_SYMBOL:
      return _validate_symbol_elem (self, iter);
   case BSON_TYPE_CODE:
      return _validate_code_elem (self, iter);
   }

   return true;
}

// Validate a single BSON element referred-to by the given iterator
static bool
_validate_element (validator *self, bson_iter_t *iter)
{
   return _validate_element_key (self, iter) && _validate_element_value (self, iter);
}

/**
 * @brief Validate the elements of a document, beginning with the element pointed-to
 * by the given iterator.
 */
static bool
_validate_remaining_elements (validator *self, bson_iter_t *iter)
{
   bool done = false;
   while (!done) {
      require (_validate_element (self, iter));
      require_advance (done, iter);
   }
   return true;
}

// Do validation for a DBRef document, indicated by a leading $ref key
static bool
_validate_dbref (validator *self, bson_iter_t *iter)
{
   // The iterator must be pointing to the initial $ref element
   assert (_key_is (iter, "$ref"));
   // Check that $ref is a UTF-8 element
   require_with_error (
      BSON_ITER_HOLDS_UTF8 (iter), iter->off, BSON_VALIDATE_DOLLAR_KEYS, "$ref element must be a UTF-8 element");
   require (_validate_element_value (self, iter));

   // We require an $id as the next element
   bool done;
   require_advance (done, iter);
   require_with_error (
      !done && _key_is (iter, "$id"), iter->off, BSON_VALIDATE_DOLLAR_KEYS, "Expected an $id element following $ref");
   require (_validate_element_value (self, iter));

   // We should stop, or we should have a $db, or we may have other elements
   require_advance (done, iter);
   if (done) {
      // No more elements. Nothing left to check
      return true;
   }
   // If it's a $db, check that it's a UTF-8 string
   if (_key_is (iter, "$db")) {
      require_with_error (BSON_ITER_HOLDS_UTF8 (iter),
                          iter->off,
                          BSON_VALIDATE_DOLLAR_KEYS,
                          "$db element in DBref must be a UTF-8 element");
      require (_validate_element_value (self, iter));
      // Advance past the $db
      require_advance (done, iter);
      if (done) {
         // Nothing left to do
         return true;
      }
   }
   // All subsequent elements should be validated as normal, and we don't expect
   // any more $-keys
   return _validate_remaining_elements (self, iter);
}

// If we are validating special $-keys, validate a document whose first element is a $-key
static bool
_validate_dollar_doc (validator *self, bson_iter_t *iter)
{
   if (_key_is (iter, "$ref")) {
      return _validate_dbref (self, iter);
   }
   // Have the element key validator issue an error message about the bad $-key
   bool okay = _validate_element_key (self, iter);
   assert (!okay);
   return false;
}

static bool
_validate_doc (validator *self, const bson_t *bson)
{
   bson_iter_t iter;
   require_with_error (bson_iter_init (&iter, bson), 0, BSON_VALIDATE_CORRUPT, "Unable to initialize iterator");
   bool done;
   require_advance (done, &iter);
   if (done) {
      // Nothing to check
      return true;
   }
   // Check if the first key starts with a dollar
   if (self->params->check_special_dollar_keys) {
      const char *const key = bson_iter_key (&iter);
      if (key[0] == '$') {
         return _validate_dollar_doc (self, &iter);
      }
   }

   return _validate_remaining_elements (self, &iter);
}

// This private function is called by `bson_validate_with_error_and_offset`
bool
_bson_validate_impl_v2 (const bson_t *bson, bson_validate_flags_t flags, size_t *offset, bson_error_t *error)
{
   // Clear the error
   *error = (bson_error_t){0};

   // Initialize validation parameters
   validation_params const params = {
      .allow_invalid_utf8 = !(flags & BSON_VALIDATE_UTF8),
      .allow_null_in_utf8 = flags & BSON_VALIDATE_UTF8_ALLOW_NULL,
      .check_special_dollar_keys = (flags & BSON_VALIDATE_DOLLAR_KEYS),
      .allow_dot_in_keys = !(flags & BSON_VALIDATE_DOT_KEYS),
      .allow_empty_keys = !(flags & BSON_VALIDATE_EMPTY_KEYS),
   };

   // Start the validator on the root document
   validator v = {0};
   v.params = &params;
   bool okay = _validate_doc (&v, bson);
   *offset = v.error_offset;
   *error = v.error;
   assert (okay == (v.error.code == 0) &&
           "Validation routine should return `false` if-and-only-if it sets an error code");
   return okay;
}
