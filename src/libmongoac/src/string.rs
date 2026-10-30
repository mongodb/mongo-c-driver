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

use crate::private::macros::*;
use crate::private::safety::{AsSafeRef, AsSafeRefs};

use std::ffi::c_char;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct StringViewT {
    pub ptr: *const c_char,
    pub len: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct StringViewArrayT {
    pub ptr: *const StringViewT,
    pub len: usize,
}

#[repr(C)]
#[derive(Default)]
pub struct StringT {
    pub ptr: *const c_char,
    pub len: usize,
}

#[repr(C)]
#[derive(Default)]
pub struct StringArrayT {
    pub ptr: *const StringT,
    pub len: usize,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_string_destroy(string: StringT) {
    safe_slice_drop!(string.ptr as *mut u8, string.len);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_string_array_destroy(array: StringArrayT) {
    safe_array_drop!(array, mongoac_string_destroy);
}

impl From<&str> for StringViewT {
    fn from(s: &str) -> Self {
        StringViewT {
            ptr: s.as_ptr().cast(),
            len: s.len(),
        }
    }
}

impl AsSafeRef for StringViewT {
    type Element = u8;
    type T = str;
    type Error = std::str::Utf8Error;

    const TYPE_NAME: &'static str = "UTF-8";

    fn fields(&self) -> (*const u8, usize) {
        (self.ptr.cast(), self.len)
    }

    fn validate(bytes: &[Self::Element]) -> Result<&Self::T, Self::Error> {
        std::str::from_utf8(bytes)
    }
}

impl AsSafeRefs for StringViewArrayT {
    type T = StringViewT;

    fn fields(&self) -> (*const Self::T, usize) {
        (self.ptr, self.len)
    }
}

impl From<String> for StringT {
    fn from(s: String) -> Self {
        let raw = safe_slice_into_raw!(s.into_bytes());

        StringT {
            ptr: raw.cast(),
            len: raw.len(),
        }
    }
}

impl From<Vec<String>> for StringArrayT {
    fn from(strings: Vec<String>) -> Self {
        let elements: Vec<StringT> = strings.into_iter().map(Into::into).collect();
        let raw = safe_slice_into_raw!(elements);

        StringArrayT {
            ptr: raw.cast(),
            len: raw.len(),
        }
    }
}
