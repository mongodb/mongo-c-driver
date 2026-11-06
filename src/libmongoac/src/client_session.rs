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

use mongodb::ClientSession;
use mongodb::bson::{Document, RawDocumentBuf};

use std::sync::Arc;
use tokio::sync::Mutex;

#[derive(Clone)]
pub struct ClientSessionT {
    state: Arc<Mutex<ClientSession>>,
    id: Document, // Avoid blocking getter on `self.state.lock()`.
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_session_destroy(session: *mut ClientSessionT) {
    safe_drop!(session);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_session_clone(
    session: *const ClientSessionT,
) -> *mut ClientSessionT {
    safe_into_raw!(safe_as_ref!(session).clone())
}

// Do not implement `get_client()`: dubious value.

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_session_get_id(
    session: *const ClientSessionT,
    error: *mut ErrorT,
) -> BsonT {
    let error = safe_optional_error_as_mut!(error);
    let session = safe_as_ref_with_error!(session, error);

    safe_result!(RawDocumentBuf::try_from(&session.id), error).into()
}

// TODO: start_transaction
// TODO: advance_cluster_time
// TODO: advance_operation_time
// TODO: cluster_time
// TODO: commit_transaction
// TODO: operation_time
// TODO: snapshot_time

impl ClientSessionT {
    #[must_use]
    pub fn new(session: ClientSession) -> Self {
        let id = session.id().clone(); // Invariant: value never changes.
        let state = Arc::new(Mutex::new(session));

        Self { state, id }
    }

    /// This lock acquisition is unlikely to be contended given normal usage patterns.
    pub(crate) async fn lock(&self) -> tokio::sync::MutexGuard<'_, ClientSession> {
        self.state.lock().await
    }
}

#[macro_export]
macro_rules! session_op {
    ($op:expr, $session:expr) => {{
        let session = $session;
        match session {
            Some(session) => $op
                .session(&mut *session.lock().await)
                .await
                .map_err($crate::error::ErrorT::from),
            None => $op.await.map_err($crate::error::ErrorT::from),
        }
    }};
}
