@echo off
setlocal enableextensions

set SOLUTION=vs2022\core-vchan-xen.sln

if "%1" == "" (
  echo Usage: %0 Release^|Debug
  exit /b 1
)

set CONFIGURATION=%1

if not defined EWDK_PATH (
  echo EWDK_PATH not defined
  exit /b 1
)

:: strip quotes
set EWDK_PATH=%EWDK_PATH:"=%

set VS="%EWDK_PATH%\Program Files\Microsoft Visual Studio\2022\BuildTools"
if not exist %VS% (
  echo Can't find Visual Studio in %VS%
  exit /b 1
)

set VS=%VS:"=%

set MSBUILD_ROOT="%vs%\MSBuild"
set MSBUILD_ROOT=%MSBUILD_ROOT:"=%

set MSBUILD="%MSBUILD_ROOT%\Current\Bin\MSBuild.exe"

if not exist %MSBUILD% (
  echo Can't find MSBuild in %MSBUILD%
  exit /b 1
)

:: needed for building without normal visual studio
call "%EWDK_PATH%\BuildEnv\SetupBuildEnv.cmd" x86_amd64

set EWDK_INCLUDE="%EWDK_PATH%\Program Files\Windows Kits\10\Include\%Version_Number%\shared;%EWDK_PATH%\Program Files\Windows Kits\10\Include\%Version_Number%\um;%EWDK_PATH%\Program Files\Windows Kits\10\Include\%Version_Number%\ucrt"
set EWDK_INCLUDE=%EWDK_INCLUDE:"=%

set EWDK_LIB="%EWDK_PATH%\Program Files\Windows Kits\10\Lib\%Version_Number%\um\x64;%EWDK_PATH%\Program Files\Windows Kits\10\Lib\%Version_Number%\ucrt\x64"
set EWDK_LIB=%EWDK_LIB:"=%

%MSBUILD% %SOLUTION% -t:Rebuild -p:Platform=x64 -p:Configuration=%CONFIGURATION%
