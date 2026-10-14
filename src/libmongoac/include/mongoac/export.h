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

#ifndef MONGOAC_EXPORT_H
#define MONGOAC_EXPORT_H

// Avoid -Wempty-translation-unit warnings.
#include <stddef.h>

#ifdef MONGOAC_STATIC
#define MONGOAC_ABI_EXPORT
#elif defined(_WIN32)
#define MONGOAC_ABI_EXPORT __declspec(dllimport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define MONGOAC_ABI_EXPORT __attribute__((visibility("default")))
#else
#define MONGOAC_ABI_EXPORT
#endif

#endif // MONGOAC_EXPORT_H
