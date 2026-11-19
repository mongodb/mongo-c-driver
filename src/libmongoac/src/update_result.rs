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
use mongodb::results::UpdateResult;

#[derive(Clone)]
pub struct UpdateResultT(UpdateResult);

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_update_result_destroy(result: *mut UpdateResultT) {
    safe_drop!(result);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_update_result_clone(result: *const UpdateResultT) -> *mut UpdateResultT {
    safe_into_raw!(safe_as_ref!(result).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_update_result_get_matched_count(
    result: *const UpdateResultT,
    error: *mut ErrorT,
) -> u64 {
    let error = safe_optional_error_as_mut!(error);
    let result = safe_as_ref_with_error!(result, error);

    result.0.matched_count
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_update_result_get_modified_count(
    result: *const UpdateResultT,
    error: *mut ErrorT,
) -> u64 {
    let error = safe_optional_error_as_mut!(error);
    let result = safe_as_ref_with_error!(result, error);

    result.0.modified_count
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_update_result_get_upserted_id(
    result: *const UpdateResultT,
    error: *mut ErrorT,
) -> BsonT {
    let error = safe_optional_error_as_mut!(error);
    let result = safe_as_ref_with_error!(result, error);

    let Some(id) = &result.0.upserted_id else {
        return BsonT::default();
    };

    let doc = mongodb::bson::doc! { "v": id.clone() };
    safe_result!(RawDocumentBuf::try_from(doc), error).into()
}

impl From<UpdateResult> for UpdateResultT {
    fn from(value: UpdateResult) -> Self {
        Self(value)
    }
}
