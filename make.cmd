@echo off

if .%1. == .make. goto make
if .%1. == .clean. goto clean
goto help

:make
set make_sw=-h
set actInf=Make:
if exist bin goto start
md bin
goto start

:clean
set make_sw=-h clean
set actInf=Clean:

:start
if .%WATCOM%. == .. goto needow

rem	Make binaries
rem	=============

set plugins=cpu drives net os4irq process
rem set plugins=%plugins% sample1 sample2 sample3
set utils=cpuid getver rxgetver sysstate

echo %actInf% main program.

cd .\sl
wmake %make_sw%

echo %actInf% plugins.

cd ..\plugins
for %%i in (%plugins%) do echo [ plugin: %%i ] & cd %%i & wmake %make_sw% & cd ..

echo %actInf% utilites.

cd ..\utils
for %%i in (%utils%) do echo [ %%i ] & cd %%i & wmake %make_sw% & cd ..

cd ..
EXIT


:help
echo.
echo Compile everything and put binaries in .\bin:
echo   make.cmd make
echo.
echo Clean project (files in .\bin will not be deleted):
echo   make.cmd clean
exit

:needow
echo Open Watcom must be installed on your system.
exit
