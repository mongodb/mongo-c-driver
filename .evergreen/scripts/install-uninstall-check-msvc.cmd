REM Supported/used environment variables:
REM    CC             Compiler, "mingw" or "Visual Studio 14 2015 Win64".
REM    BSON_ONLY      Whether to build only the BSON library.

rem Ensure Cygwin executables like sh.exe are not in PATH
rem set PATH=C:\Windows\system32;C:\Windows

rem Load environment for Visual Studio 15 2017.
rem https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-150
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars64.bat" || goto :error

echo on
echo

set SRCROOT=%CD%
set TAR=C:\cygwin\bin\tar

set version=1.31.0
set major=1

if "%BSON_ONLY%"=="1" (
   set BUILD_DIR=%CD%\build-dir-bson
   set INSTALL_DIR=%CD%\install-dir-bson
) else (
   set BUILD_DIR=%CD%\build-dir-mongoc
   set INSTALL_DIR=%CD%\install-dir-mongoc
)
rmdir /S /Q %BUILD_DIR% 2>nul || true
mkdir %BUILD_DIR% || goto :error

rmdir /S /Q %INSTALL_DIR% 2>nul || true
mkdir %INSTALL_DIR% || goto :error

set PATH=%PATH%;%INSTALL_DIR%\bin

cd %BUILD_DIR% || goto :error

if "%BSON_ONLY%"=="1" (
  set BSON_ONLY_OPTION=-DENABLE_MONGOC=OFF
) else (
  set BSON_ONLY_OPTION=-DENABLE_MONGOC=ON
)

%CMAKE% -G "Visual Studio 15 2017" -A x64 "-DCMAKE_INSTALL_PREFIX=%INSTALL_DIR%" "-DCMAKE_BUILD_TYPE=Debug" %BSON_ONLY_OPTION% .. || goto :error
%CMAKE% --build . --config Debug || goto :error
%CMAKE% --build . --config Debug --target install || goto :error

echo > %INSTALL_DIR%\lib\canary.txt || goto :error

REM no kms-message components should be installed
if exist %INSTALL_DIR%\include\kms_message\kms_message.h (
   echo kms_message.h found!
   goto :error
) else (
   echo kms_message.h check ok
)
if exist %INSTALL_DIR%\lib\libkms_message-static.a (
   echo libkms_message-static.a found!
   goto :error
) else (
   echo libkms_message-static.a check ok
)
if exist %INSTALL_DIR%\lib\cmake\kms_message\kms_message-config.cmake (
   echo kms_message-config.cmake found!
   goto :error
) else (
   echo kms_message-config.cmake check ok
)

dir %INSTALL_DIR%\share\mongo-c-driver || goto :error

rem CDRIVER-6062: uninstall script may return non-zero exit code despite success.
%CMAKE% --build . --target uninstall || true

if exist %INSTALL_DIR%\lib\pkgconfig\bson%major%.pc (
   echo bson%major%.pc found!
   goto :error
) else (
   echo bson%major%.pc check ok
)
if not exist %INSTALL_DIR%\lib\canary.txt (
   echo canary.txt not found!
   goto :error
) else (
   echo canary.txt check ok
)
if not exist %INSTALL_DIR%\lib (
   echo %INSTALL_DIR%\lib not found!
   goto :error
) else (
   echo %INSTALL_DIR%\lib check ok
)
if "%BSON_ONLY%" NEQ "1" (
   if exist %INSTALL_DIR%\lib\pkgconfig\mongoc%major%.pc (
     echo mongoc%major%.pc found!
     goto :error
   ) else (
     echo mongoc%major%.pc check ok
   )
   if exist %INSTALL_DIR%\lib\cmake\mongoc-1.0\mongoc-1.0-config.cmake (
     echo mongoc-1.0-config.cmake found!
     goto :error
   ) else (
     echo mongoc-1.0-config.cmake check ok
   )
   if exist %INSTALL_DIR%\lib\cmake\mongoc-1.0\mongoc-1.0-config-version.cmake (
     echo mongoc-1.0-config-version.cmake found!
     goto :error
   ) else (
     echo mongoc-1.0-config-version.cmake check ok
   )
   if exist %INSTALL_DIR%\lib\cmake\mongoc-1.0\mongoc-targets.cmake (
     echo mongoc-targets.cmake found!
     goto :error
   ) else (
     echo mongoc-targets.cmake check ok
   )
)
if exist %INSTALL_DIR%\include\bson-%version%\bson\bson.h (
   echo bson\bson.h found!
   goto :error
) else (
   echo bson\bson.h check ok
)
if exist %INSTALL_DIR%\include\bson-%version% (
   echo %INSTALL_DIR%\include\bson-%version% found!
   goto :error
) else (
   echo %INSTALL_DIR%\include\bson-%version% check ok
)
if "%BSON_ONLY%" NEQ "1" (
   if exist %INSTALL_DIR%\include\libmongoc-1.0\mongoc\mongoc.h (
     echo mongoc\mongoc.h found!
     goto :error
   ) else (
     echo mongoc\mongoc.h check ok
   )
   if exist %INSTALL_DIR%\include\libmongoc-1.0\mongoc.h (
     echo mongoc.h found!
     goto :error
   ) else (
     echo mongoc.h check ok
   )
   if exist %INSTALL_DIR%\include\libmongoc-1.0 (
     echo %INSTALL_DIR%\include\libmongoc-1.0 found!
     goto :error
   ) else (
     echo %INSTALL_DIR%\include\libmongoc-1.0 check ok
   )
)
if exist %INSTALL_DIR%\share\mongo-c-driver\uninstall-bson.cmd (
   echo uninstall-bson.cmd found!
   goto :error
) else (
   echo uninstall-bson.cmd check ok
)
if exist %INSTALL_DIR%\share\mongo-c-driver\uninstall.cmd (
   echo uninstall.cmd found!
   goto :error
) else (
   echo uninstall.cmd check ok
)
if exist %INSTALL_DIR%\share\mongo-c-driver\uninstall-bson.sh (
   echo uninstall-bson.sh found!
   goto :error
) else (
   echo uninstall-bson.sh check ok
)
if exist %INSTALL_DIR%\share\mongo-c-driver\uninstall.sh (
   echo uninstall.sh found!
   goto :error
) else (
   echo uninstall.sh check ok
)
if exist %INSTALL_DIR%\share\mongo-c-driver (
   echo %INSTALL_DIR%\share\mongo-c-driver found!
   goto :error
) else (
   echo %INSTALL_DIR%\share\mongo-c-driver check ok
)

goto :EOF
:error
exit /B %errorlevel%
