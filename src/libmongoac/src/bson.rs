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

use mongodb::bson::RawDocumentBuf;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct BsonViewT {
    pub ptr: *const u8,
    pub len: usize,
}

#[repr(C)]
#[derive(Default)]
pub struct BsonT {
    pub ptr: *const u8,
    pub len: usize,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_bson_destroy(bson: BsonT) {
    safe_slice_drop!(bson.ptr.cast_mut(), bson.len);
}

impl From<&RawDocumentBuf> for BsonViewT {
    fn from(doc: &RawDocumentBuf) -> Self {
        let bytes = doc.as_bytes();

        BsonViewT {
            ptr: bytes.as_ptr(),
            len: bytes.len(),
        }
    }
}
