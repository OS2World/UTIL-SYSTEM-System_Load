/*
	Building CPUTemp installation package.
*/

/* WPI file name */
WPIFile = "..\cputemp"

/*
   List of installation package files.

             PCK No   Path              File name
*/
call addFile   101,   "..\bin",         "cputemp.dll"
call addFile   102,   "..\bin",         "rdmsr.sys"
call addFile   102,   ".",              "drvins.cmd"
call addFile   1,     "..\bin",         "cputemp.exe"
call addFile   2,     "..\bin",         "cput.dll"


fileScriptInput = "script.inp"
fileScriptOutput = "script.out"

/* Query WarpIn path.
 *
 * Old method: warpinPath = "%osdir%\install\WARPIN"
 */
warpinPath = strip( SysIni( "USER", "WarpIN", "Path" ), "T", "00"x )
if warpinPath = "ERROR:" | warpinPath = "" then
do
  say "WarpIN is not installed correctly"
  exit 1
end

/* We make the script directory current. */
parse source os cmd scriptPath
savePath = directory()
scriptPath = left( scriptPath, lastpos( "\", scriptPath ) - 1 )
call directory scriptPath


if RxFuncQuery('SysLoadFuncs') then
do
  call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
  call SysLoadFuncs
end


parse arg sw
if sw = "clean" then
do
  fullname = stream( WPIFile || ".wpi", "c", "query exists" )
  if fullname \= "" then
    call SysFileDelete fullname
  call exitScript 0
end


do idx = 1 to InsFiles.0
  fullname = stream( InsFiles.idx._path || "\" || InsFiles.idx._name, ,
                     "c", "query exists" )
  if fullname = "" then
  do
    say "File " || InsFiles.idx._path || "\" || InsFiles.idx._name || ,
        " does not exist."
    say "Was the project compiled?"
    call exitScript 1
  end
end

/* Substitution versions of the components (from BLDLEVEL signatures) in   */
/* the script. Reads file fileScriptInput and makes file fileScriptOutput  */
/* Replaces all switches "<!-- BL:D:\path\program.exe -->" with version of */
/* D:\path\program.exe (bldlevel signature uses).                          */
if makeScript( fileScriptInput, fileScriptOutput ) = 0 then
  call exitScript 2


/* Building installation package. */

call SysFileDelete WPIFile || ".wpi"

cmd = "@"warpinPath || "\WIC.EXE " || WPIFile || " -a"
do idx = 1 to InsFiles.0
  cmd = cmd || " " || InsFiles.idx._pckno || " -c" || InsFiles.idx._path || ,
        " " || InsFiles.idx._name
end
cmd = cmd || " -s " || fileScriptOutput

"@set beginlibpath=" || warpinPath
cmd


call SysFileDelete fileScriptOutput

/* Check that the installation file is created. */
fileHwExpWPI = stream( WPIFile || ".wpi", "c", "query exists" )
if fileHwExpWPI = "" then
  call exitScript 3

say "WPI file created: " || fileHwExpWPI

/* Success, rc = 0 */
call exitScript 0


addFile:
  if symbol( "InsFiles.0" ) \= "VAR"
    then __idx = 1
    else __idx = InsFiles.0 + 1

  parse arg InsFiles.__idx._pckno, InsFiles.__idx._path, InsFiles.__idx._name
  InsFiles.0 = __idx
return

makeScript: PROCEDURE
  fileInput = arg(1)
  fileOutput = arg(2)

/*  if RxFuncQuery( "gvLoadFuncs" ) then*/
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

exitScript: PROCEDURE expose savePath
  call directory savePath
  exit arg( 1 )
