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

use std::ffi::c_char;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct StringViewT {
    pub ptr: *const c_char,
    pub len: usize,
}

#[repr(C)]
#[derive(Default)]
pub struct StringT {
    pub ptr: *const c_char,
    pub len: usize,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_string_destroy(string: StringT) {
    if string.ptr.is_null() {
        return;
    }

    // SAFETY: bytes `[0, len)` at `data` MUST be accessible when `data` is not null.
    // SAFETY: `string.ptr` is always allocated as a `Box<[u8]>`.
    safe_drop!(std::ptr::slice_from_raw_parts_mut(
        string.ptr as *mut u8,
        string.len,
    ));
}

impl From<&str> for StringViewT {
    fn from(s: &str) -> Self {
        StringViewT {
            ptr: s.as_ptr().cast::<c_char>(),
            len: s.len(),
        }
    }
}

impl From<String> for StringT {
    fn from(s: String) -> Self {
        let bytes = s.into_bytes();
        let len = bytes.len();

        StringT {
            ptr: Box::into_raw(bytes.into_boxed_slice()).cast::<c_char>(),
            len,
        }
    }
}
