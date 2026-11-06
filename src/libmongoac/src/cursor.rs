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
use crate::client_session::ClientSessionT;
use crate::error::ErrorT;
use crate::future::FutureT;
use crate::private::macros::*;
use crate::runtime::{RuntimeAware, RuntimeT};

use mongodb::bson::RawDocumentBuf;
use mongodb::{Cursor, SessionCursor};

use futures_util::stream::TryStreamExt;

use std::sync::Arc;
use tokio::sync::Mutex;

#[derive(Clone)]
pub struct CursorT {
    state: Arc<Mutex<CursorState>>,
    runtime: RuntimeT,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_cursor_destroy(cursor: *mut CursorT) {
    safe_drop!(cursor);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_cursor_clone(cursor: *const CursorT) -> *mut CursorT {
    safe_into_raw!(safe_as_ref!(cursor).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_cursor_next(cursor: *mut CursorT, error: *mut ErrorT) -> BsonT {
    let error = safe_optional_error_as_mut!(error);
    let cursor = safe_as_mut_with_error!(cursor, error);

    safe_result!(cursor.next(), error).map_or(BsonT::default(), BsonT::from)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_cursor_next_async(cursor: *mut CursorT) -> *mut FutureT {
    let cursor = safe_as_mut!(cursor);

    safe_into_raw!(cursor.next_async())
}

impl CursorT {
    fn next(&mut self) -> Result<Option<RawDocumentBuf>, ErrorT> {
        self.runtime.block_on(async {
            // This lock acquisition is unlikely to be contended given normal usage patterns.
            self.state.lock().await.next().await
        })
    }

    fn next_async(&mut self) -> FutureT {
        let state = self.state.clone();

        self.spawn::<Option<RawDocumentBuf>>(async move {
            // This lock acquisition is unlikely to be contended given normal usage patterns.
            state.lock().await.next().await
        })
    }

    pub(crate) fn new(cursor: Cursor<RawDocumentBuf>, runtime: RuntimeT) -> Self {
        Self {
            state: Arc::new(Mutex::new(CursorState::Plain { cursor })),
            runtime,
        }
    }

    pub(crate) fn new_with_session(
        cursor: SessionCursor<RawDocumentBuf>,
        session: ClientSessionT,
        runtime: RuntimeT,
    ) -> Self {
        Self {
            state: Arc::new(Mutex::new(CursorState::Session { cursor, session })),
            runtime,
        }
    }
}

impl RuntimeAware for CursorT {
    fn runtime(&self) -> &RuntimeT {
        &self.runtime
    }
}

enum CursorState {
    Plain {
        cursor: Cursor<RawDocumentBuf>,
    },
    Session {
        cursor: SessionCursor<RawDocumentBuf>,
        session: ClientSessionT,
    },
}

impl CursorState {
    async fn next(&mut self) -> Result<Option<RawDocumentBuf>, ErrorT> {
        match self {
            CursorState::Plain { cursor } => cursor.try_next().await.map_err(Into::into),
            CursorState::Session { cursor, session } => cursor
                .stream(&mut *session.lock().await)
                .try_next()
                .await
                .map_err(Into::into),
        }
    }
}

#[macro_export]
macro_rules! cursor_session_op {
    ($op:expr, $session:expr, $runtime:expr) => {{
        let op = $op;
        let session = $session;
        let runtime = $runtime;
        match session {
            Some(session) => op
                .session(&mut *session.lock().await)
                .await
                .map_err($crate::error::ErrorT::from)
                .map(|cursor| {
                    CursorT::new_with_session(cursor.with_type(), session.clone(), runtime)
                }),
            None => op
                .await
                .map_err($crate::error::ErrorT::from)
                .map(|cursor| CursorT::new(cursor.with_type(), runtime)),
        }
    }};
}
