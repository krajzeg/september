@echo off

call compile %1   || goto :error
target\09 %~dpn1.09 || goto :error
goto :end

:error
exit /b %errorlevel%

:end
