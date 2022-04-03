rem
rem usage: build build-dir {Debug|Release} {all|clean}
rem

setlocal

if not [%3]==[] goto :get_args
echo "Insufficient arguments"
exit /b 1

:get_args

set "build_dir=%1"
set "build_type=%2"
set "target=%3"

cd "%build_dir%"

set PATH=C:\ST\STM32CubeIDE_1.6.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.9-2020-q2-update.win32_2.0.0.202105311346\tools\bin;C:\ST\STM32CubeIDE_1.6.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.0.0.202105311346\tools\bin;C:/ST/STM32CubeIDE_1.6.0/STM32CubeIDE//plugins/com.st.stm32cube.ide.jre.win64_2.0.100.202110150814/jre/bin/client;C:/ST/STM32CubeIDE_1.6.0/STM32CubeIDE//plugins/com.st.stm32cube.ide.jre.win64_2.0.100.202110150814/jre/bin;C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;C:\WINDOWS\System32\WindowsPowerShell\v1.0\;C:\usr\local\wbin;C:\usr\local\emacs-23.4\bin;C:\Program Files\Intel\WiFi\bin\;C:\Program Files\Common Files\Intel\WirelessCommon\;C:\WINDOWS\System32\OpenSSH\;C:\Program Files (x86)\Intel\Intel(R) Management Engine Components\DAL;C:\Program Files\Intel\Intel(R) Management Engine Components\DAL;C:\Program Files\Git\cmd;C:\WINDOWS\system32\config\systemprofile\AppData\Roaming\Python\Scripts;C:\Program Files\PuTTY\;C:\usr\local\ffmpeg-2021-02-23-git-78d5e1c653-essentials_build\ffmpeg-2021-02-23-git-78d5e1c653-essentials_build\bin;;C:\Program Files\Microsoft VS Code\bin;C:\Users\gene\AppData\Local\Microsoft\WindowsApps;;C:\ST\STM32CubeIDE_1.6.0\STM32CubeIDE

set compiler_prefix=arm-none-eabi-

set "version_file=..\App\gpio-app\version.h"

if [%BUILD_TAG%]==[] goto :do_make

echo #ifndef _VERSION_H_ >"%version_file%"
echo #define _VERSION_H_ >>"%version_file%"
echo #define VERSION "%BUILD_TAG%-%build_type%" >>"%version_file%"
echo #endif >>"%version_file%"

:do_make

make -j4 "%target%"
