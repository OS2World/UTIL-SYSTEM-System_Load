@echo off

if .%1. == .make-wpi. goto make
if .%1. == .make. goto make
if .%1. == .clean. goto clean
goto help

:make
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

set components=cputemp rdmsr cput

echo %actInf% CPU temperature library.

for %%i in (%components%) do echo [ component: %%i ] & cd %%i & wmake %make_sw% & cd ..

if .%1. == .make-wpi. goto makeWPI
EXIT

rem	Make WarpIn package
rem	===================

:makeWPI
cd warpin
cmd /c make.cmd
cd..
cmd /c make.cmd clean
echo.
if exist cputemp.wpi goto WPICreatedMsg
echo WarpIn package was NOT created.
EXIT

:WPICreatedMsg
echo WarpIn package cputemp.wpi created.
EXIT



:help
echo CPU temperature library - MAKE utility.
echo Syntax:
echo   MAKE.CMD make
echo   MAKE.CMD clean
echo   MAKE.CMD make-wpi
echo where:
echo   make       Compile everything and put binaries in .\bin.
echo   clean      Clean project.
echo   make-wpi   Compile everything, create WarpIn package, clean project.
echo.
exit

:needow
echo Open Watcom must be installed on your system.
exit
