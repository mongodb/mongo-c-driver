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

use num_enum::{FromPrimitive, IntoPrimitive};
use strum::EnumMessage;

#[allow(non_camel_case_types)]
pub type mongoac_error_category_t = i32;
pub const MONGOAC_ERROR_CATEGORY_NONE: mongoac_error_category_t = 0;
pub const MONGOAC_ERROR_CATEGORY_MONGOAC: mongoac_error_category_t = 1;
pub const MONGOAC_ERROR_CATEGORY_UNKNOWN: mongoac_error_category_t = i32::MIN;

#[allow(non_camel_case_types)]
pub type mongoac_error_code_t = i32;
pub const MONGOAC_ERROR_CODE_OK: mongoac_error_code_t = 0;
pub const MONGOAC_ERROR_CODE_INVALID_ARGUMENT: mongoac_error_code_t = 1;
pub const MONGOAC_ERROR_CODE_RUNTIME_ERROR: mongoac_error_code_t = 2;
pub const MONGOAC_ERROR_CODE_UNKNOWN: mongoac_error_code_t = i32::MIN;

#[derive(Clone, Copy, Debug, Eq, FromPrimitive, IntoPrimitive, PartialEq)]
#[repr(i32)]
pub enum ErrorCategoryT {
    None = MONGOAC_ERROR_CATEGORY_NONE,
    MongoAC = MONGOAC_ERROR_CATEGORY_MONGOAC,

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
    },
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
            Self::Unknown { code: _, category } => ErrorCategoryT::Unknown(*category),
        }
    }

    #[must_use]
    fn code(&self) -> ErrorCodeT {
        match self {
            Self::None => ErrorCodeT::Ok,
            Self::MongoAC { code, .. } => *code,
            Self::Unknown { code, .. } => ErrorCodeT::Unknown(*code),
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

            ErrorCategoryT::MongoAC => Self::MongoAC { code: code.into() },

            ErrorCategoryT::Unknown(_) => Self::Unknown { code, category },
        }
    }
}

impl Clone for ErrorT {
    fn clone(&self) -> Self {
        match self {
            Self::None => Self::None,

            Self::MongoAC { code } => Self::MongoAC { code: *code },

            Self::Unknown { category, code } => Self::Unknown {
                category: *category,
                code: *code,
            },
        }
    }
}
