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

use crate::error::ErrorT;
use crate::future::{FutureExt, FutureT};
use crate::private::macros::*;

use futures_util::stream::{FuturesUnordered, StreamExt};
use std::future::Future;
use std::sync::Arc;
use tokio::time::Duration;

/// Safely convert the given raw pointer into a future associated with the specified runtime.
///
/// When `ptr` is null or the future is not associated with the specified runtime, set `error` and early-return from the function via `return Default::default();`.
///
/// Usage:
///
/// ```rust
/// fn example(future: *mut FutureT, runtime: &RuntimeT, error: Option<&mut ErrorT>) -> R {
///     let res: &FutureT = safe_future_from_runtime_with_error!(future, runtime, error);
///     assert!(!future.is_null());
///     assert!(res.get_runtime_address() == runtime.address());
/// }
/// ```
///
/// Preconditions:
///
/// - `ptr` must either be null or a valid pointer to `T`.
/// - `ptr` must not be mutably accessed concurrently by any other function.
/// - `error` must be null or in its default state (cleared).
macro_rules! safe_future_from_runtime_with_error {
    ($future:expr, $runtime:expr, $error:expr) => {{
        let future: &FutureT = safe_as_ref_with_error!($future, $error);
        let runtime: &RuntimeT = $runtime;
        if future.get_runtime_address() != runtime.address() {
            $crate::private::safety::invalid_argument(
                $error,
                "future is not associated with the given runtime",
            );
            return Default::default();
        }
        future
    }};
}

