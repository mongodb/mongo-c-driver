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

use mongodb::bson::{Document, RawDocumentBuf};

pub(crate) fn to_documents(stages: &[RawDocumentBuf]) -> Result<Vec<Document>, ErrorT> {
    let documents = stages
        .iter()
        .map(Document::try_from)
        .collect::<Result<Vec<_>, _>>()?;

    Ok(documents)
}
