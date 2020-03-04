@echo off
rem
rem  This file will build all the SystemLoad utilities.
rem  Run "make.cmd clean" to delete files created at compile time.
rem

set list=cpuid getver rxgetver sysstate

set make_sw=-h -z
if .%1. == .clean. set make_sw=-h clean

for %%n in (%list%) do echo Utility: %%n & cd %%n & wmake %make_sw% & cd ..
