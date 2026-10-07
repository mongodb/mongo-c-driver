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

use crate::error::{ErrorCodeT, ErrorT};
use crate::private::macros::*;
use crate::runtime::RuntimeT;
use crate::string::StringViewT;
use crate::version::{MONGOAC_BUILD_PLATFORM, MONGOAC_VERSION_FULL};

use mongodb::Client;
use mongodb::options::{ClientOptions, DriverInfo};

#[derive(Clone)]
pub struct ClientT {
    inner: Client,
    runtime: RuntimeT,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_destroy(client: *mut ClientT) {
    safe_drop!(client);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_clone(client: *const ClientT) -> *mut ClientT {
    safe_into_raw!(safe_as_ref!(client).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_new(conn_str: StringViewT, error: *mut ErrorT) -> *mut ClientT {
    let error = safe_optional_error_as_mut!(error);
    let conn_str = safe_string_view_with_error!(conn_str, error);

    safe_into_raw!(safe_result!(ClientT::new(conn_str), error))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_get_runtime(client: *const ClientT) -> *mut RuntimeT {
    safe_into_raw!(safe_as_ref!(client).get_runtime())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_shutdown(client: *mut ClientT) {
    safe_as_mut!(client).shutdown();
}

impl ClientT {
    fn new(conn_str: &str) -> Result<ClientT, ErrorT> {
        // Runtime Error (mongoac)
        let runtime = RuntimeT::new().map_err(|e| ErrorT::MongoAC {
            code: ErrorCodeT::RuntimeError,
            message: Some(format!("runtime creation failed: {e}")),
        })?;

        // Options Error (rust)
        let client = runtime.block_on(async move {
            // Parsing Error (rust)
            let mut opts = ClientOptions::parse(conn_str).await?;

            opts.driver_info = Some(build_driver_info());

            Client::with_options(opts)
        })?;

        Ok(ClientT {
            runtime,
            inner: client,
        })
    }

    fn get_runtime(&self) -> RuntimeT {
        self.runtime.clone()
    }

    fn shutdown(&mut self) {
        // `.shutdown()` consumes the client.
        let client = self.inner.clone();

        self.runtime.block_on(async {
            client.shutdown().await;
        });
    }
}

fn build_driver_info() -> DriverInfo {
    DriverInfo::builder()
        .name("mongoac".to_string())
        .version(Some(MONGOAC_VERSION_FULL.into()))
        .platform(Some(MONGOAC_BUILD_PLATFORM.into()))
        .build()
}
