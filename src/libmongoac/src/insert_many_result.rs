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

use mongodb::bson::RawDocumentBuf;
use mongodb::results::InsertManyResult;

#[derive(Clone)]
pub struct InsertManyResultT(InsertManyResult);

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_insert_many_result_destroy(result: *mut InsertManyResultT) {
    safe_drop!(result);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_insert_many_result_clone(
    result: *const InsertManyResultT,
) -> *mut InsertManyResultT {
    safe_into_raw!(safe_as_ref!(result).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_insert_many_result_get_inserted_ids_get(
    result: *const InsertManyResultT,
    key: usize,
    error: *mut ErrorT,
) -> BsonT {
    let error = safe_optional_error_as_mut!(error);
    let result = safe_as_ref_with_error!(result, error);

    let Some(id) = result.0.inserted_ids.get(&key) else {
        return BsonT::default();
    };

    let doc = mongodb::bson::doc! { "v": id.clone() };
    safe_result!(RawDocumentBuf::try_from(doc), error).into()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_insert_many_result_get_inserted_ids_len(
    result: *const InsertManyResultT,
) -> usize {
    safe_as_ref!(result).0.inserted_ids.len()
}

impl From<InsertManyResult> for InsertManyResultT {
    fn from(value: InsertManyResult) -> Self {
        Self(value)
    }
}
