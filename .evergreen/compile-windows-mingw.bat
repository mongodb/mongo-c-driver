rem Ensure Cygwin executables like sh.exe are not in PATH
set PATH=C:\Windows\system32;C:\Windows;C:\mingw-w64\x86_64-4.9.1-posix-seh-rt_v3-rev1\mingw64\bin;C:\mongoc;src\libbson;src\libmongoc

echo CONFIGURE_FLAGS %CONFIGURE_FLAGS%

set CMAKE=C:\cmake\bin\cmake
set CMAKE_MAKE_PROGRAM=C:\mingw-w64\x86_64-4.9.1-posix-seh-rt_v3-rev1\mingw64\bin\mingw32-make.exe
set CC=C:\mingw-w64\x86_64-4.9.1-posix-seh-rt_v3-rev1\mingw64\bin\gcc.exe

%CMAKE% -G "MinGW Makefiles" -DCMAKE_MAKE_PROGRAM=%CMAKE_MAKE_PROGRAM% -DCMAKE_PREFIX_PATH=%INSTALL_DIR%\lib\cmake -DCMAKE_C_FLAGS="-std=c99 -pedantic" %CONFIGURE_FLAGS%

%CMAKE_MAKE_PROGRAM%

set MONGOC_TEST_SKIP_LIVE=on
.\src\libmongoc\test-libmongoc.exe --no-fork -d -F test-results.json
