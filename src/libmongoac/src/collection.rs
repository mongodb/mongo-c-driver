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

use crate::collection_options::CollectionOptionsT;
use crate::private::macros::*;
use crate::runtime::{RuntimeAware, RuntimeT};

use mongodb::Collection;
use mongodb::bson::RawDocumentBuf;

#[derive(Clone)]
pub struct CollectionT {
    inner: Collection<RawDocumentBuf>,
    runtime: RuntimeT,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_destroy(coll: *mut CollectionT) {
    safe_drop!(coll);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_clone(coll: *const CollectionT) -> *mut CollectionT {
    safe_into_raw!(safe_as_ref!(coll).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_get_runtime(coll: *const CollectionT) -> *mut RuntimeT {
    safe_into_raw!(safe_as_ref!(coll).get_runtime())
}

impl CollectionT {
    #[must_use]
    pub(crate) fn new(
        db: &mongodb::Database,
        runtime: RuntimeT,
        name: &str,
        options: Option<&CollectionOptionsT>,
    ) -> Self {
        let coll = match options {
            Some(options) => db.collection_with_options::<RawDocumentBuf>(name, options.into()),
            None => db.collection::<RawDocumentBuf>(name),
        };

        CollectionT {
            inner: coll,
            runtime,
        }
    }
}

impl RuntimeAware for CollectionT {
    fn get_runtime(&self) -> RuntimeT {
        self.runtime.clone()
    }
}
