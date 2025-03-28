rem Validations shared by link-sample-program-msvc-bson.bat and
rem link-sample-program-mingw-bson.bat

echo off

rem Notice that the dll goes in "bin".
set DLL=%INSTALL_DIR%\bin\bson-1.0.dll
set major=1
set LIB_DLL=%INSTALL_DIR%\bin\libbson%major%.dll
set LIB_LIB=%INSTALL_DIR%\lib\libbson%major%.lib
if "%MINGW%"=="1" (
  if not exist %LIB_DLL% (
    echo %LIB_DLL% is missing!
    exit /B 1
  ) else (
    echo %LIB_DLL% check ok
  )
  if exist %DLL% (
    echo %DLL% is present and should not be!
    exit /B 1
  )
) else (
  if not exist %DLL% (
    echo %DLL% is missing!
    exit /B 1
  ) else (
    echo %DLL% check ok
  )
  if exist %LIB_LIB% (
    echo %LIB_LIB% is present and should not be!
    exit /B 1
  ) else (
    echo %LIB_LIB% check ok
  )
)
if not exist %INSTALL_DIR%\lib\pkgconfig\bson%major%.pc (
  echo bson%major%.pc missing!
  exit /B 1
) else (
  echo bson%major%.pc check ok
)
if not exist %INSTALL_DIR%\lib\cmake\bson-1.0\bson-1.0-config.cmake (
  echo bson-1.0-config.cmake missing!
  exit /B 1
) else (
  echo bson-1.0-config.cmake check ok
)
if not exist %INSTALL_DIR%\lib\cmake\bson-1.0\bson-1.0-config-version.cmake (
  echo bson-1.0-config-version.cmake missing!
  exit /B 1
) else (
  echo bson-1.0-config-version.cmake check ok
)
if not exist %INSTALL_DIR%\lib\cmake\bson-1.0\bson-targets.cmake (
  echo bson-targets.cmake missing!
  exit /B 1
) else (
  echo bson-targets.cmake check ok
)

if "%LINK_STATIC%"=="1" (
  if not exist %INSTALL_DIR%\lib\pkgconfig\bson%major%-static.pc (
    echo bson%major%-static.pc missing!
    exit /B 1
  ) else (
    echo bson%major%-static.pc check ok
  )
) else (
  if exist %INSTALL_DIR%\lib\pkgconfig\bson%major%-static.pc (
    echo bson%major%-static.pc should not have been installed!
    exit /B 1
  ) else (
    echo bson%major%-static.pc missing, as expected
  )
)

echo on
