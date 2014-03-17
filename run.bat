@echo off

call compile %1   || goto :error
c\09 %~dpn1.09 || goto :error
goto :end

:error
echo "Execution failed."
exit /b %errorlevel%

:end
