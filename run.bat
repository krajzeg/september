@echo off

call compile %1    || goto :error
bin\09 %~dpn1.sept || goto :error
goto :end

:error
exit /b %errorlevel%

:end