/// Safely convert the given raw array pointer into a slice of futures associated with the specified runtime.
///
/// Set `error` and early-return from the function via `return Default::default();` when any of the following conditions are true:
///
/// - `futures` is null or `count` is zero ("futures array is null or empty").
/// - a future element is null.
/// - a future element is not associated with the given runtime.
///
/// Usage:
///
/// ```rust
/// fn example(futures: *const *mut FutureT, count: usize, runtime: &RuntimeT, error: Option<&mut ErrorT>) -> R {
///     let futures: &[*mut FutureT] = safe_futures_from_runtime_with_error!(futures, count, runtime, error);
///
///     for future in futures.iter() {
///         let future: &FutureT = unsafe { future.as_ref() }.expect("safe_futures_from_runtime_with_error! postcondition");
///         assert!(future.get_runtime_address() == runtime.address());
///     }
/// }
/// ```
///
/// Preconditions:
///
/// - `futures` must either be null or a valid pointer to an array of at least `count` elements.
/// - `futures` must not be mutably accessed concurrently by any other function.
/// - All elements in the `futures` array must be unique.
/// - `error` must be null or in its default state (cleared).
macro_rules! safe_futures_from_runtime_with_error {
    ($futures:expr, $count:expr, $runtime:expr, $error:expr) => {{
        let futures: *const *mut FutureT = $futures;
        if futures.is_null() {
                $crate::private::safety::invalid_argument(
                    $error,
                    "futures array is null",
                );
            return Default::default();
        }
        let count: usize = $count;
        if count == 0 {
                $crate::private::safety::invalid_argument(
                    $error,
                    "futures array is empty",
                );
            return Default::default();
        }
        let futures: &[*mut FutureT] = unsafe { std::slice::from_raw_parts(futures, count) };
        let runtime: &RuntimeT = $runtime;
        for (i, ptr) in futures.iter().enumerate() {
            let Some(future) = safe_optional_as_ref!(*ptr) else {
                $crate::private::safety::invalid_argument(
                    $error,
                    format!("futures array element at index {i}: must not be null"),
                );
                return Default::default();
            };
            if future.get_runtime_address() != runtime.address() {
                $crate::private::safety::invalid_argument(
                    $error,
                    format!("futures array element at index {i}: future is not associated with the given runtime"),
                );
                return Default::default();
            }
        }
        futures
    }};
}

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
pub extern "C" fn mongoac_runtime_block_on(
    runtime: *const RuntimeT,
    future: *mut FutureT,
    error: *mut ErrorT,
) {
    let error = safe_optional_error_as_mut!(error);
    let runtime = safe_as_ref_with_error!(runtime, error);
    let future = safe_future_from_runtime_with_error!(future, runtime, error);

    runtime.block_on_future(future);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_block_on_with_timeout(
    runtime: *const RuntimeT,
    future: *mut FutureT,
    timeout_ms: u64,
    error: *mut ErrorT,
) {
    let error = safe_optional_error_as_mut!(error);
    let runtime = safe_as_ref_with_error!(runtime, error);
    let future = safe_future_from_runtime_with_error!(future, runtime, error);
    let timeout = Duration::from_millis(timeout_ms);

    safe_result!(runtime.block_on_future_with_timeout(future, timeout), error);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_block_on_any(
    runtime: *const RuntimeT,
    futures: *const *mut FutureT,
    count: usize,
    error: *mut ErrorT,
) -> *const *mut FutureT {
    let error = safe_optional_error_as_mut!(error);
    let runtime = safe_as_ref_with_error!(runtime, error);
    let futures = safe_futures_from_runtime_with_error!(futures, count, runtime, error);

    if let Some(ptr) = any_ready(futures) {
        return ptr;
    }

    &raw const futures[runtime.block_on_any(futures)]
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_block_on_any_with_timeout(
    runtime: *const RuntimeT,
    futures: *const *mut FutureT,
    count: usize,
    timeout_ms: u64,
    error: *mut ErrorT,
) -> *const *mut FutureT {
    let error = safe_optional_error_as_mut!(error);
    let runtime = safe_as_ref_with_error!(runtime, error);
    let futures = safe_futures_from_runtime_with_error!(futures, count, runtime, error);

    if let Some(ptr) = any_ready(futures) {
        return ptr;
    }

    let timeout = Duration::from_millis(timeout_ms);

    &raw const futures[safe_result!(runtime.block_on_any_with_timeout(futures, timeout), error)]
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_block_on_all(
    runtime: *const RuntimeT,
    futures: *const *mut FutureT,
    count: usize,
    error: *mut ErrorT,
) {
    let error = safe_optional_error_as_mut!(error);
    let runtime = safe_as_ref_with_error!(runtime, error);
    let futures = safe_futures_from_runtime_with_error!(futures, count, runtime, error);

    if all_ready(futures) {
        return;
    }

    runtime.block_on_all(futures);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_runtime_block_on_all_with_timeout(
    runtime: *const RuntimeT,
    futures: *const *mut FutureT,
    count: usize,
    timeout_ms: u64,
    error: *mut ErrorT,
) {
    let error = safe_optional_error_as_mut!(error);
    let runtime = safe_as_ref_with_error!(runtime, error);

    let futures = safe_futures_from_runtime_with_error!(futures, count, runtime, error);

    if all_ready(futures) {
        return;
    }

    let timeout = Duration::from_millis(timeout_ms);

    safe_result!(runtime.block_on_all_with_timeout(futures, timeout), error);
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

    // Named `block_on_future()` to avoid conflict with the generic `block_on::<F>()`.
    fn block_on_future(&self, future: &FutureT) {
        if future.is_ready() {
            return; // No work to do.
        }

        self.runtime.block_on(future.poll_async());
    }

    fn block_on_future_with_timeout(
        &self,
        future: &FutureT,
        timeout: Duration,
    ) -> Result<(), ErrorT> {
        if future.is_ready() {
            return Ok(()); // No work to do.
        }

        self.runtime.block_on(async {
            tokio::time::timeout(timeout, future.poll_async()).await?;
            Ok(())
        })
    }

    fn block_on_any(&self, futures: &[*mut FutureT]) -> usize {
        // `futures.len() > 0` is a `safe_futures_from_runtime_with_error!` postcondition.
        // `.next()` is always `Some` because the `FuturesUnordered` set is non-empty.
        self.runtime
            .block_on(futures_unordered(futures).next())
            .expect("safe_futures_from_runtime_with_error! postcondition")
    }

    fn block_on_any_with_timeout(
        &self,
        futures: &[*mut FutureT],
        timeout: Duration,
    ) -> Result<usize, ErrorT> {
        // `futures.len() > 0` is a `safe_futures_from_runtime_with_error!` postcondition.
        // `.next()` is always `Some` because the `FuturesUnordered` set is non-empty.
        self.runtime
            .block_on(async {
                Ok(tokio::time::timeout(timeout, futures_unordered(futures).next()).await?)
            })
            .map(|v| v.expect("safe_futures_from_runtime_with_error! postcondition"))
    }

    fn block_on_all(&self, futures: &[*mut FutureT]) {
        self.runtime.block_on(async {
            let mut fut_set = futures_unordered(futures);
            while fut_set.next().await.is_some() {}
        });
    }

    fn block_on_all_with_timeout(
        &self,
        futures: &[*mut FutureT],
        timeout: Duration,
    ) -> Result<(), ErrorT> {
        self.runtime.block_on(async {
            let mut fut_set = futures_unordered(futures);
            tokio::time::timeout(timeout, async { while fut_set.next().await.is_some() {} })
                .await?;
            Ok(())
        })
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

    /// For convenience: equivalent to `self.runtime.spawn(future)`.
    pub(crate) fn spawn<F>(&self, future: F) -> tokio::task::JoinHandle<F::Output>
    where
        F: Future + Send + 'static,
        F::Output: Send + 'static,
    {
        self.runtime.spawn(future)
    }
}

/// Provides an associated [`RuntimeT`] by which an async operation may be spawned as a new [`FutureT`].
pub(crate) trait RuntimeAware {
    /// Return the runtime associated with this value.
    fn get_runtime(&self) -> RuntimeT;

    /// Spawn the given async operation on the associated runtime as a [`FutureT`].
    ///
    /// The async operation must return a `Result<T, ErrorT>`. Explicitly specify the return type `T` when calling this
    /// function to ensure the async operation's return type is correct, e.g.: `self.spawn::<T>(op)`.
    fn spawn<T>(&self, op: impl Future<Output = Result<T, ErrorT>> + Send + 'static) -> FutureT
    where
        T: Send + Sync + 'static,
    {
        let rt = self.get_runtime();
        let handle = rt.spawn(op);

        FutureT::new(rt, handle)
    }
}

/// Convert the given slice of futures into [`FuturesUnordered`].
fn futures_unordered(futures: &[*mut FutureT]) -> FuturesUnordered<FutureExt<'_>> {
    futures
        .iter()
        .enumerate()
        .map(|(i, ptr)| {
            // SAFETY: `.as_ref().is_some()` is a `safe_futures_from_runtime_with_error!` postcondition.
            let future = unsafe { (*ptr).as_ref() }
                .expect("safe_futures_from_runtime_with_error! postcondition");
            FutureExt::new(future, i)
        })
        .collect()
}

/// Return an iterator to the first ready future when present, otherwise `None`.
fn any_ready(futures: &[*mut FutureT]) -> Option<*const *mut FutureT> {
    for (i, ptr) in futures.iter().enumerate() {
        // SAFETY: `.as_ref().is_some()` is a `safe_futures_from_runtime_with_error!` postcondition.
        if unsafe { (*ptr).as_ref() }
            .expect("safe_futures_from_runtime_with_error! postcondition")
            .is_ready()
        {
            return Some(&raw const futures[i]);
        }
    }

    None
}

/// Return `true` when all futures are ready, otherwise `false`.
fn all_ready(futures: &[*mut FutureT]) -> bool {
    futures.iter().all(|ptr| {
        // SAFETY: `.as_ref().is_some()` is a `safe_futures_from_runtime_with_error!` postcondition.
        unsafe { (*ptr).as_ref() }
            .expect("safe_futures_from_runtime_with_error! postcondition")
            .is_ready()
    })
}
