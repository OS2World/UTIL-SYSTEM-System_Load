@echo off

if .%1. == .make. goto make
if .%1. == .clean. goto clean
goto help

:make
set rctk=1
if .%2. == .rctk. goto rctk
set rctk=0
:rctk
set make_sw=-h -s
set actInf=Make:
if exist bin goto start
md bin
goto start

:clean
set make_sw=-h -s clean
set actInf=Clean:

:start
if .%WATCOM%. == .. goto needow

rem	Make binaries
rem	=============

set plugins=cpu drives net os4irq process traffic
rem set plugins=%plugins% sample1 sample2 sample3
set utils=cpuid getver rxgetver sysstate cputemp

echo %actInf% main program.

cd .\sl
wmake %make_sw% rctk=%rctk%

echo %actInf% plugins.

cd ..\plugins
for %%i in (%plugins%) do echo [ plugin: %%i ] & cd %%i & wmake %make_sw% rctk=%rctk% & cd ..

echo %actInf% utilites.

cd ..\utils
for %%i in (%utils%) do echo [ %%i ] & cd %%i & wmake %make_sw% rctk=%rctk% & cd ..

cd ..
EXIT


:help
echo System Load - MAKE utility.
echo Syntax:
echo   MAKE.CMD make [rctk]
echo   MAKE.CMD clean
echo where:
echo   make     Compile everything and put binaries in .\bin.
echo   rctk     Use Resource Compiler from IBM OS/2 Developer's Toolkit,
echo            it necessary for Watcom below 2.0.
echo   clean    Clean project (files in .\bin will not be deleted).
echo.
exit

:needow
echo Open Watcom must be installed on your system.
exit
