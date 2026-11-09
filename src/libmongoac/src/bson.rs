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

use mongodb::bson::{RawDocument, RawDocumentBuf};

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct BsonViewT {
    pub ptr: *const u8,
    pub len: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct BsonViewArrayT {
    pub ptr: *const BsonViewT,
    pub len: usize,
}

#[repr(C)]
#[derive(Default)]
pub struct BsonT {
    pub ptr: *const u8,
    pub len: usize,
}

#[repr(C)]
#[derive(Default)]
pub struct BsonArrayT {
    pub ptr: *const BsonT,
    pub len: usize,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_bson_destroy(bson: BsonT) {
    safe_slice_drop!(bson.ptr.cast_mut(), bson.len);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_bson_array_destroy(array: BsonArrayT) {
    safe_array_drop!(array, mongoac_bson_destroy);
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

impl AsSafeRef for BsonViewT {
    type Element = u8;
    type T = RawDocument;
    type Error = mongodb::bson::error::Error;

    const TYPE_NAME: &'static str = "BSON";

    fn fields(&self) -> (*const u8, usize) {
        (self.ptr, self.len)
    }

    fn validate(bytes: &[Self::Element]) -> Result<&Self::T, mongodb::bson::error::Error> {
        RawDocument::from_bytes(bytes)
    }
}

impl AsSafeRefs for BsonViewArrayT {
    type T = BsonViewT;

    fn fields(&self) -> (*const Self::T, usize) {
        (self.ptr, self.len)
    }
}

impl From<RawDocumentBuf> for BsonT {
    fn from(doc: RawDocumentBuf) -> Self {
        let ret = safe_slice_into_raw!(doc.into_bytes());

        BsonT {
            ptr: ret.cast(),
            len: ret.len(),
        }
    }
}

impl From<Vec<RawDocumentBuf>> for BsonArrayT {
    fn from(documents: Vec<RawDocumentBuf>) -> Self {
        let elements: Vec<BsonT> = documents.into_iter().map(Into::into).collect();
        let raw = safe_slice_into_raw!(elements);

        BsonArrayT {
            ptr: raw.cast(),
            len: raw.len(),
        }
    }
}
