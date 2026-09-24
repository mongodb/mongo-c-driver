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
/// - `ptr` must either be null or a valid pointer obtained by `Box<T>::into_raw()`.
/// - `ptr` must be exclusively owned by the current function when not null.
#[macro_export]
macro_rules! safe_drop {
    ($ptr:expr) => {{
        let ptr = $ptr;
        if !ptr.is_null() {
            unsafe { drop(Box::from_raw(ptr)) }
        }
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
