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

use crate::bson::BsonT;
use crate::error::ErrorT;
use crate::private::macros::*;

use mongodb::bson::{Document, RawDocumentBuf};
use mongodb::results::InsertOneResult;

#[derive(Clone)]
pub struct InsertOneResultT(InsertOneResult);

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_insert_one_result_destroy(result: *mut InsertOneResultT) {
    safe_drop!(result);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_insert_one_result_clone(
    result: *const InsertOneResultT,
) -> *mut InsertOneResultT {
    safe_into_raw!(safe_as_ref!(result).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_insert_one_result_get_inserted_id(
    result: *const InsertOneResultT,
    error: *mut ErrorT,
) -> BsonT {
    let error = safe_optional_error_as_mut!(error);
    let result = safe_as_ref_with_error!(result, error);

    let mut doc = Document::new();
    doc.insert("v", result.0.inserted_id.clone());

    safe_result!(RawDocumentBuf::try_from(doc).map_err(ErrorT::from), error).into()
}

impl From<InsertOneResult> for InsertOneResultT {
    fn from(value: InsertOneResult) -> Self {
        Self(value)
    }
}
