REM Supported/used environment variables:
REM    CC             Compiler, "mingw" or "Visual Studio 14 2015 Win64".
REM    BSON_ONLY      Whether to build only the BSON library.

rem Ensure Cygwin executables like sh.exe are not in PATH
rem set PATH=C:\Windows\system32;C:\Windows

echo on
echo

set TAR=C:\cygwin\bin\tar
set CMAKE=C:\cmake\bin\cmake
set CMAKE_MAKE_PROGRAM=C:\mingw-w64\x86_64-4.9.1-posix-seh-rt_v3-rev1\mingw64\bin\mingw32-make.exe
rem Ensure Cygwin executables like sh.exe are not in PATH
set PATH=C:\cygwin\bin;C:\Windows\system32;C:\Windows;C:\mingw-w64\x86_64-4.9.1-posix-seh-rt_v3-rev1\mingw64\bin;C:\mongoc;src\libbson;src\libmongoc

if "%BSON_ONLY%"=="1" (
   set BUILD_DIR=%CD%\build-dir-bson
   set INSTALL_DIR=%CD%\install-dir-bson
) else (
   set BUILD_DIR=%CD%\build-dir-mongoc
   set INSTALL_DIR=%CD%\install-dir-mongoc
)
rmdir /S /Q %BUILD_DIR%
mkdir %BUILD_DIR%

rmdir /S /Q %INSTALL_DIR%
mkdir %INSTALL_DIR%

set PATH=%PATH%;"c:\Program Files (x86)\MSBuild\14.0\Bin"
set PATH=%PATH%;"c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin"
set PATH=%PATH%;%INSTALL_DIR%\bin

cd %BUILD_DIR%
%TAR% xf ..\..\mongoc.tar.gz -C . --strip-components=1
if errorlevel 1 (
   exit /B 1
)

if "%BSON_ONLY%"=="1" (
  set BSON_ONLY_OPTION=-DENABLE_MONGOC=OFF
) else (
  set BSON_ONLY_OPTION=-DENABLE_MONGOC=ON
)

echo.%CC%| findstr /I "gcc">Nul && (
  rem Build libmongoc, with flags that the downstream R driver mongolite uses
  %CMAKE% -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=%CMAKE_MAKE_PROGRAM% -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% -DCMAKE_CFLAGS="-std=c99 -pedantic" -DCMAKE_PREFIX_PATH=%INSTALL_DIR%\lib\cmake %BSON_ONLY_OPTION% .
  %CMAKE_MAKE_PROGRAM%
  if errorlevel 1 (
     exit /B 1
  )
  %CMAKE_MAKE_PROGRAM% install
  if errorlevel 1 (
     exit /B 1
  )

  echo > %INSTALL_DIR%\lib\canary.txt

  dir %INSTALL_DIR%\share\mongo-c-driver

  %CMAKE_MAKE_PROGRAM% uninstall
  if errorlevel 1 (
     exit /B 1
  )
) || (
  %CMAKE% -G "%CC%" "-DCMAKE_INSTALL_PREFIX=%INSTALL_DIR%" "-DCMAKE_BUILD_TYPE=Debug" %BSON_ONLY_OPTION% .
  MSBuild.exe /m ALL_BUILD.vcxproj
  if errorlevel 1 (
     exit /B 1
  )
  MSBuild.exe /m INSTALL.vcxproj
  if errorlevel 1 (
     exit /B 1
  )

  echo > %INSTALL_DIR%\lib\canary.txt

  dir %INSTALL_DIR%\share\mongo-c-driver

  MSBuild.exe /m generate_uninstall\uninstall.vcxproj
  if errorlevel 1 (
     exit /B 1
  )
)

