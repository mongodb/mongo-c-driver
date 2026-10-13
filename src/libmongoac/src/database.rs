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
use crate::collection::CollectionT;
use crate::collection_options::CollectionOptionsT;
use crate::database_options::DatabaseOptionsT;
use crate::private::macros::*;
use crate::runtime::{RuntimeAware, RuntimeT};
use crate::string::StringViewT;

use mongodb::Database;

#[derive(Clone)]
pub struct DatabaseT {
    inner: Database,
    runtime: RuntimeT,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_database_destroy(db: *mut DatabaseT) {
    safe_drop!(db);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_database_clone(db: *const DatabaseT) -> *mut DatabaseT {
    safe_into_raw!(safe_as_ref!(db).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_database_get_client(db: *const DatabaseT) -> *mut ClientT {
    safe_into_raw!(safe_as_ref!(db).get_client())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_database_get_collection(
    db: *const DatabaseT,
    name: StringViewT,
    options: *const CollectionOptionsT,
) -> *mut CollectionT {
    let db = safe_as_ref!(db);
    let name = safe_string_view!(name);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(db.get_collection(name, options))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_database_get_name(db: *const DatabaseT) -> StringViewT {
    safe_as_ref!(db).get_name().into()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_database_get_runtime(db: *const DatabaseT) -> *mut RuntimeT {
    safe_into_raw!(safe_as_ref!(db).get_runtime())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_database_get_runtime_address(db: *const DatabaseT) -> usize {
    safe_as_ref!(db).get_runtime_address()
}

impl DatabaseT {
    #[must_use]
    pub(crate) fn new(
        client: &mongodb::Client,
        runtime: RuntimeT,
        name: &str,
        options: Option<&DatabaseOptionsT>,
    ) -> Self {
        let db = match options {
            Some(options) => client.database_with_options(name, options.into()),
            None => client.database(name),
        };

        DatabaseT { inner: db, runtime }
    }

    #[must_use]
    fn get_client(&self) -> ClientT {
        ClientT::new_from_client(self.inner.client(), &self.runtime)
    }

    #[must_use]
    fn get_collection(&self, name: &str, options: Option<&CollectionOptionsT>) -> CollectionT {
        CollectionT::new(&self.inner, self.runtime.clone(), name, options)
    }

    #[must_use]
    fn get_name(&self) -> &str {
        self.inner.name()
    }

    fn get_runtime_address(&self) -> usize {
        self.runtime.address()
    }

    // TODO: aggregate
    // TODO: collection_with_options
    // TODO: create_collection
    // TODO: drop
    // TODO: gridfs_bucket
    // TODO: list_collection_names
    // TODO: list_collections
    // TODO: read_concern
    // TODO: run_command
    // TODO: run_cursor_command
    // TODO: run_raw_command
    // TODO: run_raw_cursor_command
    // TODO: selection_criteria
    // TODO: watch
    // TODO: write_concern
}

impl RuntimeAware for DatabaseT {
    fn get_runtime(&self) -> RuntimeT {
        self.runtime.clone()
    }
}
