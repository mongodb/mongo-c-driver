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

use std::sync::Arc;
use tokio::time::Duration;

#[derive(Clone)]
pub struct RuntimeT {
    runtime: Arc<tokio::runtime::Runtime>,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_destroy(runtime: *mut RuntimeT) {
    safe_drop!(runtime);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_clone(runtime: *const RuntimeT) -> *mut RuntimeT {
    safe_into_raw!(safe_as_ref!(runtime).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_address(runtime: *const RuntimeT) -> usize {
    safe_as_ref!(runtime).address()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_make_progress(runtime: *const RuntimeT) {
    safe_as_ref!(runtime).make_progress();
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_make_progress_for(runtime: *const RuntimeT, duration_ms: u64) {
    let duration = Duration::from_millis(duration_ms);

    safe_as_ref!(runtime).make_progress_for(duration);
}

impl RuntimeT {
    pub(crate) fn new() -> Result<Self, tokio::io::Error> {
        let runtime = tokio::runtime::Builder::new_current_thread()
            .enable_all() // I/O and timer.
            .build()?;

        Ok(Self {
            runtime: Arc::new(runtime),
        })
    }

    #[must_use]
    pub(crate) fn address(&self) -> usize {
        Arc::as_ptr(&self.runtime) as usize
    }

    fn make_progress(&self) {
        self.runtime.block_on(tokio::task::yield_now());
    }

    fn make_progress_for(&self, duration: Duration) {
        self.runtime
            .block_on(async { tokio::time::sleep(duration).await });
    }

    /// For convenience: equivalent to `self.runtime.block_on(future)`.
    pub(crate) fn block_on<F: Future>(&self, future: F) -> F::Output {
        self.runtime.block_on(future)
    }
}
