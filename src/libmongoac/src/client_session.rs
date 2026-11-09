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

#[derive(Clone)]
pub struct ClientSessionT;

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_session_destroy(session: *mut ClientSessionT) {
    safe_drop!(session);
}

// TODO

#[macro_export]
macro_rules! session_op {
    ($op:expr, $session:expr) => {{
        let session = $session;
        match session {
            Some(_) => {
                unimplemented!("TODO: CDRIVER-6392")
            }
            None => $op.await.map_err($crate::error::ErrorT::from),
        }
    }};
}
