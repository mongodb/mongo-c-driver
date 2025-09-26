/*
 * Copyright 2009-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <mongoc/mongoc-version-functions.h>

#include <mongoc/mongoc-version.h>

#ifdef _WIN32
#include <winbase.h>
#include <winnt.h>

#include <windows.h>
#endif

/**
 * mongoc_get_major_version:
 *
 * Helper function to return the runtime major version of the library.
 */
int
mongoc_get_major_version(void)
{
   return MONGOC_MAJOR_VERSION;
}


/**
 * mongoc_get_minor_version:
 *
 * Helper function to return the runtime minor version of the library.
 */
int
mongoc_get_minor_version(void)
{
   return MONGOC_MINOR_VERSION;
}

/**
 * mongoc_get_micro_version:
 *
 * Helper function to return the runtime micro version of the library.
 */
int
mongoc_get_micro_version(void)
{
   return MONGOC_MICRO_VERSION;
}

/**
 * mongoc_get_version:
 *
 * Helper function to return the runtime string version of the library.
 */
const char *
mongoc_get_version(void)
{
   return MONGOC_VERSION_S;
}

/**
 * mongoc_check_version:
 *
 * True if libmongoc's version is greater than or equal to the required
 * version.
 */
bool
mongoc_check_version(int required_major, int required_minor, int required_micro)
{
   return MONGOC_CHECK_VERSION(required_major, required_minor, required_micro);
}

#ifdef _WIN32

typedef NTSTATUS (APIENTRY *RTLVERIFYVERSIONINFO_FN) (PRTL_OSVERSIONINFOEXW VersionInfo, ULONG TypeMask, ULONGLONG ConditionMask);

/**
 * _mongoc_verify_windows_version:
 *
 * True if the Windows version is greater than or equal to the required
 * desktop or server version.
 */
bool
_mongoc_verify_windows_version(int major_version, int minor_version, int build_number, bool strictly_equal)
{
   static RTLVERIFYVERSIONINFO_FN pRtlVerifyVersionInfo;
   OSVERSIONINFOEXW osvi;
   bool matched;
   int op = VER_GREATER_EQUAL;

   if (strictly_equal) {
      op = VER_EQUAL;
   }

   /* Windows version functions may not return the correct version for
   later Windows versions unless the application is so manifested. Try  
   to use the more accurate kernel function RtlVerifyVersionInfo */
   HMODULE hDll = LoadLibrary(TEXT("Ntdll.dll"));
   if (hDll) {
      pRtlVerifyVersionInfo = (RTLVERIFYVERSIONINFO_FN)GetProcAddress(hDll, "RtlVerifyVersionInfo");
   }

   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXW));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
   osvi.dwMajorVersion = major_version;
   osvi.dwMinorVersion = minor_version;

   ULONGLONG mask = 0;
   VER_SET_CONDITION(mask, VER_MAJORVERSION, op);
   VER_SET_CONDITION(mask, VER_MINORVERSION, op);

   if (pRtlVerifyVersionInfo) {
      matched = (pRtlVerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, mask) == 0);
   } else {
      matched = (VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, mask) != 0);
   } 

   // Compare build number separately if major and minor versions are equal
   if (build_number && matched && _mongoc_verify_windows_version(major_version, minor_version, 0, true)) {
      ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXW));
      osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
      osvi.dwBuildNumber = build_number;

      mask = 0;
      VER_SET_CONDITION(mask, VER_BUILDNUMBER, op);

      if (pRtlVerifyVersionInfo) {
         matched = (pRtlVerifyVersionInfo(&osvi, VER_BUILDNUMBER, mask) == 0);
      } 
      else {
         matched = (VerifyVersionInfoW(&osvi, VER_BUILDNUMBER, mask) != 0);
      }
   }

   return matched;
}

#endif
