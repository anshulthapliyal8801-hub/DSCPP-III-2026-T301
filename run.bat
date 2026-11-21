@echo off
rem ====================================================================
rem  MUNSphere build and run script (Windows, MinGW-w64 GCC/G++)
rem
rem    run.bat            build, then start the server on port 8080
rem    run.bat run --port 9090    build, then start with options
rem    run.bat admin      build, then create an Admin account (first setup)
rem    run.bat demo       build, fill demo-data\ with a demo conference, start on it
rem    run.bat build      build only
rem    run.bat test       build and run all C and C++ tests
rem    run.bat clean      delete the build folder
rem
rem  Every Shared\*_dsa.c, Module*\*_dsa.c and *.cpp is found automatically, so
rem  new modules need no changes to this script.
rem ====================================================================
setlocal EnableDelayedExpansion
cd /d "%~dp0"

set "MODE=%~1"
if "%MODE%"=="" set "MODE=run"

if /i "%MODE%"=="clean" goto do_clean

where gcc >nul 2>nul
if errorlevel 1 (
  echo gcc was not found. Install MinGW-w64 and add its bin folder to PATH.
  exit /b 1
)
where g++ >nul 2>nul
if errorlevel 1 (
  echo g++ was not found. Install MinGW-w64 and add its bin folder to PATH.
  exit /b 1
)

set "INCLUDES=-IShared"
for /d %%D in (Module*) do set "INCLUDES=!INCLUDES! -I%%D"
set "DEFS=-D__USE_MINGW_ANSI_STDIO=1"
set "CFLAGS=-std=c11 -Wall -Wextra -O2 !DEFS! !INCLUDES!"
set "CXXFLAGS=-std=c++17 -Wall -Wextra -O2 !DEFS! !INCLUDES!"
set "LIBS=-lws2_32 -lbcrypt -static"

if /i "%MODE%"=="build" goto do_build
if /i "%MODE%"=="test" goto do_test
if /i "%MODE%"=="run" goto do_run
if /i "%MODE%"=="admin" goto do_admin
if /i "%MODE%"=="demo" goto do_demo
echo Unknown command "%MODE%". Use: run.bat [run^|admin^|demo^|build^|test^|clean]
exit /b 1

:do_build
call :build
exit /b %errorlevel%

:do_run
call :build
if errorlevel 1 exit /b 1
echo.
echo Starting MUNSphere. Open http://127.0.0.1:8080/ in your browser.
echo.
build\munsphere.exe %2 %3 %4 %5 %6 %7
exit /b %errorlevel%

:do_admin
call :build
if errorlevel 1 exit /b 1
echo.
build\munsphere.exe --create-admin %2 %3
exit /b %errorlevel%

:do_demo
call :build
if errorlevel 1 exit /b 1
rem Demo data lives in its own folder so it never mixes with real data.
if not exist demo-data\accounts.txt (
  build\munsphere.exe --data demo-data --seed-demo
  if errorlevel 1 exit /b 1
) else (
  echo demo-data already exists. Delete the demo-data folder to start the demo afresh.
)
echo.
echo Starting MUNSphere on the demo data. Open http://127.0.0.1:8080/ in your browser.
build\munsphere.exe --data demo-data %2 %3 %4 %5
exit /b %errorlevel%

:do_test
call :build
if errorlevel 1 exit /b 1
set "FAILED=0"
for %%F in (tests\test_*.c) do (
  echo.
  echo === Building %%F
  gcc !CFLAGS! "%%F" !OBJS! -o "build\%%~nF.exe"
  if errorlevel 1 (set "FAILED=1") else (
    "build\%%~nF.exe"
    if errorlevel 1 set "FAILED=1"
  )
)
for %%F in (tests\test_*.cpp) do (
  echo.
  echo === Building %%F
  g++ !CXXFLAGS! "%%F" !CXXOBJS! !OBJS! -o "build\%%~nF.exe" !LIBS!
  if errorlevel 1 (set "FAILED=1") else (
    "build\%%~nF.exe"
    if errorlevel 1 set "FAILED=1"
  )
)
echo.
if "!FAILED!"=="1" (
  echo SOME TESTS FAILED.
  exit /b 1
)
echo ALL TESTS PASSED.
exit /b 0

:do_clean
if exist build rmdir /s /q build
echo Build folder removed.
exit /b 0

rem --------------------------------------------------------------------
rem :build  compiles each C and C++ file to an object once, then links the
rem server. Sets OBJS (C objects) and CXXOBJS (C++ objects except main),
rem which the test step links against without recompiling.
rem --------------------------------------------------------------------
:build
if not exist build mkdir build
set "OBJS="
for %%F in (Shared\*_dsa.c) do (
  echo   C    %%F
  gcc !CFLAGS! -c "%%F" -o "build\%%~nF.o"
  if errorlevel 1 (
    echo Compiling %%F failed.
    exit /b 1
  )
  set "OBJS=!OBJS! build\%%~nF.o"
)
for /d %%D in (Module*) do (
  for %%F in ("%%D\*_dsa.c") do (
    echo   C    %%~F
    gcc !CFLAGS! -c "%%~F" -o "build\%%~nF.o"
    if errorlevel 1 (
      echo Compiling %%~F failed.
      exit /b 1
    )
    set "OBJS=!OBJS! build\%%~nF.o"
  )
)
if not exist build\obj mkdir build\obj
set "CXXOBJS="
for %%F in (Shared\*.cpp) do (
  echo   C++  %%F
  g++ !CXXFLAGS! -c "%%F" -o "build\obj\%%~nF.o"
  if errorlevel 1 (
    echo Compiling %%F failed.
    exit /b 1
  )
  set "CXXOBJS=!CXXOBJS! build\obj\%%~nF.o"
)
for /d %%D in (Module*) do (
  for %%F in ("%%D\*.cpp") do (
    if /i not "%%~nxF"=="main.cpp" (
      echo   C++  %%~F
      g++ !CXXFLAGS! -c "%%~F" -o "build\obj\%%~nF.o"
      if errorlevel 1 (
        echo Compiling %%~F failed.
        exit /b 1
      )
      set "CXXOBJS=!CXXOBJS! build\obj\%%~nF.o"
    )
  )
)
echo   C++  linking build\munsphere.exe
g++ !CXXFLAGS! Module1_Conference\main.cpp !CXXOBJS! !OBJS! -o build\munsphere.exe !LIBS!
if errorlevel 1 (
  echo Linking failed.
  exit /b 1
)
echo   Built build\munsphere.exe
exit /b 0
