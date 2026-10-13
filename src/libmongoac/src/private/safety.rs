// Copyright 2009-present MongoDB, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Define safety macros for use by C-layer functions.
//!
//! All `unsafe` blocks required to validate-and-convert unsafe C types into safe Rust types are encapsulated by
//! safety macros defined in this crate and exported by `crate::private::macros`.

/// Safely drop the raw pointer when not null.
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *mut T) {
///     safe_drop!(ptr);
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer obtained by `Box<T>::into_raw()` (e.g. via `safe_into_raw!`).
/// - `ptr` must be exclusively owned by the current function when not null.
#[macro_export]
macro_rules! safe_drop {
    ($ptr:expr) => {{
        let ptr = $ptr;
        if !ptr.is_null() {
            unsafe {
                drop(Box::from_raw(ptr));
            }
        }
    }};
}

/// Safely drop the raw array pointer when not null.
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *mut T, len: usize) {
///     safe_slice_drop!(ptr, len);
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer obtained by `Box<[T]>::into_raw()` (e.g. via `safe_slice_into_raw!`).
/// - When `ptr` is not null, `ptr` must be exclusively owned by the current function.
/// - When `ptr` is not null, `len` must equal the length of the original boxed slice.
#[macro_export]
macro_rules! safe_slice_drop {
    ($ptr:expr, $len:expr) => {{
        let ptr = $ptr;
        let len = $len;
        if !ptr.is_null() {
            unsafe {
                drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(ptr, len)));
            }
        }
    }};
}

/// Safely convert the given value into a raw owning pointer.
///
/// Usage:
///
/// ```rust
/// fn example(v: T) -> *mut T {
///     safe_into_raw!(v)
/// }
/// ```
///
/// Postconditions:
///
/// - The raw owning pointer is not null.
/// - The raw owning pointer is exclusively owned by the current function.
#[macro_export]
macro_rules! safe_into_raw {
    ($v:expr) => {{
        let v = $v;
        Box::into_raw(Box::new(v))
    }};
}

/// Safely convert the given slice into a raw owning array pointer.
///
/// Usage:
///
/// ```rust
/// fn example(v: &[T]) -> *mut [T] {
///     safe_slice_into_raw!(v)
/// }
/// ```
///
/// Postconditions:
///
/// - The raw owning array pointer is not null.
/// - The raw owning array pointer is exclusively owned by the current function.
#[macro_export]
macro_rules! safe_slice_into_raw {
    ($v:expr) => {{
        let v = $v;
        Box::into_raw(Box::<[_]>::from(v))
    }};
}

/// Safely convert the raw pointer into a mutable reference when not null.
///
/// When `ptr` is null, early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *mut T) -> R {
///     let res: &mut T = safe_as_mut!(ptr);
///     assert!(!ptr.is_null());
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer to `T`.
/// - `ptr` must not be accessed concurrently by any other function.
#[macro_export]
macro_rules! safe_as_mut {
    ($ptr:expr) => {{
        let ptr = $ptr;
        match unsafe { ptr.as_mut() } {
            Some(r) => r,
            None => return Default::default(),
        }
    }};
}

/// Safely return the result of the given expression only when the associated error is unset.
///
/// When `Err(re) = result` and `Some(ee) = error`, assign `re` to `ee` prior to early-return via `return Default::default();`.
/// When `Err(re) = result` and `error` is `None`, early-return via `return Default::default();`.
/// When `Ok(v) = result`, return `v`.
///
/// Usage:
///
/// ```rust
/// fn example(res: Result<T, E>, error: Option<&mut ErrorT>) {
///     let v: T = safe_result!(res, error);
///     if let Some(error) = error {
///         assert_eq!(error.category(), MONGOAC_ERROR_CATEGORY_NONE);
///         assert_eq!(error.code(), MONGOAC_ERROR_CODE_OK);
///     }
/// }
/// ```
///
/// Preconditions:
///
/// - When `Some(ee) = error`, `ee` is in its default state (e.g. via `safe_optional_error_as_mut!`).
/// - When `Err(re) = result`, `re` must be convertible to `ErrorT`.
#[macro_export]
macro_rules! safe_result {
    ($result:expr, $error:expr) => {{
        let result: Result<_, _> = $result;
        match result {
            Ok(v) => v,
            Err(e) => {
                let error: Option<&mut $crate::error::ErrorT> = $error;
                if let Some(error) = error {
                    *error = Into::into(e);
                }
                return Default::default();
            }
        }
    }};
}

