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
use crate::client_session::ClientSessionT;
use crate::collection_options::CollectionOptionsT;
use crate::drop_collection_options::DropCollectionOptionsT;
use crate::error::ErrorT;
use crate::estimated_document_count_options::EstimatedDocumentCountOptionsT;
use crate::future::FutureT;
use crate::private::macros::*;
use crate::runtime::{RuntimeAware, RuntimeT};
use crate::string::StringViewT;

use mongodb::Collection;
use mongodb::bson::RawDocumentBuf;
use mongodb::options::DropCollectionOptions;
use mongodb::options::EstimatedDocumentCountOptions;

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
pub extern "C" fn mongoac_collection_drop(
    coll: *const CollectionT,
    session: *mut ClientSessionT,
    options: *const DropCollectionOptionsT,
    error: *mut ErrorT,
) {
    let error = safe_optional_error_as_mut!(error);
    let coll = safe_as_ref_with_error!(coll, error);
    let session = safe_optional_as_mut!(session);
    let options = safe_optional_as_ref!(options);

    safe_result!(coll.drop(session, options), error);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_drop_async(
    coll: *const CollectionT,
    session: *mut ClientSessionT,
    options: *const DropCollectionOptionsT,
) -> *mut FutureT {
    let coll = safe_as_ref!(coll);
    let session = safe_optional_as_mut!(session);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(coll.drop_async(session, options))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_estimated_document_count(
    coll: *const CollectionT,
    options: *const EstimatedDocumentCountOptionsT,
    error: *mut ErrorT,
) -> u64 {
    let error = safe_optional_error_as_mut!(error);
    let coll = safe_as_ref_with_error!(coll, error);
    let options = safe_optional_as_ref!(options);

    safe_result!(coll.estimated_document_count(options), error)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_estimated_document_count_async(
    coll: *const CollectionT,
    options: *const EstimatedDocumentCountOptionsT,
) -> *mut FutureT {
    let coll = safe_as_ref!(coll);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(coll.estimated_document_count_async(options))
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

    fn drop(
        &self,
        session: Option<&mut ClientSessionT>,
        options: Option<&DropCollectionOptionsT>,
    ) -> Result<(), ErrorT> {
        self.runtime.block_on(async {
            session_op!(
                self.inner.drop().with_options(options.map(Into::into)),
                session
            )
        })
    }

    fn drop_async(
        &self,
        session: Option<&mut ClientSessionT>,
        options: Option<&DropCollectionOptionsT>,
    ) -> FutureT {
        let coll = self.inner.clone();
        let session = session.map(|s| s.clone());
        let options: Option<DropCollectionOptions> = options.map(Into::into);

        self.spawn::<()>(async move { session_op!(coll.drop().with_options(options), session) })
    }

    fn estimated_document_count(
        &self,
        options: Option<&EstimatedDocumentCountOptionsT>,
    ) -> Result<u64, ErrorT> {
        self.runtime.block_on(async {
            self.inner
                .estimated_document_count()
                .with_options(options.map(Into::into))
                .await
                .map_err(ErrorT::from)
        })
    }

    fn estimated_document_count_async(
        &self,
        options: Option<&EstimatedDocumentCountOptionsT>,
    ) -> FutureT {
        let coll = self.inner.clone();
        let options: Option<EstimatedDocumentCountOptions> = options.map(Into::into);

        self.spawn::<u64>(async move {
            coll.estimated_document_count()
                .with_options(options)
                .await
                .map_err(ErrorT::from)
        })
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
    // TODO: drop_index
    // TODO: drop_indexes
    // TODO: drop_search_index
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
