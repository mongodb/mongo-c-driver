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
//! safety macros defined in this module and exported by `crate::private::macros`.

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

/// Safely drop the owning array and its elements.
///
/// Usage:
///
/// ```rust
/// fn example(array: ExampleArrayT) {
///     safe_array_drop!(array, |element| destroy_example(element));
/// }
/// ```
///
/// Preconditions:
///
/// - `array.ptr` must either be null or a valid pointer to `array.len` elements (e.g. obtained via `safe_slice_into_raw!`).
/// - When `array.ptr` is not null, `array` must be exclusively owned by the current function.
/// - When `array.ptr` is not null, `array.len` must equal the length of the original boxed slice.
/// - `destroy` must take ownership of an element and drop it.
#[macro_export]
macro_rules! safe_array_drop {
    ($array:expr, $destroy:expr) => {{
        let array = $array;
        let destroy = $destroy;

        if !array.ptr.is_null() {
            let elements = unsafe {
                Box::from_raw(std::ptr::slice_from_raw_parts_mut(
                    array.ptr.cast_mut(),
                    array.len,
                ))
            };

            for element in elements {
                destroy(element);
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

/// Safely convert the raw pointer into an optional mutable reference.
///
/// Usage:
///
/// ```rust
/// fn example(ptr: *mut T) -> R {
///     let res: Option<&mut T> = safe_optional_as_mut!(ptr);
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
/// - `ptr` must not be accessed concurrently by any other function.
#[macro_export]
macro_rules! safe_optional_as_mut {
    ($ptr:expr) => {{
        let ptr = $ptr;
        unsafe { ptr.as_mut() }
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
/// When `sv.ptr` is null or `sv` contains invalid UTF-8, early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(sv: StringViewT) -> R {
///     let res: &str = safe_string_view!(sv);
///     assert!(!sv.ptr.is_null());
/// }
/// ```
///
/// Preconditions:
///
/// - `sv.ptr` must either be null or a valid pointer to a valid UTF-8 string.
/// - `sv.ptr` must not be mutably accessed concurrently by any other function.
/// - When `sv.ptr` is not null, the range `[sv.ptr, sv.ptr + sv.len)` must be valid and accessible.
#[macro_export]
macro_rules! safe_string_view {
    ($sv:expr) => {{
        use $crate::private::safety::AsSafeRef as _;

        let sv: $crate::string::StringViewT = $sv;

        match unsafe { sv.as_safe_ref() } {
            Ok(v) => v,
            Err(_) => return Default::default(),
        }
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
        use $crate::private::safety::AsSafeRef as _;

        let sv: $crate::string::StringViewT = $sv;

        match unsafe { sv.as_safe_ref() } {
            Ok(v) => v,
            Err(e) => {
                $crate::private::safety::invalid_argument($error, e.message(stringify!($sv)));
                return Default::default();
            }
        }
    }};
}

/// Safely convert the `BsonViewT` into a `&RawDocument` when not null.
///
/// When `bson.ptr` is null or `bson` contains invalid BSON, early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(bson: BsonViewT) -> R {
///     let res: &RawDocument = safe_bson_view!(bson);
///     assert!(!bson.ptr.is_null());
/// }
/// ```
///
/// Preconditions:
///
/// - `bson.ptr` must either be null or a valid pointer to a valid BSON document.
/// - `bson.ptr` must not be mutably accessed concurrently by any other function.
/// - When `bson.ptr` is not null, the range `[bson.ptr, bson.ptr + bson.len)` must be valid and accessible.
#[macro_export]
macro_rules! safe_bson_view {
    ($bson:expr) => {{
        use $crate::private::safety::AsSafeRef as _;

        let bson: $crate::bson::BsonViewT = $bson;

        match unsafe { bson.as_safe_ref() } {
            Ok(v) => v,
            Err(_) => return Default::default(),
        }
    }};
}

/// Safely convert the `BsonViewT` into a `&RawDocument` when not null.
///
/// When `bson.ptr` is null or `bson` contains invalid BSON, set `error` and early-return from the function via
/// `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(bson: BsonViewT, error: Option<&mut ErrorT>) -> R {
///     let res: &RawDocument = safe_bson_view_with_error!(bson, error);
///     assert!(!bson.ptr.is_null());
/// }
/// ```
///
/// Preconditions:
///
/// - `bson.ptr` must either be null or a valid pointer to a valid BSON document.
/// - `bson.ptr` must not be mutably accessed concurrently by any other function.
/// - When `bson.ptr` is not null, the range `[bson.ptr, bson.ptr + bson.len)` must be valid and accessible.
/// - `error` must be null or in its default state (cleared).
#[macro_export]
macro_rules! safe_bson_view_with_error {
    ($bson:expr, $error:expr) => {{
        use $crate::private::safety::AsSafeRef as _;

        let bson: $crate::bson::BsonViewT = $bson;

        match unsafe { bson.as_safe_ref() } {
            Ok(v) => v,
            Err(e) => {
                $crate::private::safety::invalid_argument($error, e.message(stringify!($bson)));
                return Default::default();
            }
        }
    }};
}

/// Safely convert the `BsonViewArrayT` into a `Vec<&RawDocument>` when not null or empty.
///
/// When `arr.ptr` is null, `arr.len` is zero, an element is null, or an element contains invalid BSON, early-return
/// from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(arr: BsonViewArrayT) -> R {
///     let res: Vec<&RawDocument> = safe_bson_view_array!(arr);
/// }
/// ```
///
/// Preconditions:
///
/// - `arr.ptr` must either be null or a valid pointer to an array of `arr.len` elements.
/// - `arr.ptr` must not be mutably accessed concurrently by any other function.
/// - When `arr.ptr` is not null, the range `[arr.ptr, arr.ptr + arr.len)` must be valid and accessible.
#[macro_export]
macro_rules! safe_bson_view_array {
    ($arr:expr) => {{
        use $crate::private::safety::AsSafeRefs as _;

        let arr: $crate::bson::BsonViewArrayT = $arr;

        match unsafe { arr.as_safe_refs() } {
            Ok(v) => v,
            Err(_) => return Default::default(),
        }
    }};
}

/// Safely convert the `BsonViewArrayT` into a `Vec<&RawDocument>` when not null or empty.
///
/// When `arr.ptr` is null, `arr.len` is zero, an element is null, or an element contains invalid BSON, set
/// `error` and early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(arr: BsonViewArrayT, error: Option<&mut ErrorT>) -> R {
///     let res: Vec<&RawDocument> = safe_bson_view_array_with_error!(arr, error);
/// }
/// ```
///
/// Preconditions:
///
/// - `arr.ptr` must either be null or a valid pointer to an array of `arr.len` elements.
/// - `arr.ptr` must not be mutably accessed concurrently by any other function.
/// - When `arr.ptr` is not null, the range `[arr.ptr, arr.ptr + arr.len)` must be valid and accessible.
/// - `error` must be null or in its default state (cleared).
#[macro_export]
macro_rules! safe_bson_view_array_with_error {
    ($arr:expr, $error:expr) => {{
        use $crate::private::safety::AsSafeRefs as _;

        let arr: $crate::bson::BsonViewArrayT = $arr;

        match unsafe { arr.as_safe_refs() } {
            Ok(v) => v,
            Err(e) => {
                $crate::private::safety::invalid_argument($error, e.message(stringify!($arr)));
                return Default::default();
            }
        }
    }};
}

/// Safely convert the `StringViewArrayT` into a `Vec<&str>` when not null or empty.
///
/// When `arr.ptr` is null, `arr.len` is zero, an element is null, or an element contains invalid UTF-8, early-return
/// from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(arr: StringViewArrayT) -> R {
///     let res: Vec<&str> = safe_string_view_array!(arr);
/// }
/// ```
///
/// Preconditions:
///
/// - `arr.ptr` must either be null or a valid pointer to an array of `arr.len` elements.
/// - `arr.ptr` must not be mutably accessed concurrently by any other function.
/// - When `arr.ptr` is not null, the range `[arr.ptr, arr.ptr + arr.len)` must be valid and accessible.
#[macro_export]
macro_rules! safe_string_view_array {
    ($arr:expr) => {{
        use $crate::private::safety::AsSafeRefs as _;

        let arr: $crate::string::StringViewArrayT = $arr;

        match unsafe { arr.as_safe_refs() } {
            Ok(v) => v,
            Err(_) => return Default::default(),
        }
    }};
}

/// Safely convert the `StringViewArrayT` into a `Vec<&str>` when not null or empty.
///
/// When `arr.ptr` is null, `arr.len` is zero, an element is null, or an element contains invalid UTF-8, set
/// `error` and early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(arr: StringViewArrayT, error: Option<&mut ErrorT>) -> R {
///     let res: Vec<&str> = safe_string_view_array_with_error!(arr, error);
/// }
/// ```
///
/// Preconditions:
///
/// - `arr.ptr` must either be null or a valid pointer to an array of `arr.len` elements.
/// - `arr.ptr` must not be mutably accessed concurrently by any other function.
/// - When `arr.ptr` is not null, the range `[arr.ptr, arr.ptr + arr.len)` must be valid and accessible.
/// - `error` must be null or in its default state (cleared).
#[macro_export]
macro_rules! safe_string_view_array_with_error {
    ($arr:expr, $error:expr) => {{
        use $crate::private::safety::AsSafeRefs as _;

        let arr: $crate::string::StringViewArrayT = $arr;

        match unsafe { arr.as_safe_refs() } {
            Ok(v) => v,
            Err(e) => {
                $crate::private::safety::invalid_argument($error, e.message(stringify!($arr)));
                return Default::default();
            }
        }
    }};
}

/// Define how the raw view type is safely convertible into a `&T`.
pub(crate) trait AsSafeRef: Copy {
    /// The raw element type as returned by [`std::slice::from_raw_parts`].
    type Element;

    /// The safe type to be referenced.
    type T: ?Sized;

    /// The error message to append to [`invalid_argument`].
    type Error: std::fmt::Display;

    /// The name of the type being validated.
    const TYPE_NAME: &'static str;

    /// Return `(self.ptr, self.len)` for raw elements, or `(&self, 1)` for a single value.
    ///
    /// When the raw pointer to a single value is null, return (null, 0).
    fn fields(&self) -> (*const Self::Element, usize);

    /// Safely convert the raw element(s) into a reference.
    fn validate(bytes: &[Self::Element]) -> Result<&Self::T, Self::Error>;

    /// Safely convert the raw type into `&T`.
    ///
    /// When `ptr` is null or an element is invalid, returns [`AsSafeRefError`].
    ///
    /// Preconditions:
    ///
    /// - `ptr` must either be null or a valid pointer to a range of valid [`Self::Element`].
    /// - When `ptr` is not null, the range `[ptr, ptr + len)` must be valid and accessible.
    unsafe fn as_safe_ref<'a>(&self) -> Result<&'a Self::T, AsSafeRefError<Self>>
    where
        Self::Element: 'a,
    {
        let (ptr, len) = self.fields();

        if ptr.is_null() {
            return Err(AsSafeRefError::Null);
        }

        match Self::validate(unsafe { std::slice::from_raw_parts(ptr, len) }) {
            Ok(v) => Ok(v),
            Err(e) => Err(AsSafeRefError::Invalid(e)),
        }
    }
}

/// Define how the raw array type is safely convertible into a `Vec<&T>`.
pub(crate) trait AsSafeRefs: Copy {
    /// The safe type to be referenced.
    type T;

    /// Return (`self.ptr`, `self.len`) for raw elements.
    fn fields(&self) -> (*const Self::T, usize);

    /// Safely convert the raw array type into `Vec<&T>`.
    ///
    /// When `ptr` is null, `len` is zero, an element is null, or an element is invalid, returns [`AsSafeRefsError`].
    ///
    /// Preconditions:
    ///
    /// - `ptr` must either be null or a valid pointer to a range of valid elements convertible to [`Self::T`].
    /// - When `ptr` is not null, the range `[ptr, ptr + len)` must be valid and accessible.
    unsafe fn as_safe_refs<'a>(
        &self,
    ) -> Result<Vec<&'a <Self::T as AsSafeRef>::T>, AsSafeRefsError<Self::T>>
    where
        Self::T: AsSafeRef,
        <Self::T as AsSafeRef>::Element: 'a,
    {
        let (ptr, len) = self.fields();

        if ptr.is_null() {
            return Err(AsSafeRefsError::Null);
        }

        if len == 0 {
            return Err(AsSafeRefsError::Empty);
        }

        let views = unsafe { std::slice::from_raw_parts(ptr, len) };

        let mut elements = Vec::with_capacity(views.len());

        for (index, view) in views.iter().enumerate() {
            match unsafe { view.as_safe_ref() } {
                Ok(v) => elements.push(v),
                Err(AsSafeRefError::Null) => return Err(AsSafeRefsError::NullElement { index }),
                Err(AsSafeRefError::Invalid(error)) => {
                    return Err(AsSafeRefsError::InvalidElement { index, error });
                }
            }
        }

        Ok(elements)
    }
}

/// Validation errors which occur when safely converting a raw type into `&T`.
pub(crate) enum AsSafeRefError<T: AsSafeRef> {
    Null,
    Invalid(T::Error),
}

impl<T: AsSafeRef> AsSafeRefError<T> {
    pub(crate) fn message(&self, arg: &str) -> String {
        match self {
            Self::Null => format!("{arg}: must not be null"),
            Self::Invalid(error) => format!("{arg}: invalid {}: {error}", T::TYPE_NAME),
        }
    }
}

/// Validation errors which occur when safely converting a raw array type into `Vec<&T>`.
pub(crate) enum AsSafeRefsError<T: AsSafeRef> {
    Null,
    Empty,
    NullElement { index: usize },
    InvalidElement { index: usize, error: T::Error },
}

impl<T: AsSafeRef> AsSafeRefsError<T> {
    pub(crate) fn message(&self, param: &str) -> String {
        match self {
            Self::Null => format!("{param}: must not be null"),
            Self::Empty => format!("{param}: must not be empty"),
            Self::NullElement { index } => {
                format!("{param} element at index {index}: must not be null")
            }
            Self::InvalidElement { index, error } => {
                format!(
                    "{param} element at index {index}: invalid {}: {error}",
                    T::TYPE_NAME
                )
            }
        }
    }
}

// Convenience helper to assign `InvalidArgument` to the given `ErrorT`.
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
