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

use crate::database::DatabaseT;
use crate::database_options::DatabaseOptionsT;
use crate::error::{ErrorCodeT, ErrorT};
use crate::future::FutureT;
use crate::private::macros::*;
use crate::runtime::{RuntimeAware, RuntimeT};
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
pub extern "C" fn mongoac_client_get_database(
    client: *const ClientT,
    name: StringViewT,
    options: *const DatabaseOptionsT,
) -> *mut DatabaseT {
    let client = safe_as_ref!(client);
    let name = safe_string_view!(name);
    let options = safe_optional_as_ref!(options);

    safe_into_raw!(client.get_database(name, options))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_get_default_database(client: *const ClientT) -> *mut DatabaseT {
    match safe_as_ref!(client).get_default_database() {
        Some(db) => safe_into_raw!(db),
        None => std::ptr::null_mut(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_shutdown(client: *mut ClientT) {
    safe_as_mut!(client).shutdown();
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_shutdown_async(client: *mut ClientT) -> *mut FutureT {
    safe_into_raw!(safe_as_mut!(client).shutdown_async())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_get_runtime(client: *const ClientT) -> *mut RuntimeT {
    safe_into_raw!(safe_as_ref!(client).get_runtime())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_client_get_runtime_address(client: *const ClientT) -> usize {
    safe_as_ref!(client).get_runtime_address()
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

    fn get_database(&self, name: &str, options: Option<&DatabaseOptionsT>) -> DatabaseT {
        DatabaseT::new(&self.inner, self.runtime.clone(), name, options)
    }

    #[must_use]
    fn get_default_database(&self) -> Option<DatabaseT> {
        self.inner
            .default_database()
            .map(|db| DatabaseT::new(&self.inner, self.runtime.clone(), db.name(), None))
    }

    fn shutdown(&mut self) {
        let client = self.inner.clone(); // Avoid consuming `self.inner`.

        self.runtime.block_on(async {
            client.shutdown().await;
        });
    }

    fn shutdown_async(&mut self) -> FutureT {
        let client = self.inner.clone();

        self.spawn::<()>(async move {
            client.shutdown().await;
            Ok(())
        })
    }

    // TODO: append_metadata
    // TODO: bulk_write
    // TODO: list_database_names
    // TODO: list_databases
    // TODO: read_concern
    // TODO: selection_criteria
    // TODO: start_session
    // TODO: warm_connection_pool
    // TODO: watch
    // TODO: write_concern

    pub(crate) fn new_from_client(client: &Client, runtime: &RuntimeT) -> Self {
        ClientT {
            inner: client.clone(),
            runtime: runtime.clone(),
        }
    }
}

impl RuntimeAware for ClientT {
    fn runtime(&self) -> &RuntimeT {
        &self.runtime
    }
}

fn build_driver_info() -> DriverInfo {
    DriverInfo::builder()
        .name("mongoac".to_string())
        .version(Some(MONGOAC_VERSION_FULL.into()))
        .platform(Some(MONGOAC_BUILD_PLATFORM.into()))
        .build()
}
