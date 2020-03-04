@echo off
rem
rem  This file will build all the SystemLoad plugins.
rem  Run "make.cmd clean" to delete files created at compile time.
rem

set list=cpu drives net os4irq process sysinfo traffic

set make_sw=-h -z
if .%1. == .clean. set make_sw=-h clean

for %%n in (%list%) do echo Plugin: %%n & cd %%n & wmake %make_sw% & cd ..
