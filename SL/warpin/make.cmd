/*
	Installation package for WARPIN.
*/

WPIName = "..\sl"
fileScriptInput = "script.inp"
fileScriptOutput = "script.out"


if RxFuncQuery('SysLoadFuncs') then
do
  call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
  call SysLoadFuncs
end

/* Substitution versions of the components (from BLDLEVEL signatures) in   */
/* the script. Reads file fileScriptInput and makes file fileScriptOutput  */
/* Replaces all switches "<!-- BL:D:\path\program.exe -->" with version of */
/* D:\path\program.exe (bldlevel signature uses).                          */
if makeScript( fileScriptInput, fileScriptOutput ) = 0 then
  exit

/* Building installation package. */

call SysFileDelete WPIName || ".wpi"

"set beginlibpath=%osdir%\install\WARPIN"

"%osdir%\install\WARPIN\WIC.EXE " || WPIName || " -a " || ,
"1 License.txt 1 readme.txt 1 slfld.ico 1 slfld_o.ico " || ,
"1 -c..\bin sl.exe " || ,
"100 -c..\bin cpu.dll cpu.hlp " || ,
"110 -c..\bin drives.dll " || ,
"120 -c..\bin net.dll " || ,
"130 -c..\bin os4irq.dll " || ,
"140 -c..\bin process.dll " || ,
"150 -c..\bin traffic.dll traffic.hlp " || ,
"160 -c..\bin sysinfo.dll sysinfo.hlp " || ,
"2 -c..\bin cpuid.exe getver.exe rxgetver.dll rxgetver.cmd sysstate.exe cputemp.exe " || ,
"-s " || fileScriptOutput

call SysFileDelete fileScriptOutput
EXIT

makeScript: PROCEDURE
  fileInput = arg(1)
  fileOutput = arg(2)

  if RxFuncQuery( "gvLoadFuncs" ) then
  do
    call RxFuncAdd "gvLoadFuncs", "RXGETVER", "gvLoadFuncs"
    call gvLoadFuncs
  end

  if stream( fileInput, "c", "open read" ) \= "READY:" then
  do
    say "Cannot open input file: " || fileInput
    return 0
  end

  call SysFileDelete fileOutput
  if stream( fileOutput, "c", "open write" ) \= "READY:" then
  do
    say "Cannot open output file: " || fileOutput
    return 0
  end

  resOk = 1
  do while lines( fileInput ) \= 0
    parse value linein( fileInput ) with part1 "<!-- BL:" file " -->" part2

    if file \= "" then
    do
      rc = gvGetFromFile( file, "ver." )
      if rc \= "OK" then
      do
        say "File: " || file
        say rc
        resOk = 0
        leave
      end

      parse value ver._BL_REVISION || ".0" with ver.1 "." ver.2 "." ver.3 "." v
      verIns = ver.1 || "\" || ver.2 || "\" || ver.3
    end
    else
      verIns = ""

    call lineout fileOutput, part1 || verIns || part2
  end

  call stream fileInput, "c", "close"
  call stream fileOutput, "c", "close"
  call gvDropFuncs

  if \resOk then
    call SysFileDelete fileOutput
    
  return resOk