/// Safely convert the optional raw error pointer into a mutable reference.
///
/// When `ptr` is null, returns `None`.
/// When `ptr` is not null, the error is unconditionally set to its default state (cleared).
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *mut ErrorT) {
///     let res: Option<&mut $crate::error::ErrorT> = safe_optional_error_as_mut!(ptr);
///     match res {
///         Some(e) => {
///             assert_eq!(e.category(), MONGOAC_ERROR_CATEGORY_NONE);
///             assert_eq!(e.code(), MONGOAC_ERROR_CODE_OK);
///         }
///         None => {
///             assert!(ptr.is_null());
///         }
///     }
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer to `ErrorT`.
/// - `ptr` must not be accessed concurrently by any other function.
#[macro_export]
macro_rules! safe_optional_error_as_mut {
    ($ptr:expr) => {{
        let ptr = $ptr;
        match unsafe { ptr.as_mut() } {
            Some(e) => {
                e.clear();
                Some(e)
            }
            None => None,
        }
    }};
}

/// Safely convert the raw pointer into a reference when not null.
///
/// When `ptr` is null, early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *const T) -> R {
///     let res: &T = safe_as_ref!(ptr);
///     assert!(!ptr.is_null());
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer to `T`.
/// - `ptr` must not be mutably accessed concurrently by any other function.
#[macro_export]
macro_rules! safe_as_ref {
    ($ptr:expr) => {{
        let ptr = $ptr;
        match unsafe { ptr.as_ref() } {
            Some(r) => r,
            None => return Default::default(),
        }
    }};
}

/// Safely convert the raw pointer into a reference when not null.
///
/// When `ptr` is null, set `error` and early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *const T, error: Option<&mut ErrorT>) -> R {
///     let res: &T = safe_as_ref_with_error!(ptr, error);
///     assert!(!ptr.is_null());
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer to `T`.
/// - `ptr` must not be mutably accessed concurrently by any other function.
/// - `error` must be null or in its default state (cleared).
#[macro_export]
macro_rules! safe_as_ref_with_error {
    ($ptr:expr, $error:expr) => {{
        let ptr = $ptr;
        match unsafe { ptr.as_ref() } {
            Some(r) => r,
            None => {
                $crate::private::safety::invalid_argument(
                    $error,
                    format!("{}: must not be null", stringify!($ptr)),
                );
                return Default::default();
            }
        }
    }};
}

/// Safely convert the raw pointer into an optional reference.
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *const T) -> R {
///     let res: Option<&T> = safe_optional_as_ref!(ptr);
///     if res.is_some() {
///         assert!(!ptr.is_null());
///     } else {
///         assert!(ptr.is_null());
///     }
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer to `T`.
/// - `ptr` must not be mutably accessed concurrently by any other function.
#[macro_export]
macro_rules! safe_optional_as_ref {
    ($ptr:expr) => {{
        let ptr = $ptr;
        unsafe { ptr.as_ref() }
    }};
}

/// Safely convert the `StringViewT` into a `str` when not null.
///
/// When `sv.ptr` is null or `sv` contains invalid UTF-8, set `error` and early-return from the function via
/// `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(sv: StringViewT, error: Option<&mut ErrorT>) -> R {
///     let res: &str = safe_string_view_with_error!(sv, error);
///     assert!(!sv.ptr.is_null());
/// }
/// ```
///
/// Preconditions:
///
/// - `sv.ptr` must either be null or a valid pointer to a valid UTF-8 string.
/// - `sv.ptr` must not be mutably accessed concurrently by any other function.
/// - When `sv.ptr` is not null, the range `[sv.ptr, sv.ptr + sv.len)` must be valid and accessible.
/// - `error` must be null or in its default state (cleared).
#[macro_export]
macro_rules! safe_string_view_with_error {
    ($sv:expr, $error:expr) => {{
        let sv: $crate::string::StringViewT = $sv;

        if sv.ptr.is_null() {
            $crate::private::safety::invalid_argument(
                $error,
                format!("{}: must not be null", stringify!($sv)),
            );
            return Default::default();
        }

        match std::str::from_utf8(unsafe {
            std::slice::from_raw_parts(sv.ptr.cast::<u8>(), sv.len)
        }) {
            Ok(sv) => sv,
            Err(e) => {
                $crate::private::safety::invalid_argument(
                    $error,
                    format!("{}: invalid UTF-8: {}", stringify!($sv), e),
                );
                return Default::default();
            }
        }
    }};
}

/// Convenience helper to assign `InvalidArgument` to the given `ErrorT`.
pub(crate) fn invalid_argument<M>(error: Option<&mut crate::error::ErrorT>, msg: M)
where
    M: Into<String>,
{
    use crate::error::ErrorCodeT;
    use crate::error::ErrorT;

    if let Some(error) = error {
        *error = ErrorT::MongoAC {
            code: ErrorCodeT::InvalidArgument,
            message: Some(msg.into()),
        };
    }
}
