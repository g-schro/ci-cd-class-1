rem
rem usage: deliver workspace build_id git_commit
rem

setlocal

set "
if not [%3]==[] goto :get_args
echo usage: deliver workspace build-tag
exit /b 1

:get_args
set "workspace=%1"
set "build_id=%2"
set "git_commit=%3"
set "deliver_dir=C:\Users\gene\Documents\Gene\proj\builds\%build_id%"

if not exist "%deliver_dir%\" goto :makedir
echo ERROR: Deliver directory %deliver_dir% already exists
exit /b 1

:makedir
mkdir "%deliver_dir%"
if exist "%deliver_dir%\" goto :write_meta
echo ERROR: Cannot create directory %deliver_dir%
exit /b 1

:write_meta
cd "%deliver_dir%"
echo Jenkins build id: %build_id% >meta.txt
echo Git commit: %git_commit% >>meta.txt
echo Last 5 commits... >>meta.txt
echo[ >>meta.txt
git -C "%workspace%" log --format=medium -5 >>meta.txt

mkdir Debug
copy "%workspace%\Debug\*.bin" Debug
copy "%workspace%\Debug\*.map" Debug
copy "%workspace%\Debug\*.list" Debug

mkdir Release
copy "%workspace%\Release\*.bin" Release
copy "%workspace%\Release\*.map" Release
copy "%workspace%\Release\*.list" Release
