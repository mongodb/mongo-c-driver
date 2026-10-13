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
use crate::runtime::{RuntimeAware, RuntimeT};

use parking_lot::Mutex;
use tokio::task::{JoinError, JoinHandle};

use std::any::Any;
use std::future::{Future, poll_fn};
use std::pin::Pin;
use std::sync::Arc;
use std::sync::OnceLock;
use std::task::{Context, Poll, Waker};

#[derive(Clone)]
pub struct FutureT {
    value: Arc<dyn FutureValueType>,
    runtime: RuntimeT,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_destroy(future: *mut FutureT) {
    safe_drop!(future);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_clone(future: *const FutureT) -> *mut FutureT {
    safe_into_raw!(safe_as_ref!(future).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_get_runtime(future: *const FutureT) -> *mut RuntimeT {
    safe_into_raw!(safe_as_ref!(future).get_runtime())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_get_runtime_address(future: *const FutureT) -> usize {
    safe_as_ref!(future).get_runtime_address()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_is_ready(future: *const FutureT) -> bool {
    safe_as_ref!(future).is_ready()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_get_bool(future: *const FutureT, error: *mut ErrorT) -> bool {
    let error = safe_optional_error_as_mut!(error);
    let future = safe_as_ref_with_error!(future, error);

    *safe_result!(future.get_bool(), error)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_get_uint64(future: *const FutureT, error: *mut ErrorT) -> u64 {
    let error = safe_optional_error_as_mut!(error);
    let future = safe_as_ref_with_error!(future, error);

    *safe_result!(future.get_uint64(), error)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_get_void(future: *const FutureT, error: *mut ErrorT) {
    let error = safe_optional_error_as_mut!(error);
    let future = safe_as_ref_with_error!(future, error);

    safe_result!(future.get_void(), error);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_future_poll(future: *mut FutureT) -> bool {
    safe_as_mut!(future).poll()
}

impl FutureT {
    /// Create a new [`FutureT`] from the given handle to an underlying future.
    #[must_use]
    pub(crate) fn new<T: Send + Sync + 'static>(
        runtime: RuntimeT,
        handle: JoinHandle<Result<T, ErrorT>>,
    ) -> Self {
        Self {
            runtime,
            value: Arc::new(FutureValue::new(handle)),
        }
    }

    #[must_use]
    pub(crate) fn get_runtime_address(&self) -> usize {
        self.runtime.address()
    }

    #[must_use]
    pub(crate) fn is_ready(&self) -> bool {
        self.value.is_ready()
    }

    fn get_bool(&self) -> Result<&bool, ErrorT> {
        self.get::<bool>("bool")
    }

    fn get_uint64(&self) -> Result<&u64, ErrorT> {
        self.get::<u64>("uint64")
    }

    fn get_void(&self) -> Result<&(), ErrorT> {
        self.get::<()>("void")
    }

    /// Equivalent to [`FutureValue<T>::poll_with_context()`] with a noop waker.
    #[must_use]
    fn poll(&mut self) -> bool {
        self.poll_with_context(&mut Context::from_waker(Waker::noop()))
    }

    /// Equivalent to [`FutureValue<T>::poll_with_context()`] with the given context.
    #[must_use]
    fn poll_with_context(&self, ctx: &mut Context<'_>) -> bool {
        self.value.poll_with_context(ctx)
    }

    /// Equivalent to [`FutureValue<T>::result()`].
    fn get<T: Send + Sync + 'static>(&self, name: &str) -> Result<&T, ErrorT> {
        match self.value.as_any().downcast_ref::<FutureValue<T>>() {
            Some(fvt) => fvt.result(),
            None => Err(ErrorT::MongoAC {
                code: ErrorCodeT::InvalidArgument,
                message: Some(format!("future does not return a {name}")),
            }),
        }
    }

    /// Return a new future which polls the underlying future.
    pub(crate) fn poll_async(&self) -> impl Future<Output = ()> + '_ {
        poll_fn(|ctx| {
            if self.value.poll_with_context(ctx) {
                Poll::Ready(())
            } else {
                Poll::Pending
            }
        })
    }
}

impl RuntimeAware for FutureT {
    fn get_runtime(&self) -> RuntimeT {
        self.runtime.clone()
    }
}

/// The concrete representation of a handle to an underlying future and its expected result.
struct FutureValue<T> {
    /// The result of an underlying future once it is ready.
    result: OnceLock<Result<T, ErrorT>>,

    /// The handle to a pending underlying future.
    handle: Mutex<Option<JoinHandle<Result<T, ErrorT>>>>,
}

impl<T: Send + Sync + 'static> FutureValue<T> {
    /// Create a new [`FutureValue<T>`] from the given handle to an underlying future.
    #[must_use]
    fn new(handle: JoinHandle<Result<T, ErrorT>>) -> Self {
        Self {
            result: OnceLock::new(),
            handle: Mutex::new(Some(handle)),
        }
    }

    /// Return `true` when the result of the underlying future is already available.
    ///
    /// This function does not poll the underlying future!
    fn is_ready(&self) -> bool {
        self.result.get().is_some()
    }

    /// Return the result of the underlying future when it is already available.
    ///
    /// When the result is not available, return a runtime error.
    fn result(&self) -> Result<&T, ErrorT> {
        match self.result.get() {
            Some(Ok(val)) => Ok(val),
            Some(Err(err)) => Err(err.clone()),
            None => Err(ErrorT::MongoAC {
                code: ErrorCodeT::InvalidArgument,
                message: Some("future is not ready".to_string()),
            }),
        }
    }

    /// Poll the underlying future with the given context and return `true` when its result is available.
    ///
    /// When the result is already available, return `true` without polling the underlying future (idempotence).
    fn poll_with_context(&self, ctx: &mut Context<'_>) -> bool {
        // Double-checked lock: result may already be available.
        if self.is_ready() {
            return true;
        }

        // This lock acquisition is unlikely to be contended given normal usage patterns.
        let mut guard = self.handle.lock();

        let Some(handle) = guard.as_mut() else {
            // Double-checked lock: failed: result should already be available.
            return self.is_ready();
        };

        // Double-checked lock: success: poll the underlying future.
        match Pin::new(handle).poll(ctx) {
            Poll::Ready(result) => {
                let result: Result<T, ErrorT> = result.unwrap_or_else(|err: JoinError| {
                    // `JoinError` should only occur when the underlying future panics or is cancelled: in either case,
                    // the result is no longer obtainable, so the `JoinError` itself must be the result.
                    Err(ErrorT::MongoAC {
                        code: ErrorCodeT::RuntimeError,
                        message: Some(format!("tokio::task::JoinError: {err}")),
                    })
                });

                // Enforced by double-checked lock and conditioned on `self.is_ready()`.
                if self.result.set(result).is_err() {
                    unreachable!("FutureValue<T>::result must be set exactly once");
                }

                // Prohibit polling an already completed future.
                *guard = None;

                true
            }
            Poll::Pending => false,
        }
    }
}

/// [`FutureValue<T>`] operations which are not dependent on `T`.
trait FutureValueType: Send + Sync {
    /// Return `true` when a result is already available.
    ///
    /// This function does not poll the underlying future!
    #[must_use]
    fn is_ready(&self) -> bool;

    /// Poll the underlying future and return `true` when its result is ready.
    #[must_use]
    fn poll_with_context(&self, ctx: &mut Context<'_>) -> bool;

    /// Return `&self` as `&dyn Any` so it can be downcast to [`FutureValue<T>`].
    #[must_use]
    fn as_any(&self) -> &dyn Any;
}

impl<T: Send + Sync + 'static> FutureValueType for FutureValue<T> {
    fn is_ready(&self) -> bool {
        self.is_ready()
    }

    fn poll_with_context(&self, ctx: &mut Context<'_>) -> bool {
        self.poll_with_context(ctx)
    }

    fn as_any(&self) -> &dyn Any {
        self
    }
}

/// Support associating a [`FutureT`] with its index in a slice.
pub(crate) struct FutureExt<'a> {
    pub future: &'a FutureT,
    pub index: usize,
}

impl<'a> FutureExt<'a> {
    /// Associate the given [`FutureT`] with its index in a slice.
    #[must_use]
    pub(crate) fn new(future: &'a FutureT, index: usize) -> Self {
        Self { future, index }
    }
}

impl Future for FutureExt<'_> {
    /// Return the index of the associated [`FutureT`] when it is ready.
    type Output = usize;

    fn poll(self: Pin<&mut Self>, ctx: &mut Context<'_>) -> Poll<Self::Output> {
        let this = self.get_mut();
        if this.future.poll_with_context(ctx) {
            Poll::Ready(this.index)
        } else {
            Poll::Pending
        }
    }
}
