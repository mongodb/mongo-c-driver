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
/// fn example(ptr: *mut T)  -> R {
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

/// Safely convert the raw pointer into a reference when not null.
///
/// When `ptr` is null, early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *const T)  -> R {
///     let res: &T = safe_as_ref!(ptr);
///     assert!(!ptr.is_null());
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer to `T`.
/// - `ptr` must not be accessed as mutable concurrently by any other function.
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
