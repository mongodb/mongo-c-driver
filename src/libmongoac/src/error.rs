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

use crate::string::StringT;

use num_enum::{FromPrimitive, IntoPrimitive};
use strum::EnumMessage;

#[allow(non_camel_case_types)]
pub type mongoac_error_category_t = i32;
pub const MONGOAC_ERROR_CATEGORY_NONE: mongoac_error_category_t = 0;
pub const MONGOAC_ERROR_CATEGORY_MONGOAC: mongoac_error_category_t = 1;
pub const MONGOAC_ERROR_CATEGORY_SERVER: mongoac_error_category_t = 2;
pub const MONGOAC_ERROR_CATEGORY_RUST: mongoac_error_category_t = 3;
pub const MONGOAC_ERROR_CATEGORY_UNKNOWN: mongoac_error_category_t = i32::MIN;

#[allow(non_camel_case_types)]
pub type mongoac_error_code_t = i32;
pub const MONGOAC_ERROR_CODE_OK: mongoac_error_code_t = 0;
pub const MONGOAC_ERROR_CODE_INVALID_ARGUMENT: mongoac_error_code_t = 1;
pub const MONGOAC_ERROR_CODE_RUNTIME_ERROR: mongoac_error_code_t = 2;
pub const MONGOAC_ERROR_CODE_TIMEOUT: mongoac_error_code_t = 3;
pub const MONGOAC_ERROR_CODE_UNKNOWN: mongoac_error_code_t = i32::MIN;

#[derive(Clone, Copy, Debug, Eq, FromPrimitive, IntoPrimitive, PartialEq)]
#[repr(i32)]
pub enum ErrorCategoryT {
    None = MONGOAC_ERROR_CATEGORY_NONE,
    MongoAC = MONGOAC_ERROR_CATEGORY_MONGOAC,
    Server = MONGOAC_ERROR_CATEGORY_SERVER,
    Rust = MONGOAC_ERROR_CATEGORY_RUST,

    #[num_enum(catch_all)]
    Unknown(i32),
}

#[derive(Clone, Copy, Debug, EnumMessage, Eq, FromPrimitive, IntoPrimitive, PartialEq)]
#[repr(i32)]
pub enum ErrorCodeT {
    #[strum(message = "ok")]
    Ok = MONGOAC_ERROR_CODE_OK,

    #[strum(message = "invalid argument")]
    InvalidArgument = MONGOAC_ERROR_CODE_INVALID_ARGUMENT,

    #[strum(message = "runtime error")]
    RuntimeError = MONGOAC_ERROR_CODE_RUNTIME_ERROR,

    #[strum(message = "timeout")]
    Timeout = MONGOAC_ERROR_CODE_TIMEOUT,

