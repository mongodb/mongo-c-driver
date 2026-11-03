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

use mongodb::results::InsertManyResult;

pub struct InsertManyResultT(InsertManyResult);

// `InsertManyResult` does not implement `Clone`: copy its fields into a default result. This will
// be fixed in version 3.10.0 of the Rust Driver (RUST-2535).
impl Clone for InsertManyResultT {
    fn clone(&self) -> Self {
        let mut result = InsertManyResult::default();
        result.inserted_ids = self.0.inserted_ids.clone();
        Self(result)
    }
}

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

// TODO: get_inserted_ids

impl From<InsertManyResult> for InsertManyResultT {
    fn from(value: InsertManyResult) -> Self {
        Self(value)
    }
}
