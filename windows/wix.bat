@ECHO OFF

SET DIFXLIB="%WIX%\bin\difxapp_%DDK_ARCH%.wixlib"
SET MSIARCH=%DDK_ARCH%
IF "%WIN_BUILD_TYPE%"=="chk" (SET MSIBUILD=_debug) ELSE (SET MSIBUILD=)
SET MSIOS=%DDK_DIST%

SET MSINAME=libvchan-%MSIOS%%MSIARCH%%MSIBUILD%.msm

"%WIX%\bin\candle" installer.wxs -arch %MSIARCH% -ext "%WIX%\bin\WixUIExtension.dll" -ext "%WIX%\bin\WixDifxAppExtension.dll" -ext "%WIX%\bin\WixIIsExtension.dll"
"%WIX%\bin\light.exe" -o %MSINAME% installer.wixobj %DIFXLIB% -ext "%WIX%\bin\WixUIExtension.dll" -ext "%WIX%\bin\WixDifxAppExtension.dll" -ext "%WIX%\bin\WixIIsExtension.dll"
