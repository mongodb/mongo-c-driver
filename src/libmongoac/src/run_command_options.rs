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

use crate::read_preference::ReadPreferenceT;

use mongodb::options::RunCommandOptions;
use mongodb::options::SelectionCriteria;

#[derive(Clone)]
pub struct RunCommandOptionsT(RunCommandOptions);

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_command_options_destroy(options: *mut RunCommandOptionsT) {
    safe_drop!(options);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_command_options_clone(
    options: *const RunCommandOptionsT,
) -> *mut RunCommandOptionsT {
    safe_into_raw!(safe_as_ref!(options).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_command_options_new() -> *mut RunCommandOptionsT {
    safe_into_raw!(RunCommandOptionsT(RunCommandOptions::default()))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_command_options_get_read_preference(
    options: *const RunCommandOptionsT,
) -> *const ReadPreferenceT {
    match &safe_as_ref!(options).0.selection_criteria {
        Some(SelectionCriteria::ReadPreference(v)) => {
            safe_ptr_from_ref!(v, mongodb::options::ReadPreference)
        }
        _ => std::ptr::null(),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_run_command_options_set_read_preference(
    options: *mut RunCommandOptionsT,
    v: *const ReadPreferenceT,
) {
    safe_as_mut!(options).0.selection_criteria =
        safe_optional_as_ref!(v).map(|v| SelectionCriteria::ReadPreference(v.into()));
}

// TODO: CDRIVER-6396: server_selector (`SelectionCriteria::Predicate`)

impl From<&RunCommandOptionsT> for RunCommandOptions {
    fn from(value: &RunCommandOptionsT) -> Self {
        value.0.clone()
    }
}
