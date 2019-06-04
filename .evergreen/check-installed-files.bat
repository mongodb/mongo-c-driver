rem Validations shared by link-sample-program-msvc.bat and
rem link-sample-program-mingw.bat

echo off

rem Notice that the dll goes in "bin".
set DLL=%INSTALL_DIR%\bin\libmongoc-1.0.dll
if not exist %DLL% (
  echo %DLL% is missing!
  exit /B 1
) else (
  echo libmongoc-1.0.dll check ok
)
if not exist %INSTALL_DIR%\lib\pkgconfig\libmongoc-1.0.pc (
  echo libmongoc-1.0.pc missing!
  exit /B 1
) else (
  echo libmongoc-1.0.pc check ok
)
if not exist %INSTALL_DIR%\lib\cmake\mongoc-1.0\mongoc-1.0-config.cmake (
  echo mongoc-1.0-config.cmake missing!
  exit /B 1
) else (
  echo mongoc-1.0-config.cmake check ok
)
if not exist %INSTALL_DIR%\lib\cmake\mongoc-1.0\mongoc-1.0-config-version.cmake (
  echo mongoc-1.0-config-version.cmake missing!
  exit /B 1
) else (
  echo mongoc-1.0-config-version.cmake check ok
)
if not exist %INSTALL_DIR%\lib\cmake\mongoc-1.0\mongoc-targets.cmake (
  echo mongoc-targets.cmake missing!
  exit /B 1
) else (
  echo mongoc-targets.cmake check ok
)
if not exist %INSTALL_DIR%\lib\pkgconfig\libmongoc-static-1.0.pc (
  echo libmongoc-static-1.0.pc missing!
  exit /B 1
) else (
  echo libmongoc-static-1.0.pc check ok
)

echo on
