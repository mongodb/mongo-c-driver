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

use crate::client::ClientT;
use crate::collection_options::CollectionOptionsT;
use crate::private::macros::*;
use crate::runtime::{RuntimeAware, RuntimeT};
use crate::string::StringViewT;

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
pub extern "C" fn mongoac_collection_get_client(coll: *const CollectionT) -> *mut ClientT {
    safe_into_raw!(safe_as_ref!(coll).get_client())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_get_name(coll: *const CollectionT) -> StringViewT {
    safe_as_ref!(coll).get_name().into()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_get_runtime(coll: *const CollectionT) -> *mut RuntimeT {
    safe_into_raw!(safe_as_ref!(coll).get_runtime())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_get_runtime_address(coll: *const CollectionT) -> usize {
    safe_as_ref!(coll).get_runtime_address()
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

    #[must_use]
    fn get_client(&self) -> ClientT {
        ClientT::new_from_client(self.inner.client(), &self.runtime)
    }

    #[must_use]
    fn get_name(&self) -> &str {
        self.inner.name()
    }

    // TODO: aggregate
    // TODO: count_documents
    // TODO: create_index
    // TODO: create_indexes
    // TODO: create_search_index
    // TODO: create_search_indexes
    // TODO: delete_many
    // TODO: delete_one
    // TODO: distinct
    // TODO: drop
    // TODO: drop_index
    // TODO: drop_indexes
    // TODO: drop_search_index
    // TODO: estimated_document_count
    // TODO: find
    // TODO: find_one
    // TODO: find_one_and_delete
    // TODO: find_one_and_replace
    // TODO: find_one_and_update
    // TODO: insert_many
    // TODO: insert_one
    // TODO: insert_one_model
    // TODO: list_index_names
    // TODO: list_indexes
    // TODO: list_search_indexes
    // TODO: namespace
    // TODO: read_concern
    // TODO: replace_one
    // TODO: replace_one_model
    // TODO: selection_criteria
    // TODO: update_many
    // TODO: update_one
    // TODO: update_search_index
    // TODO: watch
    // TODO: write_concern
}

impl RuntimeAware for CollectionT {
    fn runtime(&self) -> &RuntimeT {
        &self.runtime
    }
}
