rem
rem usage: check-test-results <junit-xml-file>
rem

setlocal

if not [%1]==[] goto :get_args
echo "Missing arguments"
exit /b 1

:get_args

set "file=%1"

findstr /l "<failure type" "%file%"

if %ERRORLEVEL% EQU 0 (exit /b 1) else (exit /b 0)
