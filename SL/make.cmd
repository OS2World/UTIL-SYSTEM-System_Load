@echo off
rem
rem  This file performs a complete compilation of the project and the creation
rem  of the installation package.
rem  Run "make.cmd clean" to delete files created at compile time and WPI file.
rem

set make_sw=-h -z
if .%1. == .clean. set make_sw=-h clean & set makecmd_sw=clean

cd src
wmake %make_sw%
if errorlevel 1 goto _ExitErr

cd plugins
call make.cmd %makecmd_sw%
cd ..

cd utilites
call make.cmd %makecmd_sw%
cd ..\..

cd warpin
call make.cmd %makecmd_sw%
cd ..

EXIT

:_ExitErr
EXIT 1
