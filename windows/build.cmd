for %%p in ("%~dp0\..\..") do set QUBES_REPO=%%~fp\artifacts
set BUILD_CFG=%1
if "%BUILD_CFG%" == "" set BUILD_CFG=Release
powershell %QUBES_BUILDER%\qubesbuilder\plugins\build_windows\scripts\local\build.ps1 %~dp0\.. %QUBES_REPO% %BUILD_CFG%