if exist %INSTALL_DIR%\lib\pkgconfig\libbson-1.0.pc (
   echo libbson-1.0.pc found!
   exit /B 1
) else (
   echo libbson-1.0.pc check ok
)
if exist %INSTALL_DIR%\lib\cmake\libbson-1.0\libbson-1.0-config.cmake (
   echo libbson-1.0-config.cmake found!
   exit /B 1
) else (
   echo libbson-1.0-config.cmake check ok
)
if exist %INSTALL_DIR%\lib\cmake\libbson-1.0\libbson-1.0-config-version.cmake (
   echo libbson-1.0-config-version.cmake found!
   exit /B 1
) else (
   echo libbson-1.0-config-version.cmake check ok
)
if not exist %INSTALL_DIR%\lib\canary.txt (
   echo canary.txt not found!
   exit /B 1
) else (
   echo canary.txt check ok
)
if not exist %INSTALL_DIR%\lib (
   echo %INSTALL_DIR%\lib not found!
   exit /B 1
) else (
   echo %INSTALL_DIR%\lib check ok
)
if "%BSON_ONLY%" NEQ "1" (
   if exist %INSTALL_DIR%\lib\pkgconfig\libmongoc-1.0.pc (
     echo libmongoc-1.0.pc found!
     exit /B 1
   ) else (
     echo libmongoc-1.0.pc check ok
   )
   if exist %INSTALL_DIR%\lib\cmake\libmongoc-1.0\libmongoc-1.0-config.cmake (
     echo libmongoc-1.0-config.cmake found!
     exit /B 1
   ) else (
     echo libmongoc-1.0-config.cmake check ok
   )
   if exist %INSTALL_DIR%\lib\cmake\libmongoc-1.0\libmongoc-1.0-config-version.cmake (
     echo libmongoc-1.0-config-version.cmake found!
     exit /B 1
   ) else (
     echo libmongoc-1.0-config-version.cmake check ok
   )
)
if exist %INSTALL_DIR%\include\libbson-1.0\bson\bson.h (
   echo bson\bson.h found!
   exit /B 1
) else (
   echo bson\bson.h check ok
)
if exist %INSTALL_DIR%\include\libbson-1.0\bson.h (
   echo bson.h found!
   exit /B 1
) else (
   echo bson.h check ok
)
if exist %INSTALL_DIR%\include\libbson-1.0 (
   echo $INSTALL_DIR\include\libbson-1.0 found!
   exit /B 1
) else (
   echo $INSTALL_DIR\include\libbson-1.0 check ok
)
if "%BSON_ONLY%" NEQ "1" (
   if exist %INSTALL_DIR%\include\libmongoc-1.0\mongoc\mongoc.h (
     echo mongoc\mongoc.h found!
     exit /B 1
   ) else (
     echo mongoc\mongoc.h check ok
   )
   if exist %INSTALL_DIR%\include\libmongoc-1.0\mongoc.h (
     echo mongoc.h found!
     exit /B 1
   ) else (
     echo mongoc.h check ok
   )
   if exist %INSTALL_DIR%\include\libmongoc-1.0 (
     echo $INSTALL_DIR\include\libmongoc-1.0 found!
     exit /B 1
   ) else (
     echo $INSTALL_DIR\include\libmongoc-1.0 check ok
   )
)
if exist %INSTALL_DIR%\share\mongo-c-driver\uninstall-bson.cmd (
   echo uninstall-bson.cmd found!
   exit /B 1
) else (
   echo uninstall-bson.cmd check ok
)
if exist %INSTALL_DIR%\share\mongo-c-driver\uninstall.cmd (
   echo uninstall.cmd found!
   exit /B 1
) else (
   echo uninstall.cmd check ok
)
if exist %INSTALL_DIR%\share\mongo-c-driver\uninstall-bson.sh (
   echo uninstall-bson.sh found!
   exit /B 1
) else (
   echo uninstall-bson.sh check ok
)
if exist %INSTALL_DIR%\share\mongo-c-driver\uninstall.sh (
   echo uninstall.sh found!
   exit /B 1
) else (
   echo uninstall.sh check ok
)
if exist %INSTALL_DIR%\share\mongo-c-driver (
   echo $INSTALL_DIR\share\mongo-c-driver found!
   exit /B 1
) else (
   echo $INSTALL_DIR\share\mongo-c-driver check ok
)
