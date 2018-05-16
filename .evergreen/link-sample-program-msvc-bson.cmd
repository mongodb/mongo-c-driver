REM Supported/used environment variables:
REM   LINK_STATIC              Whether to statically link to libbson

rem Ensure Cygwin executables like sh.exe are not in PATH
rem set PATH=C:\Windows\system32;C:\Windows

echo on
echo

set TAR=C:\cygwin\bin\tar
set CMAKE=C:\cmake\bin\cmake

set SRCROOT=%CD%
set BUILD_DIR=%CD%\build-dir
rmdir /S /Q %BUILD_DIR%
mkdir %BUILD_DIR%

set INSTALL_DIR=%CD%\install-dir
rmdir /S /Q %INSTALL_DIR%
mkdir %INSTALL_DIR%

set PATH=%PATH%;"c:\Program Files (x86)\MSBuild\14.0\Bin"
set PATH=%PATH%;"c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin"
set PATH=%PATH%;%INSTALL_DIR%\bin

cd %BUILD_DIR%
%TAR% xf ..\..\mongoc.tar.gz -C . --strip-components=1

if "%LINK_STATIC%"=="1" (
  %CMAKE% -G "Visual Studio 14 2015 Win64" -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% -DENABLE_TESTS=OFF -DENABLE_BSON=ON .
) else (
  %CMAKE% -G "Visual Studio 14 2015 Win64" -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% -DENABLE_TESTS=OFF -DENABLE_BSON=ON -DENABLE_STATIC=OFF .
)

msbuild.exe /m ALL_BUILD.vcxproj
msbuild.exe INSTALL.vcxproj

call ..\.evergreen\check-installed-files-bson.bat
if errorlevel 1 (
   exit /B %errorlevel%
)

rem Test our CMake package config file with CMake's find_package command.
set EXAMPLE_DIR=%SRCROOT%\src\libbson\examples\cmake\find_package

if "%LINK_STATIC%"=="1" (
  set EXAMPLE_DIR="%EXAMPLE_DIR%_static"
)

cd %EXAMPLE_DIR%
%CMAKE% -G "Visual Studio 14 2015 Win64" -DCMAKE_PREFIX_PATH=%INSTALL_DIR%\lib\cmake .
msbuild.exe ALL_BUILD.vcxproj

rem Yes, they should've named it "dependencies".
dumpbin.exe /dependents Debug\hello_bson.exe

Debug\hello_bson.exe
