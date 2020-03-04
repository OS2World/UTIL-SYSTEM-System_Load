@echo off

if .%1.==.clean. goto clean

rem Query current date (variable %archdate%).
@%unixroot%\usr\libexec\bin\date +"set archdate=%%Y%%m%%d" >archdate.cmd
call archdate.cmd
del archdate.cmd

set fnSrc=SL-src-%archdate%.zip
set fnWPI=SL-wpi-%archdate%.zip

rem Cleaning.
call make-arch.cmd clean

rem Make archives of sources and binaries.

rem First archive - sources.
echo *** Packing sources: %fnSrc%.
del %fnSrc% 2>nul
7za.exe a -tzip -mx7 -r0 -x!*.zip %fnSrc% .\src make-arch.cmd >nul

echo *** Compiling the project.
cd src\CPUTemp
cmd /c make.cmd make

cd ..\SL
cmd /c make.cmd make

cd ..\..

rem Second archive - WPI.
echo *** Packing WPI: %fnWPI%.
del %fnWPI% 2>nul
7za.exe a -tzip -mx7 -r0 %fnWPI% .\src\CPUTemp\cputemp.wpi .\src\SL\SL.wpi >nul


:l00
EXIT


:clean
echo *** Cleaning.
cd src\CPUTemp
cmd /c make.cmd clean

cd ..\SL
cmd /c make.cmd clean
del SL.wpi 2>nul

cd ..\..
