rem
rem usage: static-analysis top-level-dir
rem

setlocal

if not [%1]==[] goto :get_args
echo "Insufficient arguments"
exit /b 1

:get_args

set "top_dir=%1"

set "cmd=C:\Program Files\Cppcheck\cppcheck"
set "opt=--error-exitcode=1 --inline-suppr"

"%cmd%" %opt% "%top_dir%"
