rem Validations shared by link-sample-program-msvc-bson.bat and
rem link-sample-program-mingw-bson.bat

echo off

rem Notice that the dll goes in "bin".
set DLL=%INSTALL_DIR%\bin\libbson-1.0.dll
if not exist %DLL% (
  echo %DLL% is missing!
  exit /B 1
) else (
  echo libbson-1.0.dll check ok
)
if not exist %INSTALL_DIR%\lib\pkgconfig\libbson-1.0.pc (
  echo libbson-1.0.pc missing!
  exit /B 1
) else (
  echo libbson-1.0.pc check ok
)
if not exist %INSTALL_DIR%\lib\cmake\libbson-1.0\libbson-1.0-config.cmake (
  echo libbson-1.0-config.cmake missing!
  exit /B 1
) else (
  echo libbson-1.0-config.cmake check ok
)
if not exist %INSTALL_DIR%\lib\cmake\libbson-1.0\libbson-1.0-config-version.cmake (
  echo libbson-1.0-config-version.cmake missing!
  exit /B 1
) else (
  echo libbson-1.0-config-version.cmake check ok
)

if "%LINK_STATIC%"=="1" (
  if not exist %INSTALL_DIR%\lib\pkgconfig\libbson-static-1.0.pc (
    echo libbson-static-1.0.pc missing!
    exit /B 1
  ) else (
    echo libbson-static-1.0.pc check ok
  )
  if not exist %INSTALL_DIR%\lib\cmake\libbson-static-1.0\libbson-static-1.0-config.cmake (
    echo libbson-static-1.0-config.cmake missing!
    exit /B 1
  ) else (
    echo libbson-static-1.0-config.cmake check ok
  )
  if not exist %INSTALL_DIR%\lib\cmake\libbson-static-1.0\libbson-static-1.0-config-version.cmake (
    echo libbson-static-1.0-config-version.cmake missing!
    exit /B 1
  ) else (
    echo libbson-static-1.0-config-version.cmake check ok
  )
) else (
  if exist %INSTALL_DIR%\lib\pkgconfig\libbson-static-1.0.pc (
    echo libbson-static-1.0.pc should not have been installed!
    exit /B 1
  ) else (
    echo libbson-static-1.0.pc missing, as expected
  )
  if exist %INSTALL_DIR%\lib\cmake\libbson-static-1.0\libbson-static-1.0-config.cmake (
    echo libbson-static-1.0-config.cmake should not have been installed!
    exit /B 1
  ) else (
    echo libbson-static-1.0-config.cmake missing, as expected
  )
  if exist %INSTALL_DIR%\lib\cmake\libbson-static-1.0\libbson-static-1.0-config-version.cmake (
    echo libbson-static-1.0-config-version.cmake should not have been installed!
    exit /B 1
  ) else (
    echo libbson-static-1.0-config-version.cmake missing, as expected
  )
)

echo on
