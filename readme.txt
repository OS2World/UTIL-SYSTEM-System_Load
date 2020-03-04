
  Compiling SystemLoad and CPUTemp
  ================================

Necessary tools:

* Open Watcom C

  When creating the project, version 2.0 was used. Probably other versions can
  also be used. However, a resource compiler from earlier versions will not
  work. A Resource Compiler from OS/2 Developer's Toolkit can be used.

* IBM OS/2 Developer's Toolkit

* IBM Developer Connection Device Driver Kit for OS/2

* XWorkplace sources

  The XWP and XWP helpers header files are used to compile the processor
  temperature widget. If you are not going to build the CPUTemp package, you
  will not need it.

* WarpIN installer

  If you are using an eComStation or ArcaOS operating system, this software is
  already installed.


Open the global.mif file in the text editor and edit the "User settings"
section. Follow the comments in the global.mif file. This file is included
from all the makefile files of the project so you don't need to configure
anything else.

After that, to compile and create the installation packages for SL and CPUTemp,
you can run the make.cmd files from the corresponding subdirectories.

NOTE  To delete all files created as a result of compilation, execute make.cmd
      with the command line switch "clean" (without quotes).


You can compile any parts of the project by running the wmake utility in any
subdirectory containing the makefile. To compile a debug version of any
component, open the corresponding makefile in the text editor and uncomment the
line "DEBUGCODE = yes". At run time, debug versions of executable files and
dynamic libraries will create .dbg files with debug messages.

Please note that when switching between debugging and regular compilation
(enabling or disabling the DEBUGCODE option) you need to clean the component
with the "wmake clean" command.


Author
------

Donations are most welcome!
PayPal: digi@os2.snc.ru
https://www.arcanoae.com/shop/os2-ports-and-applications-by-andrey-vasilkin/

Andrey Vasilkin, 2017-2020
E-mail: digi@os2.snc.ru
http://os2.snc.ru/
