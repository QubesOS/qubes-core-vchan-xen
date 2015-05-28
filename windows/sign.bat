@ECHO OFF

IF NOT EXIST SIGN_CONFIG.BAT GOTO DONT_SIGN

for /R %%f in (*.dll) do %SIGNTOOL% sign /v %CERT_CROSS_CERT_FLAG% /f %CERT_FILENAME% %CERT_PASSWORD_FLAG% /t http://timestamp.verisign.com/scripts/timestamp.dll %%f

:DONT_SIGN
