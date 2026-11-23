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

use mongodb::options::CursorType;

use num_enum::{FromPrimitive, IntoPrimitive};

#[allow(non_camel_case_types)]
pub type mongoac_cursor_type_t = i32;
pub const MONGOAC_CURSOR_TYPE_NON_TAILABLE: mongoac_cursor_type_t = 0;
pub const MONGOAC_CURSOR_TYPE_TAILABLE: mongoac_cursor_type_t = 1;
pub const MONGOAC_CURSOR_TYPE_TAILABLE_AWAIT: mongoac_cursor_type_t = 2;
pub const MONGOAC_CURSOR_TYPE_UNKNOWN: mongoac_cursor_type_t = i32::MIN;

#[derive(Clone, Copy, Debug, Eq, FromPrimitive, IntoPrimitive, PartialEq)]
#[repr(i32)]
pub enum CursorTypeT {
    NonTailable = MONGOAC_CURSOR_TYPE_NON_TAILABLE,
    Tailable = MONGOAC_CURSOR_TYPE_TAILABLE,
    TailableAwait = MONGOAC_CURSOR_TYPE_TAILABLE_AWAIT,

    #[num_enum(catch_all)]
    Unknown(i32),
}

impl From<CursorType> for CursorTypeT {
    fn from(value: CursorType) -> Self {
        match value {
            CursorType::NonTailable => CursorTypeT::NonTailable,
            CursorType::Tailable => CursorTypeT::Tailable,
            CursorType::TailableAwait => CursorTypeT::TailableAwait,

            _ => CursorTypeT::Unknown(MONGOAC_CURSOR_TYPE_UNKNOWN),
        }
    }
}
