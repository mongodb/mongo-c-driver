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
use crate::string::StringViewT;
use crate::{private::macros::*, string::StringViewArrayT};

use mongodb::options::{ReadPreference, ReadPreferenceOptions, TagSet};

use num_enum::{FromPrimitive, IntoPrimitive};
use std::{collections::HashMap, time::Duration};

#[allow(non_camel_case_types)]
pub type mongoac_read_preference_mode_t = i32;
pub const MONGOAC_READ_PREFERENCE_MODE_PRIMARY: mongoac_read_preference_mode_t = 0;
pub const MONGOAC_READ_PREFERENCE_MODE_SECONDARY: mongoac_read_preference_mode_t = 1;
pub const MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED: mongoac_read_preference_mode_t = 2;
pub const MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED: mongoac_read_preference_mode_t = 3;
pub const MONGOAC_READ_PREFERENCE_MODE_NEAREST: mongoac_read_preference_mode_t = 4;
pub const MONGOAC_READ_PREFERENCE_MODE_UNKNOWN: mongoac_read_preference_mode_t = i32::MIN;

#[derive(Clone, Copy, Debug, Eq, FromPrimitive, IntoPrimitive, PartialEq)]
#[repr(i32)]
pub enum ReadPreferenceModeT {
    Primary = MONGOAC_READ_PREFERENCE_MODE_PRIMARY,
    Secondary = MONGOAC_READ_PREFERENCE_MODE_SECONDARY,
    PrimaryPreferred = MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED,
    SecondaryPreferred = MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED,
    Nearest = MONGOAC_READ_PREFERENCE_MODE_NEAREST,

    #[num_enum(catch_all)]
    Unknown(i32),
}

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
    safe_optional_as_ref!(read_preference)
        .map_or(
            ReadPreferenceModeT::Unknown(MONGOAC_READ_PREFERENCE_MODE_UNKNOWN),
            Into::into,
        )
        .into()
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_get_tag_sets_get_get(
    read_preference: *const ReadPreferenceT,
    index: usize,
    key: StringViewT,
) -> StringViewT {
    let key = safe_string_view!(key);

    safe_optional_as_ref!(read_preference)
        .and_then(ReadPreferenceT::get_tag_sets)
        .and_then(|v| v.get(index))
        .and_then(|v| v.get(key))
        .map_or(StringViewT::default(), StringViewT::from)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_get_tag_sets_get_keys(
    read_preference: *const ReadPreferenceT,
    index: usize,
) -> StringViewArrayT {
    safe_optional_as_ref!(read_preference)
        .and_then(ReadPreferenceT::get_tag_sets)
        .and_then(|v| v.get(index))
        .map(|v| v.keys().map(String::as_ref).collect::<Vec<&str>>())
        .map_or(StringViewArrayT::default(), StringViewArrayT::from)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_get_tag_sets_get_len(
    read_preference: *const ReadPreferenceT,
    index: usize,
) -> usize {
    safe_optional_as_ref!(read_preference)
        .and_then(ReadPreferenceT::get_tag_sets)
        .and_then(|v| v.get(index))
        .map_or(0, HashMap::len)
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_get_tag_sets_len(
    read_preference: *const ReadPreferenceT,
) -> usize {
    safe_optional_as_ref!(read_preference)
        .and_then(ReadPreferenceT::get_tag_sets)
        .map_or(usize::default(), Vec::len)
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

    read_preference.0 = match v.into() {
        ReadPreferenceModeT::Primary => ReadPreference::primary(),
        ReadPreferenceModeT::Secondary => ReadPreference::secondary(read_preference.take_options()),
        ReadPreferenceModeT::PrimaryPreferred => {
            ReadPreference::primary_preferred(read_preference.take_options())
        }
        ReadPreferenceModeT::SecondaryPreferred => {
            ReadPreference::secondary_preferred(read_preference.take_options())
        }
        ReadPreferenceModeT::Nearest => ReadPreference::nearest(read_preference.take_options()),

        ReadPreferenceModeT::Unknown(_) => return, // Do nothing.
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_set_tag_sets_none(read_preference: *mut ReadPreferenceT) {
    if let Some(options) = safe_as_mut!(read_preference)
        .get_options_as_mut()
        .and_then(Option::as_mut)
    {
        options.tag_sets = None;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoac_read_preference_set_tag_sets_push(
    read_preference: *mut ReadPreferenceT,
    keys: StringViewArrayT,
    values: StringViewArrayT,
    error: *mut ErrorT,
) {
    let error = safe_optional_error_as_mut!(error);
    let read_preference = safe_as_mut_with_error!(read_preference, error);

    let keys: Vec<&str> = if keys.ptr.is_null() {
        Vec::new()
    } else {
        safe_string_view_array_with_error!(keys, error)
    };

    let values: Vec<&str> = if values.ptr.is_null() {
        Vec::new()
    } else {
        safe_string_view_array_with_error!(values, error)
    };

    if keys.len() != values.len() {
        crate::private::safety::invalid_argument(
            error,
            format!("length mismatch: {} != {}", keys.len(), values.len()),
        );
        return Default::default();
    }

    let Some(options) = read_preference.get_options_as_mut() else {
        return;
    };

    let len = keys.len();
    let mut tag_set = TagSet::with_capacity(len);

    for (key, value) in keys.into_iter().zip(values) {
        tag_set.insert(key.to_owned(), value.to_owned());
    }

    options
        .get_or_insert_default()
        .tag_sets
        .get_or_insert_default()
        .push(tag_set);
}

impl ReadPreferenceT {
    fn get_max_staleness(&self) -> Option<Duration> {
        self.get_options_as_ref().and_then(|v| v.max_staleness)
    }

    fn get_tag_sets(&self) -> Option<&Vec<TagSet>> {
        self.get_options_as_ref().and_then(|v| v.tag_sets.as_ref())
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

    fn set_max_staleness(&mut self, v: Option<Duration>) {
        if let Some(options) = self.get_options_as_mut() {
            options.get_or_insert_default().max_staleness = v;
        }
    }

    fn take_options(&mut self) -> Option<ReadPreferenceOptions> {
        self.get_options_as_mut().and_then(Option::take)
    }
}

impl From<&ReadPreferenceT> for ReadPreferenceModeT {
    fn from(value: &ReadPreferenceT) -> Self {
        match &value.0 {
            ReadPreference::Primary => ReadPreferenceModeT::Primary,
            ReadPreference::Secondary { .. } => ReadPreferenceModeT::Secondary,
            ReadPreference::PrimaryPreferred { .. } => ReadPreferenceModeT::PrimaryPreferred,
            ReadPreference::SecondaryPreferred { .. } => ReadPreferenceModeT::SecondaryPreferred,
            ReadPreference::Nearest { .. } => ReadPreferenceModeT::Nearest,

            _ => ReadPreferenceModeT::Unknown(MONGOAC_READ_PREFERENCE_MODE_UNKNOWN),
        }
    }
}

impl From<&ReadPreferenceT> for ReadPreference {
    fn from(value: &ReadPreferenceT) -> Self {
        value.0.clone()
    }
}
