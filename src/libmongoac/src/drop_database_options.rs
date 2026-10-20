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

use mongodb::options::DropDatabaseOptions;

#[derive(Clone)]
pub struct DropDatabaseOptionsT(DropDatabaseOptions);

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_drop_database_options_destroy(options: *mut DropDatabaseOptionsT) {
    safe_drop!(options);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_drop_database_options_clone(
    options: *const DropDatabaseOptionsT,
) -> *mut DropDatabaseOptionsT {
    safe_into_raw!(safe_as_ref!(options).clone())
}

// TODO

impl From<&DropDatabaseOptionsT> for DropDatabaseOptions {
    fn from(value: &DropDatabaseOptionsT) -> Self {
        value.0.clone()
    }
}
