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

use mongodb::options::{ReadPreference, ReadPreferenceOptions};

use std::time::Duration;

#[allow(non_camel_case_types)]
pub type mongoac_read_preference_mode_t = i32;
pub const MONGOAC_READ_PREFERENCE_MODE_PRIMARY: mongoac_read_preference_mode_t = 0;
pub const MONGOAC_READ_PREFERENCE_MODE_SECONDARY: mongoac_read_preference_mode_t = 1;
pub const MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED: mongoac_read_preference_mode_t = 2;
pub const MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED: mongoac_read_preference_mode_t = 3;
pub const MONGOAC_READ_PREFERENCE_MODE_NEAREST: mongoac_read_preference_mode_t = 4;
pub const MONGOAC_READ_PREFERENCE_MODE_UNKNOWN: mongoac_read_preference_mode_t = i32::MIN;

#[derive(Clone)]
pub struct ReadPreferenceT(ReadPreference);

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_destroy(read_preference: *mut ReadPreferenceT) {
    safe_drop!(read_preference);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_clone(
    read_preference: *const ReadPreferenceT,
) -> *mut ReadPreferenceT {
    safe_into_raw!(safe_as_ref!(read_preference).clone())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_new() -> *mut ReadPreferenceT {
    safe_into_raw!(ReadPreferenceT(ReadPreference::Primary))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_get_max_staleness_as_secs(
    read_preference: *const ReadPreferenceT,
) -> u64 {
    safe_as_ref!(read_preference)
        .get_max_staleness()
        .map_or(0, |v| v.as_secs())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_get_max_staleness_is_none(
    read_preference: *const ReadPreferenceT,
) -> bool {
    safe_optional_as_ref!(read_preference).map_or(true, |v| v.get_max_staleness().is_none())
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_get_mode(
    read_preference: *const ReadPreferenceT,
) -> mongoac_read_preference_mode_t {
    match safe_optional_as_ref!(read_preference).map(|v| &v.0) {
        Some(ReadPreference::Primary) => MONGOAC_READ_PREFERENCE_MODE_PRIMARY,
        Some(ReadPreference::Secondary { .. }) => MONGOAC_READ_PREFERENCE_MODE_SECONDARY,
        Some(ReadPreference::PrimaryPreferred { .. }) => {
            MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED
        }
        Some(ReadPreference::SecondaryPreferred { .. }) => {
            MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED
        }
        Some(ReadPreference::Nearest { .. }) => MONGOAC_READ_PREFERENCE_MODE_NEAREST,

        _ => MONGOAC_READ_PREFERENCE_MODE_UNKNOWN, // #[non_exhaustive]
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_set_max_staleness_from_secs(
    read_preference: *mut ReadPreferenceT,
    v: u64,
) {
    safe_as_mut!(read_preference).set_max_staleness(Some(Duration::from_secs(v)));
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_set_max_staleness_none(
    read_preference: *mut ReadPreferenceT,
) {
    safe_as_mut!(read_preference).set_max_staleness(None);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_set_mode(
    read_preference: *mut ReadPreferenceT,
    v: mongoac_read_preference_mode_t,
) {
    let read_preference = safe_as_mut!(read_preference);

    read_preference.0 = match v {
        MONGOAC_READ_PREFERENCE_MODE_PRIMARY => ReadPreference::primary(),
        MONGOAC_READ_PREFERENCE_MODE_SECONDARY => {
            ReadPreference::secondary(read_preference.take_options())
        }
        MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED => {
            ReadPreference::primary_preferred(read_preference.take_options())
        }
        MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED => {
            ReadPreference::secondary_preferred(read_preference.take_options())
        }
        MONGOAC_READ_PREFERENCE_MODE_NEAREST => {
            ReadPreference::nearest(read_preference.take_options())
        }

        _ => return, // unknown mode: does nothing
    };
}

// TODO: tag sets (CDRIVER-6442)
// TODO: hedge (deprecated)

impl ReadPreferenceT {
    fn get_max_staleness(&self) -> Option<Duration> {
        self.get_options_as_ref().and_then(|v| v.max_staleness)
    }

    fn get_options_as_mut(&mut self) -> Option<&mut Option<ReadPreferenceOptions>> {
        match &mut self.0 {
            ReadPreference::Secondary { options }
            | ReadPreference::PrimaryPreferred { options }
            | ReadPreference::SecondaryPreferred { options }
            | ReadPreference::Nearest { options } => Some(options),

            ReadPreference::Primary | _ => None, // #[non_exhaustive]
        }
    }

    fn get_options_as_ref(&self) -> Option<&ReadPreferenceOptions> {
        match &self.0 {
            ReadPreference::Secondary { options }
            | ReadPreference::PrimaryPreferred { options }
            | ReadPreference::SecondaryPreferred { options }
            | ReadPreference::Nearest { options } => options.as_ref(),

            ReadPreference::Primary | _ => None, // #[non_exhaustive]
        }
    }

    fn set_max_staleness(&mut self, max_staleness: Option<Duration>) {
        if let Some(options) = self.get_options_as_mut() {
            options.get_or_insert_default().max_staleness = max_staleness;
        }
    }

    fn take_options(&mut self) -> Option<ReadPreferenceOptions> {
        self.get_options_as_mut().and_then(Option::take)
    }
}

impl From<&ReadPreferenceT> for ReadPreference {
    fn from(value: &ReadPreferenceT) -> Self {
        value.0.clone()
    }
}
