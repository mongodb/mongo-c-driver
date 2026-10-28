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

use crate::bson::BsonViewT;
use crate::client::ClientT;
use crate::client_session::ClientSessionT;
use crate::collection_options::CollectionOptionsT;
use crate::count_options::CountOptionsT;
use crate::drop_collection_options::DropCollectionOptionsT;
use crate::error::ErrorT;
use crate::estimated_document_count_options::EstimatedDocumentCountOptionsT;
use crate::future::FutureT;
use crate::insert_many_options::InsertManyOptionsT;
use crate::insert_many_result::InsertManyResultT;
use crate::insert_one_options::InsertOneOptionsT;
use crate::insert_one_result::InsertOneResultT;
use crate::private::macros::*;
use crate::runtime::{RuntimeAware, RuntimeT};
use crate::string::StringViewT;

use mongodb::Collection;
use mongodb::bson::{Document, RawDocument, RawDocumentBuf};
use mongodb::options::{CountOptions, DropCollectionOptions, EstimatedDocumentCountOptions, InsertManyOptions, InsertOneOptions};
use mongodb::results::{InsertManyResult, InsertOneResult};

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
pub extern "C" fn mongoac_collection_count_documents(
    coll: *const CollectionT,
    session: *mut ClientSessionT,
    filter: BsonViewT,
    options: *const CountOptionsT,
    error: *mut ErrorT,
) -> u64 {
    let error = safe_optional_error_as_mut!(error);
    let coll = safe_as_ref_with_error!(coll, error);
    let session = safe_optional_as_mut!(session);
    let filter = safe_bson_view_with_error!(filter, error);
    let options = safe_optional_as_ref!(options);

    safe_result!(coll.count_documents(session, filter, options), error)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_count_documents_async(
    coll: *const CollectionT,
    session: *mut ClientSessionT,
    filter: BsonViewT,
    options: *const CountOptionsT,
) -> *mut FutureT {
    let coll = safe_as_ref!(coll);
    let session = safe_optional_as_mut!(session);
    let filter = safe_bson_view!(filter);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(coll.count_documents_async(session, filter, options))
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
pub extern "C" fn mongoac_collection_insert_many(
    collection: *const CollectionT,
    session: *mut ClientSessionT,
    documents: *const BsonViewT,
    count: usize,
    options: *const InsertManyOptionsT,
    error: *mut ErrorT,
) -> *mut InsertManyResultT {
    let error = safe_optional_error_as_mut!(error);
    let collection = safe_as_ref_with_error!(collection, error);
    let session = safe_optional_as_mut!(session);
    let documents = safe_bson_view_array_as_vec_with_error!(documents, count, error);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(InsertManyResultT::from(safe_result!(
        collection.insert_many(session, documents, options),
        error
    )))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_insert_many_async(
    collection: *const CollectionT,
    session: *mut ClientSessionT,
    documents: *const BsonViewT,
    count: usize,
    options: *const InsertManyOptionsT,
    error: *mut ErrorT,
) -> *mut FutureT {
    let error = safe_optional_error_as_mut!(error);
    let collection = safe_as_ref_with_error!(collection, error);
    let session = safe_optional_as_mut!(session);
    let documents = safe_bson_view_array_as_vec_with_error!(documents, count, error);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(collection.insert_many_async(session, documents, options))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_insert_one(
    collection: *const CollectionT,
    session: *mut ClientSessionT,
    doc: BsonViewT,
    options: *const InsertOneOptionsT,
    error: *mut ErrorT,
) -> *mut InsertOneResultT {
    let error = safe_optional_error_as_mut!(error);
    let collection = safe_as_ref_with_error!(collection, error);
    let session = safe_optional_as_mut!(session);
    let doc = safe_bson_view_with_error!(doc, error);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(safe_result!(collection.insert_one(session, doc, options), error).into())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_collection_insert_one_async(
    collection: *const CollectionT,
    session: *mut ClientSessionT,
    doc: BsonViewT,
    options: *const InsertOneOptionsT,
) -> *mut FutureT {
    let collection = safe_as_ref!(collection);
    let session = safe_optional_as_mut!(session);
    let doc = safe_bson_view!(doc);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(collection.insert_one_async(session, doc, options))
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

    fn count_documents(
        &self,
        session: Option<&mut ClientSessionT>,
        filter: &RawDocument,
        options: Option<&CountOptionsT>,
    ) -> Result<u64, ErrorT> {
        self.runtime.block_on(async {
            let filter = Document::try_from(filter)?;

            session_op!(
                self.inner
                    .count_documents(filter)
                    .with_options(options.map(Into::into)),
                session
            )
        })
    }

    fn count_documents_async(
        &self,
        session: Option<&mut ClientSessionT>,
        filter: &RawDocument,
        options: Option<&CountOptionsT>,
    ) -> FutureT {
        let coll = self.inner.clone();
        let session = session.map(|s| s.clone());
        let filter = filter.to_owned();
        let options: Option<CountOptions> = options.map(Into::into);

        self.spawn::<u64>(async move {
            let filter = Document::try_from(filter)?;

            session_op!(coll.count_documents(filter).with_options(options), session)
        })
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

    fn insert_many(
        &self,
        session: Option<&mut ClientSessionT>,
        documents: Vec<&RawDocument>,
        options: Option<&InsertManyOptionsT>,
    ) -> Result<InsertManyResult, ErrorT> {
        let documents: Vec<RawDocumentBuf> = documents.into_iter().map(ToOwned::to_owned).collect();

        self.runtime.block_on(async {
            session_op!(
                self.inner
                    .insert_many(documents)
                    .with_options(options.map(Into::into)),
                session
            )
        })
    }

    fn insert_many_async(
        &self,
        session: Option<&mut ClientSessionT>,
        documents: Vec<&RawDocument>,
        options: Option<&InsertManyOptionsT>,
    ) -> FutureT {
        let coll = self.inner.clone();
        let session = session.map(|s| s.clone());
        let documents: Vec<RawDocumentBuf> = documents.into_iter().map(ToOwned::to_owned).collect();
        let options: Option<InsertManyOptions> = options.map(Into::into);

        self.spawn::<InsertManyResult>(async move {
            session_op!(coll.insert_many(documents).with_options(options), session)
        })
    }

    fn insert_one(
        &self,
        session: Option<&mut ClientSessionT>,
        doc: &RawDocument,
        options: Option<&InsertOneOptionsT>,
    ) -> Result<InsertOneResult, ErrorT> {
        self.runtime.block_on(async {
            session_op!(
                self.inner
                    .insert_one(doc.to_owned())
                    .with_options(options.map(Into::into)),
                session
            )
        })
    }

    fn insert_one_async(
        &self,
        session: Option<&mut ClientSessionT>,
        doc: &RawDocument,
        options: Option<&InsertOneOptionsT>,
    ) -> FutureT {
        let coll = self.inner.clone();
        let session = session.map(|s| s.clone());
        let doc = doc.to_owned();
        let options: Option<InsertOneOptions> = options.map(Into::into);

        self.spawn::<InsertOneResult>(async move {
            session_op!(coll.insert_one(doc).with_options(options), session)
        })
    }

    // TODO: aggregate
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
