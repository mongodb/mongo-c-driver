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

use std::time::Duration;

use crate::bson::{BsonT, BsonViewT};
use crate::cursor_type::{CursorTypeT, MONGOAC_CURSOR_TYPE_UNKNOWN, mongoac_cursor_type_t};
use crate::error::ErrorT;
use crate::private::macros::*;
use crate::read_preference::ReadPreferenceT;
use crate::string::StringViewT;

use mongodb::bson::{Bson, RawDocumentBuf};
use mongodb::options::SelectionCriteria;
use mongodb::options::{CursorType, RunCursorCommandOptions};

#[derive(Clone)]
pub struct RunCursorCommandOptionsT(RunCursorCommandOptions);

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_destroy(
    options: *mut RunCursorCommandOptionsT,
) {
    safe_drop!(options);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_clone(
    options: *const RunCursorCommandOptionsT,
) -> *mut RunCursorCommandOptionsT {
    safe_into_raw!(safe_as_ref!(options).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_new() -> *mut RunCursorCommandOptionsT {
    safe_into_raw!(RunCursorCommandOptionsT(RunCursorCommandOptions::default()))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_batch_size_is_none(
    options: *const RunCursorCommandOptionsT,
) -> bool {
    safe_optional_as_ref!(options).map_or(true, |v| v.0.batch_size.is_none())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_batch_size(
    options: *const RunCursorCommandOptionsT,
) -> u32 {
    safe_as_ref!(options).0.batch_size.unwrap_or_default()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_comment(
    options: *const RunCursorCommandOptionsT,
    error: *mut ErrorT,
) -> BsonT {
    let error = safe_optional_error_as_mut!(error);
    let options = safe_as_ref_with_error!(options, error);

    let doc = match &options.0.comment {
        Some(v) => mongodb::bson::doc! {"v": v.clone()},
        None => return BsonT::default(),
    };

    safe_result!(RawDocumentBuf::try_from(doc), error).into()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_comment_string(
    options: *const RunCursorCommandOptionsT,
) -> StringViewT {
    safe_as_ref!(options)
        .0
        .comment
        .as_ref()
        .and_then(Bson::as_str)
        .map_or(StringViewT::default(), Into::into)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_comment_is_none(
    options: *const RunCursorCommandOptionsT,
) -> bool {
    safe_optional_as_ref!(options).map_or(true, |v| v.0.comment.is_none())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_cursor_type_is_none(
    options: *const RunCursorCommandOptionsT,
) -> bool {
    safe_optional_as_ref!(options).map_or(true, |v| v.0.cursor_type.is_none())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_cursor_type(
    options: *const RunCursorCommandOptionsT,
) -> mongoac_cursor_type_t {
    safe_as_ref!(options)
        .0
        .cursor_type
        .map_or(
            CursorTypeT::Unknown(MONGOAC_CURSOR_TYPE_UNKNOWN),
            Into::into,
        )
        .into()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_max_time_as_secs(
    options: *const RunCursorCommandOptionsT,
) -> u64 {
    safe_as_ref!(options).0.max_time.map_or(0, |v| v.as_secs())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_max_time_subsec_millis(
    options: *const RunCursorCommandOptionsT,
) -> u32 {
    safe_as_ref!(options)
        .0
        .max_time
        .map_or(0, |v| v.subsec_millis())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_max_time_is_none(
    options: *const RunCursorCommandOptionsT,
) -> bool {
    safe_as_ref!(options).0.max_time.is_none()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_get_read_preference(
    options: *const RunCursorCommandOptionsT,
) -> *const ReadPreferenceT {
    match &safe_as_ref!(options).0.selection_criteria {
        Some(SelectionCriteria::ReadPreference(v)) => {
            safe_ptr_from_ref!(v, mongodb::options::ReadPreference)
        }
        _ => std::ptr::null(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_batch_size(
    options: *mut RunCursorCommandOptionsT,
    v: u32,
) {
    safe_as_mut!(options).0.batch_size = Some(v);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_batch_size_none(
    options: *mut RunCursorCommandOptionsT,
) {
    safe_as_mut!(options).0.batch_size = None;
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_comment(
    options: *mut RunCursorCommandOptionsT,
    v: BsonViewT,
    error: *mut ErrorT,
) {
    let error = safe_optional_error_as_mut!(error);
    let options = safe_as_mut_with_error!(options, error);
    let v = safe_bson_view_with_error!(v, error);

    let Some(v) = safe_result!(v.get("v"), error) else {
        crate::private::safety::invalid_argument(error, "missing field 'v'");
        return;
    };

    options.0.comment = Some(safe_result!(v.try_into(), error));
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_comment_none(
    options: *mut RunCursorCommandOptionsT,
) {
    safe_as_mut!(options).0.comment = None;
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_comment_string(
    options: *mut RunCursorCommandOptionsT,
    v: StringViewT,
) {
    safe_as_mut!(options).0.comment = Some(safe_string_view!(v).into());
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_cursor_type(
    options: *mut RunCursorCommandOptionsT,
    v: mongoac_cursor_type_t,
) {
    safe_as_mut!(options).0.cursor_type = match v.into() {
        CursorTypeT::NonTailable => Some(CursorType::NonTailable),
        CursorTypeT::Tailable => Some(CursorType::Tailable),
        CursorTypeT::TailableAwait => Some(CursorType::TailableAwait),

        CursorTypeT::Unknown(_) => return, // Do nothing.
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_cursor_type_none(
    options: *mut RunCursorCommandOptionsT,
) {
    safe_as_mut!(options).0.cursor_type = None;
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_max_time_from_millis(
    options: *mut RunCursorCommandOptionsT,
    v: u64,
) {
    safe_as_mut!(options).0.max_time = Some(Duration::from_millis(v));
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_max_time_from_secs(
    options: *mut RunCursorCommandOptionsT,
    v: u64,
) {
    safe_as_mut!(options).0.max_time = Some(Duration::from_secs(v));
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_max_time_none(
    options: *mut RunCursorCommandOptionsT,
) {
    safe_as_mut!(options).0.max_time = None;
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_cursor_command_options_set_read_preference(
    options: *mut RunCursorCommandOptionsT,
    v: *const ReadPreferenceT,
) {
    safe_as_mut!(options).0.selection_criteria =
        safe_optional_as_ref!(v).map(|v| SelectionCriteria::ReadPreference(v.into()));
}

// TODO: CDRIVER-6396: server_selector (`SelectionCriteria::Predicate`)

impl From<&RunCursorCommandOptionsT> for RunCursorCommandOptions {
    fn from(value: &RunCursorCommandOptionsT) -> Self {
        value.0.clone()
    }
}