    #[strum(message = "unknown error code")]
    #[num_enum(catch_all)]
    Unknown(i32),
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_error_destroy(error: *mut ErrorT) {
    safe_drop!(error);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_error_clone(error: *const ErrorT) -> *mut ErrorT {
    safe_into_raw!(safe_as_ref!(error).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_error_new() -> *mut ErrorT {
    safe_into_raw!(ErrorT::new())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_error_category(error: *const ErrorT) -> i32 {
    safe_as_ref!(error).category().into()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_error_code(error: *const ErrorT) -> i32 {
    safe_as_ref!(error).code().into()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_error_message(error: *const ErrorT) -> StringT {
    safe_as_ref!(error)
        .message()
        .map(Into::into)
        .unwrap_or_default()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_error_clear(error: *mut ErrorT) {
    safe_as_mut!(error).clear();
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_error_set(error: *mut ErrorT, category: i32, code: i32) {
    safe_as_mut!(error).set(category, code);
}

#[derive(Debug, Default)]
pub enum ErrorT {
    #[default]
    None,
    MongoAC {
        code: ErrorCodeT,
        message: Option<String>,
    },

    #[allow(private_interfaces)]
    Server(Box<ServerErrorT>), // Box<T>: avoid clippy::result_large_err warnings.
    Rust(mongodb::error::Error),

    Unknown {
        code: i32,
        category: i32,
    },
}

impl ErrorT {
    #[must_use]
    fn new() -> Self {
        Self::None
    }

    #[must_use]
    fn category(&self) -> ErrorCategoryT {
        match self {
            Self::None => ErrorCategoryT::None,
            Self::MongoAC { .. } => ErrorCategoryT::MongoAC,
            Self::Server(_) => ErrorCategoryT::Server,
            Self::Rust(_) => ErrorCategoryT::Rust,
            Self::Unknown { code: _, category } => ErrorCategoryT::Unknown(*category),
        }
    }

    #[must_use]
    fn code(&self) -> ErrorCodeT {
        match self {
            Self::None => ErrorCodeT::Ok,
            Self::MongoAC { code, .. } => *code,

            Self::Server(err) => match &**err {
                ServerErrorT::Command(err) => ErrorCodeT::from(err.code),
                ServerErrorT::WriteError(err) => ErrorCodeT::from(err.code),
                ServerErrorT::WriteConcernError(err) => ErrorCodeT::from(err.code),
            },

            Self::Rust(_) => ErrorCodeT::Unknown(MONGOAC_ERROR_CODE_UNKNOWN),

            Self::Unknown { code, .. } => ErrorCodeT::Unknown(*code),
        }
    }

    #[must_use]
    fn message(&self) -> Option<String> {
        match self {
            Self::MongoAC { code, message, .. } => {
                // All variants must have `#[strum(message = "...")]`.
                let prefix = code.get_message().unwrap_or_default();

                Some(match message {
                    Some(msg) => format!("{prefix}: {msg}"),
                    None => prefix.to_string(),
                })
            }

            Self::Server(err) => Some(match &**err {
                ServerErrorT::Command(err) => err.message.clone(),
                ServerErrorT::WriteError(err) => err.message.clone(),
                ServerErrorT::WriteConcernError(err) => err.message.clone(),
            }),

            Self::Rust(err) => Some(err.to_string()),

            _ => None,
        }
    }

    fn clear(&mut self) {
        *self = Self::None;
    }

    fn set(&mut self, category: i32, code: i32) {
        *self = match category.into() {
            ErrorCategoryT::None => {
                if code == MONGOAC_ERROR_CODE_OK {
                    Self::None // Special case: equal to default state.
                } else {
                    Self::Unknown {
                        category: MONGOAC_ERROR_CATEGORY_NONE,
                        code,
                    }
                }
            }

            ErrorCategoryT::MongoAC => Self::MongoAC {
                code: code.into(),
                message: None,
            },

            // Custom error code values are only supported for the mongoac category.
            _ => Self::Unknown { code, category },
        }
    }
}

impl Clone for ErrorT {
    fn clone(&self) -> Self {
        match self {
            Self::None => Self::None,

            Self::MongoAC { code, message } => Self::MongoAC {
                code: *code,
                message: message.clone(),
            },

            Self::Server(err) => Self::Server(err.clone()),
            Self::Rust(err) => Self::Rust(err.clone()),

            Self::Unknown { category, code } => Self::Unknown {
                category: *category,
                code: *code,
            },
        }
    }
}

impl From<tokio::time::error::Elapsed> for ErrorT {
    fn from(error: tokio::time::error::Elapsed) -> Self {
        Self::MongoAC {
            code: ErrorCodeT::Timeout,
            message: Some(error.to_string()),
        }
    }
}

/// The subset of `mongodb::error::Error` variants which contain a single unambiguous server error code.
#[derive(Clone, Debug)]
enum ServerErrorT {
    Command(mongodb::error::CommandError),
    WriteError(mongodb::error::WriteError),
    WriteConcernError(mongodb::error::WriteConcernError),
}

impl From<mongodb::error::Error> for ErrorT {
    /// Convert the `mongodb::error::Error` to `ErrorT::Server` when applicable, otherwise `ErrorT::Rust`.
    fn from(err: mongodb::error::Error) -> Self {
        use mongodb::error::ErrorKind;
        use mongodb::error::WriteFailure;

        match err.kind.as_ref() {
            ErrorKind::Command(c) => Self::Server(Box::new(ServerErrorT::Command(c.clone()))),
            ErrorKind::Write(w) => match w {
                WriteFailure::WriteError(we) => {
                    Self::Server(Box::new(ServerErrorT::WriteError(we.clone())))
                }
                WriteFailure::WriteConcernError(wce) => {
                    Self::Server(Box::new(ServerErrorT::WriteConcernError(wce.clone())))
                }

                _ => Self::Rust(err), // `#[non_exhaustive]`
            },

            _ => Self::Rust(err), // `#[non_exhaustive]`
        }
    }
}
